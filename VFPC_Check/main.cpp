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
//     "sid_suffix":    "F",
//     "first_waypoint":"UMLAT",
//     "aircraft_type": "B738",
//     "engine_type":   "J",
//     "rfl":           36000,
//     "is_ifr":        true,
//     "sim_day":       0,
//     "sim_hour":      10,
//     "sim_min":       30
//   }
//
//   route_points is the raw tokenised route (space-split of route, with speed/
//   level groups, DCT tokens etc still present — same as what the plugin reads
//   from EuroScope).  sim_day uses Monday=0 convention.  Omit sim_* to default
//   to Monday 12:00 UTC.

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
        "Exit codes:\n"
        "  0  PASS — all 7 constraint rounds completed\n"
        "  1  FAIL — constraint or SID-level gate eliminated all options\n"
        "  2  ERROR — bad arguments, unreadable files, or JSON parse failure\n";
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

    // ---- Validate required fields ----
    if (row.origin.empty()) {
        std::cerr << "ERROR: flight plan missing \"origin\"\n"; return 2;
    }
    if (row.destination.empty()) {
        std::cerr << "ERROR: flight plan missing \"destination\"\n"; return 2;
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
    std::cout << "\n=== VFPC Route Check ===\n";
    std::cout << "Callsign:    " << (row.callsign.empty() ? "(none)" : row.callsign) << "\n";
    std::cout << "Origin:      " << row.origin << "\n";
    std::cout << "Destination: " << row.destination << "\n";
    std::cout << "Route:       " << row.route << "\n";
    std::cout << "SID:         " << (row.sid.empty() ? "(none)" : row.sid) << "\n";
    std::cout << "1st WPT:     " << (row.first_waypoint.empty() ? "(none)" : row.first_waypoint) << "\n";
    std::cout << "RFL:         FL" << (row.rfl / 100) << "\n";
    std::cout << "Sim time:    day=" << simDay << " " << simHour << ":" << simMin << " UTC\n";
    std::cout << "\n";

    if (!result.sid_found) {
        std::cout << "RESULT: FAIL — no matching SID / departure route found in " << row.origin << "\n";
        std::cout << "        Check that first_waypoint matches a SID entry in out.json.\n";
        return 1;
    }

    if (!result.sid_level_pass) {
        std::cout << "RESULT: FAIL — SID-level restriction blocked this flight plan\n";
        std::cout << "        (engine type, suffix, time window, or banned-type restriction)\n";
        return 1;
    }

    if (!result.syntax_ok) {
        std::cout << "RESULT: FAIL — route syntax error: " << result.syntax_error << "\n";
        return 1;
    }

    if (result.overall_pass) {
        std::cout << "RESULT: PASS — all 7 constraint rounds completed\n";
        if (result.warns) {
            std::cout << "        WARNING — at least one alert-type constraint triggered\n";
        }
    } else {
        std::cout << "RESULT: FAIL — eliminated at round " << result.fail_round << "\n";
        std::cout << "        (rounds: 0=dest 1=exit 2=route 3=restrictions 4=FL 5=direction 6=alerts)\n";

        // Which constraints survived the last completed round?
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
