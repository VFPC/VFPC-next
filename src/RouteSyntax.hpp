// RouteSyntax.hpp
// Standalone route normalisation and validation entry point.
//
// Provides normaliseAndValidateRoute(), which is a free-function mirror of
// CVFPCPlugin::NormaliseAndValidateRouteSyntax_ (src/analyzeFP.cpp).
// The function has no EuroScope dependency, no class state, and uses only
// std::regex and standard string/vector operations.
//
// WHAT IT DOES
// Modifies `route` in-place through five sequential passes:
//
//  Pass 0 — Strip a leading speed/level group (e.g. "N0450F370")
//  Pass 1 — Strip leading SID designator tokens and/or the origin ICAO;
//            fails if a different origin ICAO is encountered
//  Pass 2 — Strip trailing STAR designator tokens and/or the destination ICAO;
//            fails if a different destination ICAO is encountered
//  Pass 3 — Remove DCT tokens; strip inline speed/level-change suffixes
//            (e.g. "GANLO/N0420F380" -> "GANLO"); fail on "DCT/N..." or
//            unrecognised slash suffixes
//  Pass 4 — If a SID is present, verify and strip the first_wp from the front
//
// On success returns true; `route` holds the cleaned waypoint sequence.
// On failure returns false; `outchk` describes the failure.
//
// LIMITATION: This header is a test-support mirror of the function in
// analyzeFP.cpp. If NormaliseAndValidateRouteSyntax_ changes in that file,
// this header must be kept in sync.

#pragma once

#include <regex>
#include <string>
#include <vector>

namespace RouteSyntax {

// Regex patterns used in each pass.
// These are identical to the local variables inside NormaliseAndValidateRouteSyntax_
// in analyzeFP.cpp; extracted to namespace scope so tests can reference them directly.
namespace patterns {
    // Speed/level group: N0450F370, M080A045, K0500M0850
    inline const std::regex spdlvl      { R"((N|M|K)[0-9]{3,4}((A|F)[0-9]{3}|(S|M)[0-9]{4}))" };
    // Speed/level group preceded by '/': for inline change stripping
    inline const std::regex spdlvlslash { R"(\/(N|M|K)[0-9]{3,4}((A|F)[0-9]{3}|(S|M)[0-9]{4})((A|F)[0-9]{3}|(S|M)[0-9]{4})?)" };
    // ICAO 4-letter code, optionally followed by runway: EGAA, EGPH/28R
    inline const std::regex icaorwy     { R"([A-Z]{4}(\/[0-9]{2}(L|C|R)?)?)" };
    // SID/STAR designator: BELZU1J, TLA2N, LANGO3D/28L
    inline const std::regex sidstarrwy  { R"([A-Z]{2,5}[0-9][A-Z](\/[0-9]{2}(L|C|R)?)?)" };
    // DCT followed immediately by a speed/level group
    inline const std::regex dctspdlvl   { R"(DCT\/(N|M|K)[0-9]{3,4}((A|F)[0-9]{3}|(S|M)[0-9]{4}))" };
    // Airway designator: M14, N601, UL607, UA1
    inline const std::regex awy         { R"((U)?[A-Z][0-9]{1,3}([A-Z])?)" };
} // namespace patterns

// normaliseAndValidateRoute
//
// Exact behavioural mirror of CVFPCPlugin::NormaliseAndValidateRouteSyntax_.
//
// Parameters:
//   route       — tokenised route; modified in-place (cleaned on success)
//   origin      — departure ICAO (e.g. "EGAA")
//   destination — destination ICAO (e.g. "EIDW")
//   sid         — SID name; empty string means no SID (non-SID departure)
//   first_wp    — first route waypoint after SID; used only when sid is non-empty
//   outchk      — set to a short failure description when false is returned
//
// Returns true on success; route is normalised in place.
// Returns false on any structural error; outchk describes the failure.
inline bool normaliseAndValidateRoute(
    std::vector<std::string>& route,
    const std::string& origin,
    const std::string& destination,
    const std::string& sid,
    const std::string& first_wp,
    std::string& outchk)
{
    using namespace patterns;

    bool success = true;
    bool repeat  = false;
    std::vector<std::string> new_route;

    for (size_t i = 0; i < 5; ++i) {
        if (!success) break;

        if (route.empty()) {
            outchk = "No Route";
            return false;
        }

        switch (i) {
        // ------------------------------------------------------------------
        // Pass 0: strip leading speed/level group
        // ------------------------------------------------------------------
        case 0:
            if (std::regex_match(route.front(), spdlvl)) {
                route.erase(route.begin());
            }
            break;

        // ------------------------------------------------------------------
        // Pass 1: strip leading SID designator(s), "SID" literal, and/or
        //         the origin ICAO. Fails if a different 4-letter ICAO is found.
        // ------------------------------------------------------------------
        case 1:
            do {
                repeat = false;
                if (route.empty()) {
                    outchk = "No Route";
                    return false;
                }
                if (std::regex_match(route.front(), sidstarrwy) ||
                    route.front() == "SID") {
                    route.erase(route.begin());
                    repeat = true;
                }
                else if (std::regex_match(route.front(), icaorwy)) {
                    if (route.front().substr(0, 4) == origin) {
                        route.erase(route.begin());
                        repeat = true;
                    }
                    else {
                        outchk = "Different Origin in Route";
                        return false;
                    }
                }
            } while (repeat);
            break;

        // ------------------------------------------------------------------
        // Pass 2: strip trailing STAR designator(s), "STAR" literal, and/or
        //         the destination ICAO. Fails if a different 4-letter ICAO found.
        // ------------------------------------------------------------------
        case 2:
            do {
                repeat = false;
                if (route.empty()) {
                    outchk = "No Route";
                    return false;
                }
                if (std::regex_match(route.back(), sidstarrwy) ||
                    route.back() == "STAR") {
                    route.pop_back();
                    repeat = true;
                }
                else if (std::regex_match(route.back(), icaorwy)) {
                    if (route.back().substr(0, 4) == destination) {
                        route.pop_back();
                        repeat = true;
                    }
                    else {
                        outchk = "Different Destination in Route";
                        return false;
                    }
                }
            } while (repeat);
            break;

        // ------------------------------------------------------------------
        // Pass 3: DCT removal and inline speed/level-change stripping.
        //   - "DCT/N..." → fail (invalid)
        //   - "DCT"      → drop
        //   - airway     → keep
        //   - no slash   → keep as waypoint
        //   - "WPT/N..." → keep "WPT", drop the suffix
        //   - other slash suffixes → fail
        // ------------------------------------------------------------------
        case 3:
            new_route.clear();
            for (const auto& each : route) {
                if (std::regex_match(each, dctspdlvl)) {
                    return false;
                }
                if (each == "DCT") {
                    continue;
                }
                if (std::regex_match(each, awy)) {
                    new_route.push_back(each);
                    continue;
                }
                const size_t slash = each.find('/');
                if (slash == std::string::npos) {
                    new_route.push_back(each);
                    continue;
                }
                const std::string chng = each.substr(slash);
                if (std::regex_match(chng, spdlvlslash)) {
                    new_route.push_back(each.substr(0, slash));
                }
                else {
                    outchk = "Invalid Speed/Level Change";
                    return false;
                }
            }
            route = std::move(new_route);
            break;

        // ------------------------------------------------------------------
        // Pass 4: when departing on a SID, the first remaining token must be
        //         first_wp (the departure waypoint). Strip it.
        // ------------------------------------------------------------------
        case 4:
            if (!sid.empty()) {
                if (route.empty()) {
                    outchk = "No Route";
                    return false;
                }
                if (route.front() != first_wp) {
                    outchk = "Route Not From First Waypoint";
                    return false;
                }
                route.erase(route.begin());
            }
            break;
        }
    }
    return true;
}

} // namespace RouteSyntax
