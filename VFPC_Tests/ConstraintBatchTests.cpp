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

#include <gtest/gtest.h>

#include <fstream>
#include <string>
#include <vector>

#include "ConstraintChecks.hpp"
#include "rapidjson/document.h"
#include "rapidjson/istreamwrapper.h"

using namespace ConstraintChecks;

// ---------------------------------------------------------------------------
// Fixture path — resolved from source file location at compile time.
// ---------------------------------------------------------------------------

static std::string fixtureDir()
{
    // __FILE__ expands to the absolute path of this .cpp file at compile time.
    std::string f = __FILE__;
    // Normalise all separators to '/' for consistent splitting.
    for (auto& c : f) { if (c == '\\') c = '/'; }
    auto slash = f.rfind('/');
    return (slash == std::string::npos ? "./" : f.substr(0, slash + 1)) + "fixtures/";
}

static const std::string FIXTURE_FILE = fixtureDir() + "constraint_cases.json";

// ---------------------------------------------------------------------------
// Combined evaluator — mirrors the plugin's per-constraint check sequence.
// ---------------------------------------------------------------------------

static bool evaluateConstraint(
    const rapidjson::Value& c,
    const std::string&              destination,
    const std::vector<std::string>& route,
    const std::vector<std::string>& points,
    int                             rfl)
{
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
    std::string             id;
    std::string             note;
    rapidjson::Document     constraint;
    std::string             destination;
    std::vector<std::string> route;
    std::vector<std::string> points;
    int                     rfl      = 0;
    bool                    expected = false;
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
// Tests.
// ---------------------------------------------------------------------------

TEST(ConstraintBatch, FixtureFileLoads)
{
    std::string err;
    auto cases = loadCases(err);
    ASSERT_TRUE(err.empty()) << err;
    ASSERT_GT(cases.size(), 0u)
        << "No cases loaded — fixture file may be empty: " << FIXTURE_FILE;
    // Print count so it shows up in verbose output.
    std::cout << "  Loaded " << cases.size() << " fixture cases from:\n"
              << "  " << FIXTURE_FILE << "\n";
}

TEST(ConstraintBatch, AllCasesPass)
{
    std::string err;
    auto cases = loadCases(err);
    ASSERT_TRUE(err.empty()) << "Fixture load failed: " << err;

    int passed = 0;
    int failed = 0;

    for (auto& bc : cases) {
        bool result = evaluateConstraint(
            bc.constraint, bc.destination, bc.route, bc.points, bc.rfl);

        if (result == bc.expected) {
            ++passed;
        } else {
            ADD_FAILURE()
                << "\n  CASE   : " << bc.id
                << "\n  NOTE   : " << bc.note
                << "\n  DEST   : " << bc.destination
                << "  RFL=" << bc.rfl
                << "\n  EXPECT : " << (bc.expected ? "PASS" : "FAIL")
                << "\n  GOT    : " << (result      ? "PASS" : "FAIL");
            ++failed;
        }
    }

    // Summary line even when everything passes.
    std::cout << "  Batch result: " << passed << " passed";
    if (failed) std::cout << ", " << failed << " FAILED";
    std::cout << " (total " << (passed + failed) << ")\n";
}
