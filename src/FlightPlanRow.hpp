// FlightPlanRow.hpp
// Plain data representation of a parsed flight plan.
//
// Mirrors the FlightPlanRow struct defined at the top of analyzeFP.hpp,
// extracted here so test-support headers can include it without pulling in
// EuroScopePlugIn.h (which analyzeFP.hpp transitively includes via Constant.hpp
// -> stdafx.h).
//
// If analyzeFP.hpp changes FlightPlanRow, this header must be kept in sync.

#pragma once

#include <string>
#include <vector>

struct FlightPlanRow
{
    std::string callsign;
    std::string eobt;           // [day, hour, minute] as encoded by the plugin
    std::string origin;         // departure ICAO
    std::string destination;    // destination ICAO
    std::string route;          // raw route string
    std::vector<std::string> route_points; // tokenised route waypoints
    std::string sid;            // SID name (e.g. "BELZU1J")
    std::string sid_suffix;     // SID suffix letter (e.g. "J", "P") — used for suffix restriction matching
    std::string first_waypoint; // first waypoint after departure — used to look up the SID entry
    std::string aircraft_type;  // ICAO type designator (e.g. "B738")
    std::string engine_type;    // ICAO engine type letter (e.g. "J" = jet, "P" = piston)
    int  rfl     = 0;           // requested flight level in feet (e.g. 36000 for FL360)
    bool is_ifr  = false;       // true for IFR flight plans
};
