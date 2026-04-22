// SidApplicability.hpp
//
// Helpers for deciding whether SID-level restrictions should gate the
// constraint loop in ValidateFlightPlan_.
//
// Core rule:
// - If a SID-level restriction actually applies to the flight and blocks it,
//   keep the SID-level failure and report RST.
// - If no SID-level restriction is currently applicable to the flight,
//   do not short-circuit the constraint loop. Fall through so destination,
//   route, and other constraint checks can produce the correct result.
//
// This addresses cases like EGPH/TLA where the SID carries a non-jet
// restriction at the SID root. A jet should not be forced into RST simply
// because that restriction exists in the array; it should continue to
// normal constraint evaluation when the restriction is not in scope.

#pragma once

#include <string>

#include "rapidjson/document.h"

#include "TimeWindow.h"

namespace SidApplicability {

inline bool ArrayContainsString(const rapidjson::Value& arr, const std::string& needle)
{
    for (rapidjson::SizeType i = 0; i < arr.Size(); ++i) {
        if (arr[i].IsString() && needle == arr[i].GetString()) {
            return true;
        }
    }
    return false;
}

inline bool ArrayContainsEnding(const rapidjson::Value& arr, const std::string& suffix)
{
    for (rapidjson::SizeType i = 0; i < arr.Size(); ++i) {
        if (!arr[i].IsString()) {
            continue;
        }

        const std::string selector = arr[i].GetString();
        if (selector.size() > suffix.size()) {
            continue;
        }

        if (suffix.compare(suffix.size() - selector.size(), selector.size(), selector) == 0) {
            return true;
        }
    }
    return false;
}

inline bool RestrictionSelectorsMatch(
    const rapidjson::Value& restriction,
    const std::string& engine_type,
    const std::string& aircraft_type,
    const std::string& sid_suffix,
    int current_day,
    int current_hour,
    int current_min)
{
    if (!restriction.IsObject()) {
        return false;
    }

    if (restriction.HasMember("suffix") &&
        restriction["suffix"].IsArray() &&
        restriction["suffix"].Size() > 0 &&
        !ArrayContainsEnding(restriction["suffix"], sid_suffix)) {
        return false;
    }

    if (restriction.HasMember("types") &&
        restriction["types"].IsArray() &&
        restriction["types"].Size() > 0 &&
        !ArrayContainsString(restriction["types"], engine_type) &&
        !ArrayContainsString(restriction["types"], aircraft_type)) {
        return false;
    }

    const bool has_window =
        restriction.HasMember("start") &&
        restriction.HasMember("end") &&
        restriction["start"].IsObject() &&
        restriction["end"].IsObject();

    if (has_window &&
        !checkTimeWindow(current_day, current_hour, current_min, restriction)) {
        return false;
    }

    return true;
}

inline bool AnySidRestrictionApplicable(
    const rapidjson::Value& restrictions,
    const std::string& engine_type,
    const std::string& aircraft_type,
    const std::string& sid_suffix,
    int current_day,
    int current_hour,
    int current_min)
{
    if (!restrictions.IsArray() || restrictions.Size() == 0) {
        return false;
    }

    for (rapidjson::SizeType i = 0; i < restrictions.Size(); ++i) {
        if (RestrictionSelectorsMatch(
                restrictions[i],
                engine_type,
                aircraft_type,
                sid_suffix,
                current_day,
                current_hour,
                current_min)) {
            return true;
        }
    }

    return false;
}

inline bool ShouldEvaluateConstraints(
    bool sid_restrictions_passed,
    const rapidjson::Value& restrictions,
    const std::string& engine_type,
    const std::string& aircraft_type,
    const std::string& sid_suffix,
    int current_day,
    int current_hour,
    int current_min)
{
    if (sid_restrictions_passed) {
        return true;
    }

    // No SID-level restriction is currently in scope, so the SID gate should
    // not mask the downstream destination / route / level result.
    return !AnySidRestrictionApplicable(
        restrictions,
        engine_type,
        aircraft_type,
        sid_suffix,
        current_day,
        current_hour,
        current_min);
}

} // namespace SidApplicability
