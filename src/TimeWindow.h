#pragma once
#pragma once

#include <string>
#include <chrono>
#include <optional>

#include "rapidjson/document.h"

// TimeWindow.h
//
// EOBT is supplied as HHMM UTC.
// Restriction times are supplied as HHMM local time.
//
// Example:
//   EOBT 2200Z in summer -> 2300 local Europe/London
//
// Window boundary policy:
//   start is inclusive
//   end is exclusive
//
// Therefore:
//   2300-0500 means:
//     2300 passes
//     0459 passes
//     0500 fails

namespace vfpc_time
{
    inline std::optional<int> HhmmToMinutes(const std::string& hhmm)
    {
        if (hhmm.size() != 4)
            return std::nullopt;

        if (!std::isdigit(static_cast<unsigned char>(hhmm[0])) ||
            !std::isdigit(static_cast<unsigned char>(hhmm[1])) ||
            !std::isdigit(static_cast<unsigned char>(hhmm[2])) ||
            !std::isdigit(static_cast<unsigned char>(hhmm[3])))
            return std::nullopt;

        int hours = std::stoi(hhmm.substr(0, 2));
        int mins = std::stoi(hhmm.substr(2, 2));

        if (hours < 0 || hours > 23 || mins < 0 || mins > 59)
            return std::nullopt;

        return (hours * 60) + mins;
    }

    inline std::chrono::sys_seconds MakeTodayUtcEobt(const std::string& eobt_hhmm)
    {
        using namespace std::chrono;

        auto mins = HhmmToMinutes(eobt_hhmm);
        if (!mins)
            return sys_seconds{};

        auto now_utc = system_clock::now();
        auto today_utc = floor<days>(now_utc);

        return time_point_cast<seconds>(today_utc + minutes{ *mins });
    }

    inline std::optional<int> UtcEobtToLocalMinutes(
        const std::string& eobt_hhmm,
        const char* timezone_name = "Europe/London")
    {
        using namespace std::chrono;

        auto eobt_minutes = HhmmToMinutes(eobt_hhmm);
        if (!eobt_minutes)
            return std::nullopt;

        auto eobt_utc = MakeTodayUtcEobt(eobt_hhmm);

        try {
            const auto* zone = locate_zone(timezone_name);

            zoned_time<seconds> local_time{ zone, eobt_utc };
            auto local_tp = local_time.get_local_time();

            auto local_day = floor<days>(local_tp);
            hh_mm_ss tod{ local_tp - local_day };

            int local_hours = static_cast<int>(tod.hours().count());
            int local_mins = static_cast<int>(tod.minutes().count());

            return (local_hours * 60) + local_mins;
        }
        catch (...) {
            return std::nullopt;
        }
    }

    inline int LocalWeekDayFromUtcEobt(
        const std::string& eobt_hhmm,
        const char* timezone_name = "Europe/London")
    {
        using namespace std::chrono;

        auto eobt_utc = MakeTodayUtcEobt(eobt_hhmm);

        try {
            const auto* zone = locate_zone(timezone_name);

            zoned_time<seconds> local_time{ zone, eobt_utc };
            auto local_tp = local_time.get_local_time();

            auto local_day = floor<days>(local_tp);
            weekday wd{ local_day };

            return wd.iso_encoding() - 1; // Monday=0, Sunday=6
        }
        catch (...) {
            auto today_utc = floor<days>(system_clock::now());
            weekday wd{ today_utc };

            return wd.iso_encoding() - 1;
        }
    }

    inline bool TimeInWindow(
        int test_minutes,
        int start_minutes,
        int end_minutes)
    {
        if (start_minutes == end_minutes) {
            // Treat equal start/end as all day.
            return true;
        }

        if (start_minutes < end_minutes) {
            // Normal same-day window, e.g. 0900-1700
            return test_minutes >= start_minutes &&
                test_minutes < end_minutes;
        }

        // Midnight-crossing window, e.g. 2300-0500
        return test_minutes >= start_minutes ||
            test_minutes < end_minutes;
    }
}

inline bool checkTimeWindow(
    const std::string eobt,
    const rapidjson::Value& restriction)
{
    using namespace vfpc_time;

    if (!restriction.HasMember("start") || !restriction.HasMember("end"))
        return true;

    const rapidjson::Value& restriction_start = restriction["start"];
    const rapidjson::Value& restriction_end = restriction["end"];

    bool restriction_hasDate =
        restriction_start.HasMember("date") &&
        restriction_start["date"].IsInt() &&
        restriction_end.HasMember("date") &&
        restriction_end["date"].IsInt();

    bool restriction_hasTime =
        restriction_start.HasMember("time") &&
        restriction_start["time"].IsString() &&
        restriction_end.HasMember("time") &&
        restriction_end["time"].IsString();

    if (!restriction_hasDate && !restriction_hasTime)
        return true;

    int restriction_startdate = 0;
    int restriction_enddate = 0;

    if (restriction_hasDate) {
        restriction_startdate = restriction_start["date"].GetInt();
        restriction_enddate = restriction_end["date"].GetInt();
    }

    std::optional<int> local_eobt_minutes;

    if (restriction_hasTime) {
        local_eobt_minutes = UtcEobtToLocalMinutes(eobt, "Europe/London");

        if (!local_eobt_minutes)
            return false;
    }

    int restriction_start_minutes = 0;
    int restriction_end_minutes = 0;

    if (restriction_hasTime) {
        auto start_minutes =
            HhmmToMinutes(restriction_start["time"].GetString());

        auto end_minutes =
            HhmmToMinutes(restriction_end["time"].GetString());

        if (!start_minutes || !end_minutes)
            return false;

        restriction_start_minutes = *start_minutes;
        restriction_end_minutes = *end_minutes;
    }

    int currentDay = LocalWeekDayFromUtcEobt(eobt, "Europe/London");

    bool valid = false;

    if (!restriction_hasDate) {
        valid = TimeInWindow(
            *local_eobt_minutes,
            restriction_start_minutes,
            restriction_end_minutes
        );
    }
    else if (restriction_startdate == restriction_enddate) {
        if (currentDay == restriction_startdate) {
            if (!restriction_hasTime) {
                valid = true;
            }
            else {
                valid = TimeInWindow(
                    *local_eobt_minutes,
                    restriction_start_minutes,
                    restriction_end_minutes
                );
            }
        }
    }
    else if (restriction_startdate < restriction_enddate) {
        if (currentDay > restriction_startdate &&
            currentDay < restriction_enddate) {
            valid = true;
        }
        else if (currentDay == restriction_startdate) {
            if (!restriction_hasTime) {
                valid = true;
            }
            else {
                valid = *local_eobt_minutes >= restriction_start_minutes;
            }
        }
        else if (currentDay == restriction_enddate) {
            if (!restriction_hasTime) {
                valid = true;
            }
            else {
                valid = *local_eobt_minutes < restriction_end_minutes;
            }
        }
    }
    else {
        // Week wrap, e.g. Friday to Monday
        if (currentDay == restriction_startdate) {
            if (!restriction_hasTime) {
                valid = true;
            }
            else {
                valid = *local_eobt_minutes >= restriction_start_minutes;
            }
        }
        else if (currentDay == restriction_enddate) {
            if (!restriction_hasTime) {
                valid = true;
            }
            else {
                valid = *local_eobt_minutes < restriction_end_minutes;
            }
        }
        else if (currentDay > restriction_startdate ||
            currentDay < restriction_enddate) {
            valid = true;
        }
    }

    return valid;
}