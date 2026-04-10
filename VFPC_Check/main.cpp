// VFPC_Check — standalone route checker for VFPC-next
//
// PURPOSE
// Lets Peter (or any developer) verify whether a flight plan would pass the
// VFPC plugin's route validation, without needing EuroScope or a Sweatbox
// session.  The tool loads a real out.json, accepts a flight-plan description
// as a JSON file (or from stdin), runs the same ValidatorCore path the plugin
// uses, and prints a human-readable result.
//
// USAGE
//   vfpc_check.exe out.json flightplan.json
//   vfpc_check.exe out.json -           (reads flight plan from stdin)
//   vfpc_check.exe --help
//
// FLIGHT PLAN JSON FORMAT
//   {
//     "callsign":      "BAW123",
//     "origin":        "EGLL",
//     "destination":   "EIDW",
//     "route":         "UMLAT1F UMLAT DCT GIBSO N601 BAGSO DCT STU",
//     "route_points":  ["UMLAT", "N601", "BAGSO", "STU"],
//     "sid":           "UMLAT1F",
//     "aircraft_type": "B738",
//     "engine_type":   "J",
//     "rfl":           36000,
//     "is_ifr":        true,
//     "sim_day":       0,
//     "sim_hour":      10,
//     "sim_min":       30
//   }
//
//   sid_suffix and first_waypoint are derived automatically from sid using the
//   same logic as the live plugin (ParseFlightPlanRow_ in analyzeFP.cpp):
//     - Strip '#' from sid
//     - EGLL + sid=="CHK" → first_waypoint="CPT", sid_suffix="CHK"
//     - Otherwise: first_waypoint = letters before the first digit of sid;
//                  sid_suffix = last character of sid
//   Supply first_waypoint/sid_suffix explicitly in JSON only to override.
//
//   route_points is the raw tokenised route (space-split of route, with speed/
//   level groups, DCT tokens etc still present — same as what the plugin reads
//   from EuroScope).  Omit it to auto-tokenise from route.
//   sim_day uses Monday=0 convention.  Omit sim_* to default to Monday 12:00 UTC.
//
// SYNTAX ADVISORY
//   A route syntax error (SYN) does not cause a hard FAIL by itself.  The plugin
//   sets VF_SYNTAX = "Invalid Syntax" (not "ERR") and continues evaluating the
//   remaining constraint rounds (with rounds 1 and 2 as no-ops).  vfpc_check
//   matches this behaviour: PASS/FAIL is determined by the constraint rounds;
//   SYN is reported separately.
//
// MISSING ROUTE / MISSING DESTINATION
//   These are hard ERR failures in the plugin.  FinalizeValidationResult_ in
//   analyzeFP.cpp calls FieldIndicatesFailure_() on every field; "ERR" always
//   returns true regardless of the constraint round outcome.  vfpc_check mirrors
//   this: missing "route" or missing "destination" are reported as ERR and exit 1
//   immediately, before calling ValidatorCore.  "origin", "destination", and
//   "route" are therefore all required fields.
//   Note: "route_points" alone is NOT accepted as a substitute for "route".
//   EuroScope always provides the raw route string whenever it provides waypoints;
//   accepting route_points-only would give a PASS for input the plugin rejects.
//
// VFR HANDLING
//   The live plugin short-circuits non-IFR plans to "Not Checked" and returns
//   without running any constraint rounds.  If is_ifr is false (or omitted, since
//   the default is true), vfpc_check prints NOT CHECKED (VFR) and exits 0 —
//   matching the plugin's no-check outcome.

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/istreamwrapper.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/stringbuffer.h"

#include "FlightPlanRow.hpp"
#include "SidLookup.hpp"
#include "ValidatorCore.hpp"

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static void printHelp(const char* argv0)
{
    std::cout <<
        "VFPC_Check — offline VFPC route validator\n"
        "\n"
        "Usage:\n"
        "  " << argv0 << " out.json flightplan.json\n"
        "  " << argv0 << " out.json -           (read flight plan from stdin)\n"
        "  " << argv0 << " --help\n"
        "\n"
        "out.json      Real VFPC out.json (bare airport array or {cycle,airports:[...]} envelope).\n"
        "flightplan    JSON object describing the flight.  See header comment for fields.\n"
        "\n"
        "Required fields: origin, destination, and route.\n"
        "  route_points is optional (auto-tokenised from route if absent).\n"
        "  Missing origin/destination/route is a hard ERR (exit 1) — matching plugin.\n"
        "\n"
        "Exit codes:\n"
        "  0  PASS — all 7 constraint rounds completed (SYN advisory may still be printed)\n"
        "  0  NOT CHECKED — flight plan is VFR; plugin would not evaluate it\n"
        "  1  FAIL — ERR field, constraint gate, or SID-level restriction\n"
        "  2  ERROR — bad arguments, unreadable files, or JSON parse failure\n"
        "\n"
        "Notes:\n"
        "  SYN advisory: a route syntax error does not itself cause exit 1.\n"
        "  Missing route/destination: these ARE hard ERR failures (exit 1).\n"
        "  VFR: is_ifr=false exits 0 with NOT CHECKED — matching the plugin.\n";
}

static std::string slurpStream(std::istream& in)
{
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

static bool parseDocument(const std::string& text, const std::string& label,
                           rapidjson::Document& doc)
{
    doc.Parse(text.c_str());
    if (doc.HasParseError()) {
        std::cerr << "ERROR: " << label << " — JSON parse error at offset "
                  << doc.GetErrorOffset() << ": "
                  << rapidjson::GetParseError_En(doc.GetParseError()) << "\n";
        return false;
    }
    return true;
}

// Read a string member; returns empty string if absent.
static std::string strMember(const rapidjson::Value& obj, const char* key)
{
    auto it = obj.FindMember(key);
    if (it == obj.MemberEnd() || !it->value.IsString()) return {};
    return it->value.GetString();
}

static int intMember(const rapidjson::Value& obj, const char* key, int def = 0)
{
    auto it = obj.FindMember(key);
    if (it == obj.MemberEnd() || !it->value.IsInt()) return def;
    return it->value.GetInt();
}

static bool boolMember(const rapidjson::Value& obj, const char* key, bool def = false)
{
    auto it = obj.FindMember(key);
    if (it == obj.MemberEnd() || !it->value.IsBool()) return def;
    return it->value.GetBool();
}

// Check whether a JSON object has a non-empty string member for the given key.
static bool hasStrMember(const rapidjson::Value& obj, const char* key)
{
    auto it = obj.FindMember(key);
    return it != obj.MemberEnd() && it->value.IsString() && it->value.GetStringLength() > 0;
}

static std::string toUpper(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
    return s;
}

// ---------------------------------------------------------------------------
// normaliseSidFields
//
// Mirrors ParseFlightPlanRow_ in analyzeFP.cpp.
//
// After reading raw JSON fields:
//   1. Uppercase origin, destination, sid, aircraft_type, engine_type.
//      Uppercase each token in route_points.
//   2. Strip '#' from sid.
//   3. Derive first_waypoint and sid_suffix from sid unless the caller
//      explicitly supplied them in the JSON (hasExplicitFirst / hasExplicitSuffix).
//
// The EGLL/CHK special case is included verbatim from analyzeFP.cpp line 2014.
// ---------------------------------------------------------------------------
static void normaliseSidFields(FlightPlanRow& row,
                                bool hasExplicitFirst,
                                bool hasExplicitSuffix)
{
    // 1. Uppercase string fields.
    row.origin        = toUpper(row.origin);
    row.destination   = toUpper(row.destination);
    row.sid           = toUpper(row.sid);
    row.aircraft_type = toUpper(row.aircraft_type);
    row.engine_type   = toUpper(row.engine_type);
    if (hasExplicitFirst)  row.first_waypoint = toUpper(row.first_waypoint);
    if (hasExplicitSuffix) row.sid_suffix      = toUpper(row.sid_suffix);
    for (auto& tok : row.route_points) tok = toUpper(tok);

    // 2. Strip '#' from sid (EuroScope sometimes appends it).
    row.sid.erase(std::remove(row.sid.begin(), row.sid.end(), '#'), row.sid.end());

    // 3. Derive first_waypoint / sid_suffix from sid when not explicitly given.
    if (!row.sid.empty() && (!hasExplicitFirst || !hasExplicitSuffix)) {
        if (row.origin == "EGLL" && row.sid == "CHK") {
            if (!hasExplicitFirst)  row.first_waypoint = "CPT";
            if (!hasExplicitSuffix) row.sid_suffix      = "CHK";
        } else {
            if (!hasExplicitFirst) {
                // first_waypoint = leading alpha characters before the first digit.
                const auto digit_pos = row.sid.find_first_of("0123456789");
                row.first_waypoint = (digit_pos != std::string::npos)
                    ? row.sid.substr(0, digit_pos)
                    : row.sid;
            }
            if (!hasExplicitSuffix && !row.sid.empty()) {
                row.sid_suffix = std::string(1, row.sid.back());
            }
        }
    }
}

// ---------------------------------------------------------------------------
// main
// ---------------------------------------------------------------------------

int main(int argc, char** argv)
{
    if (argc == 2 && std::strcmp(argv[1], "--help") == 0) {
        printHelp(argv[0]);
        return 0;
    }

    if (argc < 3) {
        std::cerr << "ERROR: expected two arguments — out.json and flightplan.json\n"
                  << "Run with --help for usage.\n";
        return 2;
    }

    const std::string outJsonPath   = argv[1];
    const std::string fpJsonPath    = argv[2];

    // ---- Load out.json ----
    std::ifstream outFile(outJsonPath);
    if (!outFile.is_open()) {
        std::cerr << "ERROR: cannot open out.json: " << outJsonPath << "\n";
        return 2;
    }
    rapidjson::Document outDoc;
    {
        const std::string text = slurpStream(outFile);
        if (!parseDocument(text, outJsonPath, outDoc)) return 2;
    }

    // ---- Load flight plan ----
    std::string fpText;
    if (fpJsonPath == "-") {
        fpText = slurpStream(std::cin);
    } else {
        std::ifstream fpFile(fpJsonPath);
        if (!fpFile.is_open()) {
            std::cerr << "ERROR: cannot open flight plan: " << fpJsonPath << "\n";
            return 2;
        }
        fpText = slurpStream(fpFile);
    }
    rapidjson::Document fpDoc;
    if (!parseDocument(fpText, fpJsonPath, fpDoc)) return 2;
    if (!fpDoc.IsObject()) {
        std::cerr << "ERROR: flight plan JSON must be an object\n";
        return 2;
    }

    // ---- Build FlightPlanRow ----
    // Track which optional SID-derived fields were explicitly supplied so that
    // normaliseSidFields() only overwrites the ones the caller left absent.
    const bool hasExplicitFirst  = hasStrMember(fpDoc, "first_waypoint");
    const bool hasExplicitSuffix = hasStrMember(fpDoc, "sid_suffix");

    FlightPlanRow row;
    row.callsign      = strMember(fpDoc, "callsign");
    row.origin        = strMember(fpDoc, "origin");
    row.destination   = strMember(fpDoc, "destination");
    row.route         = strMember(fpDoc, "route");
    row.sid           = strMember(fpDoc, "sid");
    row.sid_suffix    = strMember(fpDoc, "sid_suffix");
    row.first_waypoint= strMember(fpDoc, "first_waypoint");
    row.aircraft_type = strMember(fpDoc, "aircraft_type");
    row.engine_type   = strMember(fpDoc, "engine_type");
    row.rfl           = intMember(fpDoc, "rfl");
    row.is_ifr        = boolMember(fpDoc, "is_ifr", true);

    // route_points: array of strings
    {
        auto it = fpDoc.FindMember("route_points");
        if (it != fpDoc.MemberEnd() && it->value.IsArray()) {
            for (const auto& v : it->value.GetArray()) {
                if (v.IsString()) row.route_points.push_back(v.GetString());
            }
        } else if (!row.route.empty()) {
            // Auto-tokenise from route string if route_points not supplied.
            std::istringstream ss(row.route);
            std::string tok;
            while (ss >> tok) row.route_points.push_back(tok);
        }
    }

    const int simDay  = intMember(fpDoc, "sim_day",  0);   // Monday=0 default
    const int simHour = intMember(fpDoc, "sim_hour", 12);
    const int simMin  = intMember(fpDoc, "sim_min",  0);

    // ---- Required-field validation — mirrors plugin ERR gates ----
    // origin: needed to locate the airport in out.json (exit 2 — tool error).
    // destination and route/route_points: plugin sets VF_*=ERR and
    // FinalizeValidationResult_ treats ERR as a hard failure regardless of
    // constraint rounds.  We mirror that with exit 1 here.
    if (row.origin.empty()) {
        std::cerr << "ERROR: flight plan missing \"origin\"\n"; return 2;
    }

    // ---- Normalise inputs to match plugin behaviour ----
    // Uppercases fields, strips '#' from sid, derives first_waypoint and
    // sid_suffix from sid (mirrors ParseFlightPlanRow_ in analyzeFP.cpp).
    normaliseSidFields(row, hasExplicitFirst, hasExplicitSuffix);

    // Destination and route checks come after normalisation so that the
    // printed header shows the uppercased values.
    const bool have_route_text   = !row.route.empty();
    const bool have_route_points = !row.route_points.empty();

    // ---- Print flight plan header ----
    // Emitted before all short-circuit exits so Peter always sees what was
    // being checked.
    auto printHeader = [&]() {
        std::cout << "\n=== VFPC Route Check ===\n";
        std::cout << "Callsign:    " << (row.callsign.empty() ? "(none)" : row.callsign) << "\n";
        std::cout << "Origin:      " << row.origin << "\n";
        std::cout << "Destination: " << (row.destination.empty() ? "(none)" : row.destination) << "\n";
        std::cout << "Route:       " << row.route << "\n";
        std::cout << "SID:         " << (row.sid.empty() ? "(none)" : row.sid) << "\n";
        std::cout << "1st WPT:     " << (row.first_waypoint.empty() ? "(none)" : row.first_waypoint) << "\n";
        std::cout << "SID suffix:  " << (row.sid_suffix.empty() ? "(none)" : row.sid_suffix) << "\n";
        std::cout << "RFL:         FL" << (row.rfl / 100) << "\n";
        std::cout << "Sim time:    day=" << simDay << " " << simHour << ":" << simMin << " UTC\n";
        std::cout << "\n";
    };

    // ---- IFR gate — mirrors analyzeFP.cpp line 1414 ----
    // Plugin returns "Not Checked" immediately; no constraint rounds run.
    if (!row.is_ifr) {
        printHeader();
        std::cout << "NOT CHECKED (VFR) — plugin does not validate non-IFR flight plans.\n\n";
        return 0;
    }

    // ---- Destination ERR gate — mirrors analyzeFP.cpp line 1472 ----
    // Plugin sets VF_DESTINATION="ERR"; FinalizeValidationResult_ treats ERR
    // as a hard failure regardless of constraint round outcomes.
    if (row.destination.empty()) {
        printHeader();
        std::cout << "ERR: destination missing — plugin marks VF_DESTINATION=ERR (hard failure).\n\n";
        return 1;
    }

    // ---- Route ERR gate — mirrors analyzeFP.cpp line 1476 ----
    // Plugin sets VF_SYNTAX="ERR" when row.route (the raw route string from
    // EuroScope) is empty; FinalizeValidationResult_ treats ERR as a hard
    // failure regardless of constraint round outcomes.  EuroScope always
    // provides the route string when it provides route_points, so in plugin
    // usage both are absent together.  The CLI mirrors this exactly: "route"
    // must be present (route_points alone is not sufficient).
    if (!have_route_text) {
        printHeader();
        std::cout << "ERR: \"route\" missing — plugin marks VF_SYNTAX=ERR (hard failure).\n";
        std::cout << "     Provide the raw route string in \"route\"; \"route_points\" alone is not accepted.\n\n";
        return 1;
    }

    // ---- Unwrap the {cycle, airports:[...]} envelope if present ----
    // The parser produces an envelope object; the plugin strips it on ingest.
    // ExtractAirportFromDocument handles a bare array or a single-airport object,
    // so we need to extract the airports array from the envelope here.
    rapidjson::Document airportArrayDoc;
    {
        const rapidjson::Value* airportsVal = nullptr;
        if (outDoc.IsObject()) {
            auto it = outDoc.FindMember("airports");
            if (it != outDoc.MemberEnd() && it->value.IsArray()) {
                // Envelope form: {cycle:"...", airports:[...]}
                airportsVal = &it->value;
                airportArrayDoc.CopyFrom(*airportsVal, airportArrayDoc.GetAllocator());
            }
        }
        if (!airportArrayDoc.IsArray()) {
            // Already a bare array or single-airport object — copy as-is.
            airportArrayDoc.CopyFrom(outDoc, airportArrayDoc.GetAllocator());
        }
    }

    // ---- Extract airport document ----
    rapidjson::Document airportDoc;
    if (!SidLookup::ExtractAirportFromDocument(airportArrayDoc, row.origin, airportDoc)) {
        std::cerr << "ERROR: airport " << row.origin
                  << " not found in " << outJsonPath << "\n";
        return 2;
    }

    // ---- Run validation ----
    const ValidatorCore::ValidationOutcome result =
        ValidatorCore::validateFlightPlan(row, airportDoc, simDay, simHour, simMin);

    // ---- Print result ----
    printHeader();

    if (!result.sid_found) {
        std::cout << "RESULT: FAIL — no matching SID / departure route found in " << row.origin << "\n";
        std::cout << "        Check that first_waypoint matches a SID entry in out.json.\n\n";
        return 1;
    }

    if (!result.sid_level_pass) {
        std::cout << "RESULT: FAIL — SID-level restriction blocked this flight plan\n";
        std::cout << "        (engine type, suffix, time window, or banned-type restriction)\n\n";
        return 1;
    }

    // Syntax errors are a SYN advisory: plugin sets VF_SYNTAX="Invalid Syntax"
    // (not "ERR"), continues with rounds 1 and 2 as no-ops, and lets the
    // constraint rounds determine overall pass/fail.
    if (!result.syntax_ok) {
        std::cout << "SYN ADVISORY: route syntax error — " << result.syntax_error << "\n";
        std::cout << "              (rounds 1=exit and 2=route were skipped; other rounds still ran)\n\n";
    }

    if (result.overall_pass) {
        std::cout << "RESULT: " << (result.syntax_ok ? "PASS" : "PASS (with SYN)")
                  << " — all 7 constraint rounds completed\n";
        if (result.warns) {
            std::cout << "        WARNING — at least one alert-type constraint triggered\n";
        }
    } else {
        std::cout << "RESULT: FAIL — eliminated at round " << result.fail_round << "\n";
        std::cout << "        (rounds: 0=dest 1=exit 2=route 3=restrictions 4=FL 5=direction 6=alerts)\n";

        if (!result.constraint_validity.empty()) {
            bool anyPass = false;
            for (bool b : result.constraint_validity) if (b) { anyPass = true; break; }
            if (!anyPass) {
                std::cout << "        No constraints survived the last completed round.\n";
            }
        }
    }
    std::cout << "\n";

    return result.overall_pass ? 0 : 1;
}
