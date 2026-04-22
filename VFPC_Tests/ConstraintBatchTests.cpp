// ConstraintBatchTests.cpp
// Fixture-driven batch coverage for combined constraint evaluation.
//
// Cases are loaded at runtime from:
//   VFPC_Tests/fixtures/constraint_cases.json
//
// The fixture path is resolved relative to this source file using the
// compile-time __FILE__ macro, so the test runs correctly regardless of
// the working directory when VFPC_Tests.exe is invoked.
//
// To add a new test case: edit constraint_cases.json and rerun the exe.
// No recompile needed.
//
// PARALLELISATION NOTE
// --------------------
// evaluateConstraint() is a pure function — no shared state, no side effects.
// The main loop collects (bool passed) for each case into a vector before any
// GTest reporting, so evaluation and reporting are cleanly separated.
//
// To run the evaluation phase in parallel when the fixture dataset grows:
//   #include <execution>
//   std::transform(std::execution::par_unseq,  // <-- add this argument
//                  cases.begin(), cases.end(), results.begin(), evalFn);
// The reporting loop is sequential (GTest's ADD_FAILURE is not thread-safe).

#include <gtest/gtest.h>

#include <algorithm>
#include <fstream>
#include <optional>
#include <string>
#include <vector>

#include "ConstraintChecks.hpp"
#include "TimeWindow.h"
#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"

using namespace ConstraintChecks;

// ---------------------------------------------------------------------------
// Fixture path — resolved from source file location at compile time.
// ---------------------------------------------------------------------------

static std::string fixtureDir()
{
    std::string f = __FILE__;
    for (auto& c : f) { if (c == '\\') c = '/'; }
    auto slash = f.rfind('/');
    return (slash == std::string::npos ? "./" : f.substr(0, slash + 1)) + "fixtures/";
}

static const std::string FIXTURE_FILE = fixtureDir() + "constraint_cases.json";

// ---------------------------------------------------------------------------
// Simulated time (optional per case).
// ---------------------------------------------------------------------------

struct SimTime {
    int day  = 0;   // Monday=0 … Sunday=6
    int hour = 0;
    int min  = 0;
};

// ---------------------------------------------------------------------------
// Combined evaluator.
//
// Semantics when a time window is present:
//   • window INACTIVE → constraint does not apply → return true
//   • window ACTIVE   → evaluate all field checks normally
//
// This mirrors the plugin: a constraint's time window governs whether it
// applies at all; only active constraints are then checked against the flight.
// ---------------------------------------------------------------------------

static bool evaluateConstraint(
    const rapidjson::Value&          c,
    const std::string&               destination,
    const std::vector<std::string>&  route,
    const std::vector<std::string>&  points,
    int                              rfl,
    const std::optional<SimTime>&    simtime,
    const rapidjson::Value*          timewindow)   // nullable
{
    if (timewindow && simtime.has_value()) {
        bool active = checkTimeWindow(
            simtime->day, simtime->hour, simtime->min, *timewindow);
        if (!active)
            return true;   // constraint not applicable right now
    }

    bool warn = false;
    return checkOneDestination(c, destination)
        && checkOneExitPoint(c, points)
        && checkOneRoute(c, route)
        && checkOneMinMax(c, rfl)
        && checkOneDirection(c, rfl)
        && checkOneAlerts(c, warn);
}

// ---------------------------------------------------------------------------
// Case structure loaded from JSON.
// ---------------------------------------------------------------------------

struct BatchCase {
    std::string              id;
    std::string              note;
    rapidjson::Document      constraint;
    std::string              destination;
    std::vector<std::string> route;
    std::vector<std::string> points;
    int                      rfl      = 0;
    bool                     expected = false;

    // Optional time-window fields.
    std::optional<SimTime>   simtime;
    rapidjson::Document      timewindow;   // may be empty (IsNull check required)
    bool                     hasTimewindow = false;
};

// ---------------------------------------------------------------------------
// Loader.
// ---------------------------------------------------------------------------

static std::vector<BatchCase> loadCases(std::string& errOut)
{
    std::vector<BatchCase> cases;

    std::ifstream ifs(FIXTURE_FILE);
    if (!ifs.is_open()) {
        errOut = "Cannot open fixture file: " + FIXTURE_FILE;
        return cases;
    }

    rapidjson::IStreamWrapper isw(ifs);
    rapidjson::Document       doc;
    doc.ParseStream(isw);

    if (doc.HasParseError()) {
        errOut = "JSON parse error in fixture file (offset "
               + std::to_string(doc.GetErrorOffset()) + ")";
        return cases;
    }
    if (!doc.HasMember("cases") || !doc["cases"].IsArray()) {
        errOut = "Fixture file has no 'cases' array";
        return cases;
    }

    const auto& arr = doc["cases"];
    for (rapidjson::SizeType i = 0; i < arr.Size(); i++) {
        const auto& entry = arr[i];
        BatchCase   bc;

        bc.id   = entry.HasMember("id")   ? entry["id"].GetString()   : "(unnamed)";
        bc.note = entry.HasMember("note") ? entry["note"].GetString() : "";

        if (!entry.HasMember("constraint")) continue;
        bc.constraint.CopyFrom(entry["constraint"], bc.constraint.GetAllocator());

        if (entry.HasMember("timewindow") && entry["timewindow"].IsObject()) {
            bc.timewindow.CopyFrom(entry["timewindow"], bc.timewindow.GetAllocator());
            bc.hasTimewindow = true;
        }

        if (entry.HasMember("simtime") && entry["simtime"].IsObject()) {
            const auto& st = entry["simtime"];
            SimTime s;
            if (st.HasMember("day")  && st["day"].IsInt())  s.day  = st["day"].GetInt();
            if (st.HasMember("hour") && st["hour"].IsInt()) s.hour = st["hour"].GetInt();
            if (st.HasMember("min")  && st["min"].IsInt())  s.min  = st["min"].GetInt();
            bc.simtime = s;
        }

        if (!entry.HasMember("flight")) continue;
        const auto& fl = entry["flight"];

        if (fl.HasMember("destination") && fl["destination"].IsString())
            bc.destination = fl["destination"].GetString();

        if (fl.HasMember("rfl") && fl["rfl"].IsInt())
            bc.rfl = fl["rfl"].GetInt();

        if (fl.HasMember("route") && fl["route"].IsArray())
            for (rapidjson::SizeType j = 0; j < fl["route"].Size(); j++)
                bc.route.push_back(fl["route"][j].GetString());

        if (fl.HasMember("points") && fl["points"].IsArray())
            for (rapidjson::SizeType j = 0; j < fl["points"].Size(); j++)
                bc.points.push_back(fl["points"][j].GetString());

        if (entry.HasMember("expected") && entry["expected"].IsBool())
            bc.expected = entry["expected"].GetBool();

        cases.push_back(std::move(bc));
    }

    return cases;
}

// ---------------------------------------------------------------------------
// Evaluation helper (pure — no side effects; safe to call in parallel).
// ---------------------------------------------------------------------------

static bool runCase(const BatchCase& bc)
{
    const rapidjson::Value* tw =
        bc.hasTimewindow ? static_cast<const rapidjson::Value*>(&bc.timewindow)
                         : nullptr;
    return evaluateConstraint(
        bc.constraint, bc.destination, bc.route, bc.points, bc.rfl,
        bc.simtime, tw);
}

// ---------------------------------------------------------------------------
// Tests.
// ---------------------------------------------------------------------------

TEST(ConstraintBatch, FixtureFileLoads)
{
    std::string err;
    auto cases = loadCases(err);
    ASSERT_TRUE(err.empty()) << err;
    ASSERT_GT(cases.size(), 0u)
        << "No cases loaded — fixture file may be empty: " << FIXTURE_FILE;

    size_t timed = 0;
    for (const auto& c : cases) { if (c.hasTimewindow) ++timed; }

    std::cout << "  Loaded " << cases.size() << " cases ("
              << timed << " time-windowed) from:\n"
              << "  " << FIXTURE_FILE << "\n";
}

TEST(ConstraintBatch, AllCasesPass)
{
    std::string err;
    auto cases = loadCases(err);
    ASSERT_TRUE(err.empty()) << "Fixture load failed: " << err;

    // ── Evaluate phase (pure transform; parallelise here when dataset grows) ──
    std::vector<bool> results(cases.size());
    std::transform(cases.begin(), cases.end(), results.begin(),
        [](const BatchCase& bc) { return runCase(bc); });

    // ── Reporting phase (sequential — GTest is not thread-safe) ──────────────
    int passed = 0;
    int failed = 0;
    for (size_t i = 0; i < cases.size(); ++i) {
        const bool got = results[i];
        if (got == cases[i].expected) {
            ++passed;
        } else {
            ADD_FAILURE()
                << "\n  CASE   : " << cases[i].id
                << "\n  NOTE   : " << cases[i].note
                << "\n  DEST   : " << cases[i].destination
                << "  RFL=" << cases[i].rfl
                << (cases[i].hasTimewindow
                    ? (cases[i].simtime
                        ? "  TIMED day=" + std::to_string(cases[i].simtime->day)
                          + " " + std::to_string(cases[i].simtime->hour)
                          + ":" + std::to_string(cases[i].simtime->min)
                        : "  TIMED (no simtime)")
                    : "")
                << "\n  EXPECT : " << (cases[i].expected ? "PASS" : "FAIL")
                << "\n  GOT    : " << (got               ? "PASS" : "FAIL");
            ++failed;
        }
    }

    std::cout << "  Batch result: " << passed << " passed";
    if (failed) std::cout << ", " << failed << " FAILED";
    std::cout << " (total " << (passed + failed) << ")\n";
}
