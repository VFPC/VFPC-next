#pragma once
#include <string>
#include <chrono>

#include "rapidjson/document.h"

// checkTimeWindow
//
// Returns true when the current time (day/hour/min) falls INSIDE the
// restriction window described by a RapidJSON restriction object, i.e.
// the restriction is ACTIVE and a flight plan should be checked against it.
//
// Day convention (Monday = 0 … Sunday = 6) must match the internal
// timedata[5] representation that versionCall() builds after the
// (day + 6) % 7 conversion from the server's Sunday-0 value.
//
// Both start and end boundaries are INCLUSIVE.
// Time strings are four-digit HHMM, e.g. "1600", "0745".
//
// Returns true (no restriction applies) when the restriction object
// has no "start"/"end" members, or neither contains a "date" nor a "time".

int GetWeekDay()
{
	auto now = std::chrono::system_clock::now();
    auto days = floor<std::chrono::days>(now);

    std::chrono::weekday wd{ days };
	return (wd.iso_encoding() - 1); // Monday=0, Sunday=6
}

inline bool checkTimeWindow(const std::string eobt, const rapidjson::Value& restriction)
{
    if (!restriction.HasMember("start") || !restriction.HasMember("end"))
        return true;

    const rapidjson::Value& restriction_start = restriction["start"];
    const rapidjson::Value& restriction_end   = restriction["end"];

    bool restriction_hasDate = restriction_start.HasMember("date") && restriction_start["date"].IsInt()
                && restriction_end.HasMember("date")   && restriction_end["date"].IsInt();
    bool restriction_hasTime = restriction_start.HasMember("time") && restriction_start["time"].IsString()
                && restriction_end.HasMember("time")   && restriction_end["time"].IsString();

    if (!restriction_hasDate && !restriction_hasTime)
        return true;

    int restriction_startdate = 0, restriction_enddate = 0;
    int restriction_starttime[2] = { 0, 0 };
    int restriction_endtime[2]   = { 0, 0 };

    if (restriction_hasDate) {
        restriction_startdate = restriction_start["date"].GetInt();
        restriction_enddate   = restriction_end["date"].GetInt();
    }

    if (restriction_hasTime) {
        std::string s = restriction_start["time"].GetString();
        std::string e = restriction_end["time"].GetString();
        restriction_starttime[0] = std::stoi(s.substr(0, 2));
        restriction_starttime[1] = std::stoi(s.substr(2, 2));
        restriction_endtime[0]   = std::stoi(e.substr(0, 2));
        restriction_endtime[1]   = std::stoi(e.substr(2, 2));
    }

    int eobt_time[2] = { 0, 0 };
    if (!eobt.empty()) {
        eobt_time[0] = std::stoi(eobt.substr(0, 2));
        eobt_time[1] = std::stoi(eobt.substr(2, 2));
	}

    // Shared boundary helpers (all boundaries inclusive):
    //   afterStart  : current >= start time
    //   beforeEnd   : current <= end time
    auto afterStart = [&]() {
        return eobt_time[0] > restriction_starttime[0]
            || (eobt_time[0] == restriction_starttime[0] && eobt_time[1] >= restriction_starttime[1]);
    };
    auto beforeEnd = [&]() {
        return eobt_time[0] < restriction_endtime[0]
            || (eobt_time[0] == restriction_endtime[0] && eobt_time[1] <= restriction_endtime[1]);
    };

    bool valid = false;
	int currentDay = GetWeekDay();

    if (!restriction_hasDate) {
        // ── Time-only window ────────────────────────────────────────────────
        // When start >= end the window wraps midnight (e.g. 23:00 → 01:00).
        if (restriction_starttime[0] > restriction_endtime[0]
         || (restriction_starttime[0] == restriction_endtime[0] && restriction_starttime[1] >= restriction_endtime[1])) {
            // Midnight-wrapping: valid if >= start OR <= end
            if (afterStart() || beforeEnd())
                valid = true;
        } else {
            // Normal same-day window: valid if >= start AND <= end
            if (afterStart() && beforeEnd())
                valid = true;
        }

    } else if (restriction_startdate == restriction_enddate) {
        // ── Same weekday ────────────────────────────────────────────────────
        if (currentDay == restriction_startdate) {
            if (!restriction_hasTime || (afterStart() && beforeEnd()))
                valid = true;
        }

    } else if (restriction_startdate < restriction_enddate) {
        // ── Forward range (no week wrap, e.g. Monday → Tuesday) ─────────────
        if (currentDay > restriction_startdate && currentDay < restriction_enddate) {
            valid = true;                          // interior day: always valid
        } else if (currentDay == restriction_startdate) {
            if (!restriction_hasTime || afterStart())
                valid = true;
        } else if (currentDay == restriction_enddate) {
            if (!restriction_hasTime || beforeEnd())
                valid = true;
        }

    } else {
        // ── Wrap-around range (startdate > enddate, e.g. Friday → Monday) ───
        if (currentDay == restriction_startdate) {
            if (!restriction_hasTime || afterStart())
                valid = true;
        } else if (currentDay == restriction_enddate) {
            if (!restriction_hasTime || beforeEnd())
                valid = true;
        } else if (currentDay > restriction_startdate || currentDay < restriction_enddate) {
            valid = true;                          // interior wrap day: always valid
        }
    }

    return valid;
}
