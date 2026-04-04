// ValidatorCore.hpp
// Standalone mid-stack validation entry point.
//
// Provides validateFlightPlan(), which takes a FlightPlanRow, a pre-loaded
// single-airport JSON object, and an explicit time context, and runs the full
// constraint-selection + evaluation path that CVFPCPlugin::ValidateFlightPlan_
// performs after the EuroScope-dependent gating steps are satisfied.
//
// WHAT THIS COVERS vs. THE LIVE PLUGIN
// +------------------------------------+
// | Live plugin step                   | Covered here? |
// |------------------------------------|---------------|
// | Snapshot load (airport_data_)      | Replaced by   |
// |                                    | direct param  |
// | IFR/VFR gating                     | Skipped (test |
// |                                    | controls row) |
// | Active-airport lock (mutex)        | Skipped       |
// | Airport/SID navigation             | YES           |
// | SID-level restriction evaluation   | YES           |
// | Constraint evaluation (7 rounds)   | YES           |
// | Route syntax normalisation         | YES (via      |
// |                                    | RouteSyntax:: |
// |                                    | normalise...) |
// | Output string formatting           | Skipped       |
// +------------------------------------+
//
// LIMITATION: This header is a test-support mirror of logic in analyzeFP.cpp.
// If the validation logic in that file changes, this header must be kept in sync.
// The timedata[5/3/4] file-level global is replaced by explicit day/hour/min
// parameters. Logging calls are omitted.

#pragma once

#include <algorithm>
#include <string>
#include <vector>

#include "rapidjson/document.h"

#include "FlightPlanRow.hpp"
#include "RouteSyntax.hpp"
#include "SidLookup.hpp"
#include "ConstraintChecks.hpp"
#include "TimeWindow.h"

namespace ValidatorCore {

// ============================================================
// Internal helpers
// Mirrors RestrictionObjectMatches_, EvaluateSidRestrictions_,
// EvaluateConstraintRestrictions_, CheckConstraintRestrictions_
// from CVFPCPlugin in analyzeFP.cpp.
// ============================================================
namespace detail {

    struct RestrictionFailureFlags {
        bool suffix = false;
        bool type   = false;
        bool time   = false;
        bool banned = false;
    };

    struct SidRestrictionResult {
        bool has_restrictions = false;
        bool passed           = true;
        RestrictionFailureFlags fails;
    };

    struct ConstraintRestrictionResult {
        bool has_restrictions = false;
        bool passed           = true;
        RestrictionFailureFlags fails;
    };

    // Whether s ends with any element of arr.
    // Mirrors CVFPCPlugin::arrayContainsEnding.
    inline bool arrayContainsEnding(const rapidjson::Value& a, const std::string& s)
    {
        for (rapidjson::SizeType i = 0; i < a.Size(); ++i) {
            const std::string comp = a[i].GetString();
            if (comp.size() > s.size()) continue;
            const std::size_t pos = s.size() - comp.size();
            bool valid = true;
            for (std::size_t j = 0; j < comp.size(); ++j) {
                if (comp[j] != s[pos + j]) { valid = false; break; }
            }
            if (valid) return true;
        }
        return false;
    }

    // Exact string match against any element of arr.
    // Mirrors CVFPCPlugin::arrayContainsString.
    inline bool arrayContainsString(const rapidjson::Value& a, const std::string& s)
    {
        for (rapidjson::SizeType i = 0; i < a.Size(); ++i) {
            if (std::string(a[i].GetString()) == s) return true;
        }
        return false;
    }

    // Evaluate one restriction object against the flight.
    // Returns true if the restriction *matches* (i.e. the flight satisfies all
    // conditions — suffix allowed, type included, time window active, not banned).
    // Mirrors CVFPCPlugin::RestrictionObjectMatches_.
    // day/hour/min replace the timedata[5/3/4] file-level global.
    inline bool restrictionObjectMatches(
        const FlightPlanRow& row,
        const rapidjson::Value& restriction,
        int day, int hour, int min,
        RestrictionFailureFlags& fails)
    {
        if (!restriction.IsObject()) return false;

        RestrictionFailureFlags local{};
        bool matches = true;

        // Suffix check
        if (const auto* suffixArr = SidLookup::detail::FindArrayMember(restriction, "suffix")) {
            if (!suffixArr->Empty()) {
                if (!arrayContainsEnding(*suffixArr, row.sid_suffix)) {
                    local.suffix = true;
                    matches = false;
                }
            }
        }

        // Aircraft / engine type check
        if (const auto* typesArr = SidLookup::detail::FindArrayMember(restriction, "types")) {
            if (!typesArr->Empty()) {
                if (!arrayContainsString(*typesArr, row.engine_type) &&
                    !arrayContainsString(*typesArr, row.aircraft_type)) {
                    local.type = true;
                    matches = false;
                }
            }
        }

        // Time-window check
        const auto* startObj = SidLookup::detail::FindMemberValue(restriction, "start");
        const auto* endObj   = SidLookup::detail::FindMemberValue(restriction, "end");
        if (startObj && endObj && startObj->IsObject() && endObj->IsObject()) {
            if (!checkTimeWindow(day, hour, min, restriction)) {
                local.time = true;
                matches = false;
            }
        }

        // Banned flag
        if (const auto* bannedVal = SidLookup::detail::FindMemberValue(restriction, "banned")) {
            if (bannedVal->IsBool() && bannedVal->GetBool()) {
                local.banned = true;
                matches = false;
            }
        }

        if (!matches) {
            fails.suffix = fails.suffix || local.suffix;
            fails.type   = fails.type   || local.type;
            fails.time   = fails.time   || local.time;
            fails.banned = fails.banned || local.banned;
        }
        return matches;
    }

    // Evaluate the SID-level restrictions array.
    // Passes if ANY single restriction in the array matches the flight.
    // Mirrors CVFPCPlugin::EvaluateSidRestrictions_.
    inline SidRestrictionResult evaluateSidRestrictions(
        const FlightPlanRow& row,
        const rapidjson::Value& sid_restrictions,
        int day, int hour, int min)
    {
        SidRestrictionResult result{};
        if (!sid_restrictions.IsArray() || sid_restrictions.Empty()) {
            result.has_restrictions = false;
            result.passed           = true;
            return result;
        }
        result.has_restrictions = true;
        result.passed           = false;

        for (rapidjson::SizeType i = 0; i < sid_restrictions.Size(); ++i) {
            RestrictionFailureFlags local_fails{};
            if (restrictionObjectMatches(row, sid_restrictions[i], day, hour, min, local_fails)) {
                result.passed = true;
                return result;
            }
            result.fails.suffix = result.fails.suffix || local_fails.suffix;
            result.fails.type   = result.fails.type   || local_fails.type;
            result.fails.time   = result.fails.time   || local_fails.time;
            result.fails.banned = result.fails.banned || local_fails.banned;
        }
        return result;
    }

    // Evaluate per-constraint restriction arrays.
    // Mirrors CVFPCPlugin::EvaluateConstraintRestrictions_.
    inline ConstraintRestrictionResult evaluateConstraintRestrictions(
        const FlightPlanRow& row,
        const rapidjson::Value& restriction_array,
        int day, int hour, int min)
    {
        ConstraintRestrictionResult result{};
        if (!restriction_array.IsArray() || restriction_array.Empty()) {
            result.has_restrictions = false;
            result.passed           = true;
            return result;
        }
        result.has_restrictions = true;
        result.passed           = false;

        for (rapidjson::SizeType i = 0; i < restriction_array.Size(); ++i) {
            RestrictionFailureFlags local_fails{};
            if (restrictionObjectMatches(row, restriction_array[i], day, hour, min, local_fails)) {
                result.passed = true;
                return result;
            }
            result.fails.suffix = result.fails.suffix || local_fails.suffix;
            result.fails.type   = result.fails.type   || local_fails.type;
            result.fails.time   = result.fails.time   || local_fails.time;
            result.fails.banned = result.fails.banned || local_fails.banned;
        }
        return result;
    }

    // Apply per-constraint restrictions to the current validity vector.
    // Mirrors CVFPCPlugin::CheckConstraintRestrictions_.
    inline std::vector<bool> checkConstraintRestrictions(
        const FlightPlanRow& row,
        const rapidjson::Value& conditions,
        int day, int hour, int min,
        RestrictionFailureFlags& aggregate_failures,
        std::vector<bool>& in)
    {
        std::vector<bool> out;
        out.reserve(conditions.Size());

        for (rapidjson::SizeType i = 0; i < conditions.Size(); ++i) {
            if (!in[i]) { out.push_back(false); continue; }

            const auto& condition = conditions[i];
            if (!condition.IsObject()) { out.push_back(false); continue; }

            const auto* restrictionArr = SidLookup::detail::FindArrayMember(condition, "restrictions");
            if (!restrictionArr) {
                out.push_back(true); // no per-constraint narrowing — still eligible
                continue;
            }

            const auto r = evaluateConstraintRestrictions(row, *restrictionArr, day, hour, min);
            if (!r.passed) {
                aggregate_failures.suffix = aggregate_failures.suffix || r.fails.suffix;
                aggregate_failures.type   = aggregate_failures.type   || r.fails.type;
                aggregate_failures.time   = aggregate_failures.time   || r.fails.time;
                aggregate_failures.banned = aggregate_failures.banned || r.fails.banned;
            }
            out.push_back(r.passed);
        }
        return out;
    }

    // Apply a single-constraint predicate across the conditions array,
    // propagating the current validity filter.
    template <typename CheckFn>
    inline std::vector<bool> applyCheck(
        const rapidjson::Value& conditions,
        std::vector<bool>& in,
        CheckFn fn)
    {
        std::vector<bool> out;
        out.reserve(conditions.Size());
        for (rapidjson::SizeType i = 0; i < conditions.Size(); ++i) {
            if (!in[i]) { out.push_back(false); continue; }
            const auto& c = conditions[i];
            if (!c.IsObject()) { out.push_back(false); continue; }
            out.push_back(fn(c));
        }
        return out;
    }

} // namespace detail

// ============================================================
// ValidationOutcome
// Plain result returned by validateFlightPlan.
// ============================================================
struct ValidationOutcome {
    bool sid_found      = false; // A matching SID/non-SID route was found in the airport
    bool sid_level_pass = true;  // SID-level restrictions were satisfied (or none existed)
    bool syntax_ok      = true;  // Route syntax normalisation succeeded
    std::string syntax_error;    // Failure description when syntax_ok is false
    bool overall_pass   = false; // All 7 evaluation rounds completed with ≥1 constraint surviving
    int  fail_round     = -1;    // Round where evaluation stopped (-1=no rounds ran, 7=all passed)
    bool warns          = false; // At least one warning constraint triggered
    std::vector<bool> constraint_validity; // Constraint validity vector after the last completed round
};

// ============================================================
// validateFlightPlan
//
// Run the constraint selection and evaluation path with plain-data inputs.
//
// Parameters:
//   row          — parsed flight plan; route_points should be pre-cleaned
//                  (route syntax normalisation is not applied here)
//   airport      — single airport JSON object, same shape as the plugin uses
//                  after CVFPCPlugin::ExtractSingleAirportToObject_:
//                  {"icao":"EGXX","sids":[{"point":"...","constraints":[...]}]}
//   day/hour/min — simulated UTC time; day uses Monday=0 convention (same as
//                  checkTimeWindow and the plugin's timedata[5/3/4])
// ============================================================
inline ValidationOutcome validateFlightPlan(
    const FlightPlanRow& row,
    const rapidjson::Value& airport,
    int day, int hour, int min)
{
    using namespace ConstraintChecks;
    using SidLookup::detail::FindMemberValue;
    using SidLookup::detail::FindArrayMember;

    ValidationOutcome out;

    // ---- 1. Locate sids array in airport ----
    const auto* airportSids = FindArrayMember(airport, "sids");
    if (!airportSids || airportSids->Empty()) {
        return out; // airport has no SID data
    }

    // ---- 2. Match SID by first_waypoint ----
    // Special case: single SID with empty "point" field = non-SID departure route.
    // Mirrors the auto-match logic at the start of the SID search in ValidateFlightPlan_.
    std::size_t matchedPos = std::string::npos;
    if (airportSids->Size() == 1) {
        const auto* ptVal = FindMemberValue((*airportSids)[0], "point");
        if (ptVal && ptVal->IsString() && std::string(ptVal->GetString()).empty()) {
            matchedPos = 0;
        }
    }
    if (matchedPos == std::string::npos && !row.first_waypoint.empty()) {
        SidLookup::TryFindSidIndex(*airportSids, row.first_waypoint, matchedPos);
    }
    if (matchedPos == std::string::npos) {
        return out; // no matching SID found
    }

    const auto& matchedSid = (*airportSids)[static_cast<rapidjson::SizeType>(matchedPos)];
    const auto* constraints = FindArrayMember(matchedSid, "constraints");
    if (!constraints || constraints->Empty()) {
        return out; // SID exists but has no evaluable constraints
    }
    out.sid_found = true;

    // ---- 3. SID-level restrictions ----
    const auto* sidRestrictions = FindArrayMember(matchedSid, "restrictions");
    if (sidRestrictions) {
        const auto r = detail::evaluateSidRestrictions(row, *sidRestrictions, day, hour, min);
        out.sid_level_pass = r.passed;
    }
    if (!out.sid_level_pass) {
        return out; // SID restriction gates the flight — skip constraint evaluation
    }

    // ---- 4. Route syntax normalisation ----
    // Mirrors CVFPCPlugin::ValidateFlightPlan_ which calls
    // NormaliseAndValidateRouteSyntax_ on a copy of route_points before the
    // constraint loop. The original route_points is kept for exit-point check
    // (round 1); the normalised copy is used for route check (round 2).
    std::vector<std::string> normalised_route = row.route_points;
    std::string syntax_err;
    const bool syntax_ok = RouteSyntax::normaliseAndValidateRoute(
        normalised_route, row.origin, row.destination, row.sid, row.first_waypoint, syntax_err);
    out.syntax_ok    = syntax_ok;
    out.syntax_error = syntax_err;

    // ---- 5. Constraint evaluation loop (7 rounds) ----
    // Mirrors the while (round < 7) loop in ValidateFlightPlan_.
    // Rounds 1 and 2 are gated on syntax_ok — if syntax failed, they are
    // no-ops (validity unchanged), matching the plugin's behaviour.
    std::vector<bool> validity(constraints->Size(), true);
    bool warn = false;
    detail::RestrictionFailureFlags constraintRestrictionFails{};
    int round = 0;

    const auto allFalse = [](const std::vector<bool>& v) {
        return std::all_of(v.begin(), v.end(), [](bool b) { return !b; });
    };

    while (round < 7) {
        std::vector<bool> newValidity;

        switch (round) {
        case 0: // destination
            newValidity = detail::applyCheck(*constraints, validity,
                [&](const rapidjson::Value& c) {
                    return checkOneDestination(c, row.destination);
                });
            break;
        case 1: // exit point — uses original route_points; no-op if syntax failed
            if (!syntax_ok) {
                newValidity = validity;
            } else {
                newValidity = detail::applyCheck(*constraints, validity,
                    [&](const rapidjson::Value& c) {
                        return checkOneExitPoint(c, row.route_points);
                    });
            }
            break;
        case 2: // route — uses normalised_route; no-op if syntax failed
            if (!syntax_ok) {
                newValidity = validity;
            } else {
                newValidity = detail::applyCheck(*constraints, validity,
                    [&](const rapidjson::Value& c) {
                        return checkOneRoute(c, normalised_route);
                    });
            }
            break;
        case 3: // per-constraint restrictions
            newValidity = detail::checkConstraintRestrictions(
                row, *constraints, day, hour, min, constraintRestrictionFails, validity);
            break;
        case 4: // min/max FL
            newValidity = detail::applyCheck(*constraints, validity,
                [&](const rapidjson::Value& c) {
                    return checkOneMinMax(c, row.rfl);
                });
            break;
        case 5: // direction (EVEN/ODD)
            newValidity = detail::applyCheck(*constraints, validity,
                [&](const rapidjson::Value& c) {
                    return checkOneDirection(c, row.rfl);
                });
            break;
        case 6: // alerts
            newValidity = detail::applyCheck(*constraints, validity,
                [&](const rapidjson::Value& c) {
                    return checkOneAlerts(c, warn);
                });
            break;
        }

        if (newValidity.empty() || allFalse(newValidity)) {
            break; // all constraints eliminated — stop here
        }
        validity = newValidity;
        ++round;
    }

    out.fail_round          = round;
    out.warns               = warn;
    out.constraint_validity = validity;
    out.overall_pass        = (round == 7);
    return out;
}

} // namespace ValidatorCore
