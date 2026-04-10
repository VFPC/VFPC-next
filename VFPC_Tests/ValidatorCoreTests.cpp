// ValidatorCoreTests.cpp
// Mid-stack integration tests for ValidatorCore::validateFlightPlan.
//
// SCOPE: Covers the path from airport JSON + FlightPlanRow + time context to a
// ValidationOutcome, exercising:
//   - SID navigation (airport JSON -> sids array -> first_waypoint match)
//   - SID-level restriction evaluation (types / suffix / time window)
//   - Constraint evaluation rounds 0-6 (destination / exit / route /
//     per-constraint restrictions / min-max / direction / alerts)
//
// NOT COVERED HERE:
//   - Route syntax normalisation (NormaliseAndValidateRouteSyntax_) — test
//     cases supply pre-cleaned route_points
//   - IFR/VFR gating, active-airport mutex — both EuroScope-dependent
//   - Output string formatting (ValidationResult text fields)
//
// See test-harness.md for a full scope map.

#include <gtest/gtest.h>

#include "ValidatorCore.hpp"

using ValidatorCore::validateFlightPlan;
using ValidatorCore::ValidationOutcome;

// Convenience macro: parse a JSON literal and assert it parsed without error.
#define JSON(var, literal)                                          \
    rapidjson::Document var;                                        \
    var.Parse<rapidjson::kParseDefaultFlags>(literal);              \
    ASSERT_FALSE(var.HasParseError()) << "JSON parse error in " #var

// Convenience builder for a minimal FlightPlanRow.
static FlightPlanRow makeRow(
    const std::string& origin,
    const std::string& destination,
    const std::string& first_wp,
    const std::vector<std::string>& route_points,
    int rfl,
    const std::string& engine_type = "J",
    const std::string& aircraft_type = "B738",
    const std::string& sid_suffix = "J")
{
    FlightPlanRow r;
    r.origin       = origin;
    r.destination  = destination;
    r.first_waypoint = first_wp;
    r.route_points = route_points;
    r.rfl          = rfl;
    r.engine_type  = engine_type;
    r.aircraft_type = aircraft_type;
    r.sid_suffix   = sid_suffix;
    r.is_ifr       = true;
    return r;
}

// ============================================================
// TC-1: Happy path — SID found, all 7 rounds pass
// ============================================================
TEST(ValidatorCore_MidStack, HappyPath_AllRoundsPass)
{
    // Airport: EGAA, one SID "BELZU" with one constraint that allows everything.
    // The constraint has dests=["EI"] (matches EIDW), no exit/route/min-max/
    // direction restriction, so all 7 rounds survive.
    JSON(airport, R"({
        "icao": "EGAA",
        "sids": [{
            "point": "BELZU",
            "constraints": [{
                "min": 60,
                "max": 660
            }]
        }]
    })");

    const auto row = makeRow("EGAA", "EIDW", "BELZU", {"BELZU", "EIDW"}, 24000);
    const ValidationOutcome result = validateFlightPlan(row, airport, 0, 12, 0);

    EXPECT_TRUE(result.sid_found);
    EXPECT_TRUE(result.sid_level_pass);
    EXPECT_TRUE(result.overall_pass);
    EXPECT_EQ(result.fail_round, 7);
    ASSERT_EQ(result.constraint_validity.size(), 1u);
    EXPECT_TRUE(result.constraint_validity[0]);
}

// ============================================================
// TC-2: Destination fails — stops at round 0
// ============================================================
TEST(ValidatorCore_MidStack, DestinationFails_Round0)
{
    // Constraint restricts destination to EGLL only.
    // Flight is going to EIDW — destination check eliminates the constraint.
    JSON(airport, R"({
        "icao": "EGAA",
        "sids": [{
            "point": "BELZU",
            "constraints": [{
                "dests": ["EGLL"],
                "min": 60,
                "max": 660
            }]
        }]
    })");

    const auto row = makeRow("EGAA", "EIDW", "BELZU", {"BELZU", "EIDW"}, 24000);
    const ValidationOutcome result = validateFlightPlan(row, airport, 0, 12, 0);

    EXPECT_TRUE(result.sid_found);
    EXPECT_TRUE(result.sid_level_pass);
    EXPECT_FALSE(result.overall_pass);
    EXPECT_EQ(result.fail_round, 0); // stopped at destination round
}

// ============================================================
// TC-3: Min/max FL fails — stops at round 4
// ============================================================
TEST(ValidatorCore_MidStack, MinMaxFails_Round4)
{
    // Constraint allows destinations starting "EI", no exit/route restrictions,
    // but caps at FL410 (41000 ft). Flight at FL600 fails min-max.
    JSON(airport, R"({
        "icao": "EGAA",
        "sids": [{
            "point": "BELZU",
            "constraints": [{
                "dests": ["EI"],
                "min": 60,
                "max": 410
            }]
        }]
    })");

    const auto row = makeRow("EGAA", "EIDW", "BELZU", {"BELZU", "EIDW"}, 60000);
    const ValidationOutcome result = validateFlightPlan(row, airport, 0, 12, 0);

    EXPECT_TRUE(result.sid_found);
    EXPECT_TRUE(result.sid_level_pass);
    EXPECT_FALSE(result.overall_pass);
    EXPECT_EQ(result.fail_round, 4); // destination(0), exit(1), route(2), restrictions(3) pass;
                                     // min-max(4) fails
}

// ============================================================
// TC-4: Direction fails — stops at round 5
// ============================================================
TEST(ValidatorCore_MidStack, DirectionFails_Round5)
{
    // Constraint requires EVEN flight levels. FL370 (37000 ft) is odd → fails round 5.
    // Destination and min-max are permissive so rounds 0-4 survive.
    JSON(airport, R"({
        "icao": "EGAA",
        "sids": [{
            "point": "BELZU",
            "constraints": [{
                "dests": ["EI"],
                "min": 60,
                "max": 660,
                "dir": "EVEN"
            }]
        }]
    })");

    const auto row = makeRow("EGAA", "EIDW", "BELZU", {"BELZU", "EIDW"}, 37000);
    const ValidationOutcome result = validateFlightPlan(row, airport, 0, 12, 0);

    EXPECT_TRUE(result.sid_found);
    EXPECT_TRUE(result.sid_level_pass);
    EXPECT_FALSE(result.overall_pass);
    EXPECT_EQ(result.fail_round, 5); // rounds 0-4 survive; direction(5) fails FL370 (odd)
}

// ============================================================
// TC-5: SID not found in airport
// ============================================================
TEST(ValidatorCore_MidStack, SidNotFound)
{
    JSON(airport, R"({
        "icao": "EGAA",
        "sids": [{
            "point": "BELZU",
            "constraints": [{"min": 60}]
        }]
    })");

    // first_waypoint "GOMUP" doesn't match the only SID in the airport.
    const auto row = makeRow("EGAA", "EIDW", "GOMUP", {"GOMUP", "EIDW"}, 24000);
    const ValidationOutcome result = validateFlightPlan(row, airport, 0, 12, 0);

    EXPECT_FALSE(result.sid_found);
    EXPECT_FALSE(result.overall_pass);
    EXPECT_EQ(result.fail_round, -1); // no rounds ran
}

// ============================================================
// TC-6: SID-level restriction — aircraft type blocks the SID
// ============================================================
TEST(ValidatorCore_MidStack, SidRestriction_TypeBlocked)
{
    // SID restriction allows props only (engine_type="P").
    // Flight is a jet (engine_type="J", aircraft_type="B738").
    // EvaluateSidRestrictions returns passed=false → sid_level_pass=false.
    JSON(airport, R"({
        "icao": "EGAA",
        "sids": [{
            "point": "BELZU",
            "restrictions": [{"types": ["P"]}],
            "constraints": [{"min": 60, "max": 660}]
        }]
    })");

    const auto row = makeRow("EGAA", "EIDW", "BELZU", {"BELZU", "EIDW"}, 24000, "J", "B738");
    const ValidationOutcome result = validateFlightPlan(row, airport, 0, 12, 0);

    EXPECT_TRUE(result.sid_found);
    EXPECT_FALSE(result.sid_level_pass); // type restriction blocks jet
    EXPECT_FALSE(result.overall_pass);
    EXPECT_EQ(result.fail_round, -1); // no constraint rounds ran (gated by SID restriction)
}

// ============================================================
// TC-7: SID-level restriction — time window gates the SID
// ============================================================
// The SID restriction is a Monday-to-Friday time window (days 0-4, time 0800-1800).
// We test on a Saturday (day=5) at noon — outside the window, so the SID is blocked.
TEST(ValidatorCore_MidStack, SidRestriction_TimeBlocked)
{
    JSON(airport, R"({
        "icao": "EGAA",
        "sids": [{
            "point": "BELZU",
            "restrictions": [{
                "types": ["J"],
                "start": {"date": 0, "time": "0800"},
                "end":   {"date": 4, "time": "1800"}
            }],
            "constraints": [{"min": 60, "max": 660}]
        }]
    })");

    // Jet flight on Saturday 12:00 — engine_type matches but time window excludes
    // Saturday (day=5), so the restriction doesn't match → sid_level_pass=false.
    const auto row = makeRow("EGAA", "EIDW", "BELZU", {"BELZU", "EIDW"}, 24000, "J", "B738");

    // day=5 (Saturday in Monday-0 convention) — outside Mon-Fri window
    const ValidationOutcome result = validateFlightPlan(row, airport, 5, 12, 0);

    EXPECT_TRUE(result.sid_found);
    EXPECT_FALSE(result.sid_level_pass);
    EXPECT_FALSE(result.overall_pass);
}
