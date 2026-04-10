// RouteSyntaxTests.cpp
// Unit and integration tests for RouteSyntax::normaliseAndValidateRoute.
//
// Tests are organised by pass number, then by failure cases, then by the
// realistic end-to-end combination tests.
//
// Naming convention for helper:
//   make({"A","B","C"}) -> std::vector<std::string>{"A","B","C"}
//
// SCOPE:
//   Covers all five normalisation passes individually and in combination.
//   The integration tests drive the route through ValidatorCore::validateFlightPlan
//   to confirm that the normalised route feeds round-2 (route check) correctly.

#include <gtest/gtest.h>

#include "RouteSyntax.hpp"
#include "ValidatorCore.hpp"

using RouteSyntax::normaliseAndValidateRoute;
using ValidatorCore::validateFlightPlan;
using ValidatorCore::ValidationOutcome;

// Build a vector<string> from an initializer list.
static std::vector<std::string> make(std::initializer_list<std::string> tokens)
{
    return std::vector<std::string>(tokens);
}

// Run the normaliser with default empty sid/first_wp (no-SID departure).
static bool norm(std::vector<std::string>& route,
                 const std::string& origin,
                 const std::string& dest)
{
    std::string chk;
    return normaliseAndValidateRoute(route, origin, dest, "", "", chk);
}

// Run with a SID.
static bool normSid(std::vector<std::string>& route,
                    const std::string& origin,
                    const std::string& dest,
                    const std::string& sid,
                    const std::string& first_wp)
{
    std::string chk;
    return normaliseAndValidateRoute(route, origin, dest, sid, first_wp, chk);
}

// ============================================================
// Empty route
// ============================================================

TEST(RouteSyntax_Guard, EmptyRoute_ReturnsFalse)
{
    auto route = make({});
    std::string chk;
    EXPECT_FALSE(normaliseAndValidateRoute(route, "EGAA", "EIDW", "", "", chk));
    EXPECT_EQ(chk, "No Route");
}

// ============================================================
// Pass 0 — leading speed/level group
// ============================================================

TEST(RouteSyntax_Pass0, SpeedLevel_N_F_StrippedFromFront)
{
    auto route = make({"N0450F370", "BELZU", "M14", "WOD"});
    ASSERT_TRUE(norm(route, "EGAA", "EIDW"));
    EXPECT_EQ(route.front(), "BELZU");
}

TEST(RouteSyntax_Pass0, SpeedLevel_M_A_StrippedFromFront)
{
    auto route = make({"M082A045", "BELZU", "M14"});
    ASSERT_TRUE(norm(route, "EGAA", "EIDW"));
    EXPECT_EQ(route.front(), "BELZU");
}

TEST(RouteSyntax_Pass0, SpeedLevel_K_S_StrippedFromFront)
{
    auto route = make({"K0500S0850", "BELZU", "M14"});
    ASSERT_TRUE(norm(route, "EGAA", "EIDW"));
    EXPECT_EQ(route.front(), "BELZU");
}

TEST(RouteSyntax_Pass0, NonSpeedFront_Unchanged)
{
    // Normal waypoint at front — pass 0 must not strip it.
    auto route = make({"BELZU", "M14"});
    ASSERT_TRUE(norm(route, "EGAA", "EIDW"));
    EXPECT_EQ(route.front(), "BELZU");
}

// ============================================================
// Pass 1 — leading SID designator, "SID" literal, origin ICAO
// ============================================================

TEST(RouteSyntax_Pass1, SidDesignator_StrippedFromFront)
{
    // "BELZU1J" matches sidstarrwy pattern and gets stripped.
    auto route = make({"BELZU1J", "BELZU", "M14"});
    ASSERT_TRUE(norm(route, "EGAA", "EIDW"));
    EXPECT_EQ(route.front(), "BELZU");
}

TEST(RouteSyntax_Pass1, MultipleSidDesignators_AllStripped)
{
    // Consecutive SID-like tokens at the front are all stripped.
    auto route = make({"BELZU1J", "TLA2N", "BELZU", "M14"});
    ASSERT_TRUE(norm(route, "EGAA", "EIDW"));
    EXPECT_EQ(route.front(), "BELZU");
}

TEST(RouteSyntax_Pass1, SID_Literal_StrippedFromFront)
{
    auto route = make({"SID", "BELZU", "M14"});
    ASSERT_TRUE(norm(route, "EGAA", "EIDW"));
    EXPECT_EQ(route.front(), "BELZU");
}

TEST(RouteSyntax_Pass1, OriginIcao_StrippedFromFront)
{
    auto route = make({"EGAA", "BELZU", "M14"});
    ASSERT_TRUE(norm(route, "EGAA", "EIDW"));
    EXPECT_EQ(route.front(), "BELZU");
}

TEST(RouteSyntax_Pass1, OriginIcao_WithRunway_StrippedFromFront)
{
    auto route = make({"EGAA/28", "BELZU", "M14"});
    ASSERT_TRUE(norm(route, "EGAA", "EIDW"));
    EXPECT_EQ(route.front(), "BELZU");
}

TEST(RouteSyntax_Pass1, DifferentOriginIcao_ReturnsFalse)
{
    // EGPH is a 4-letter code, but it's not the origin EGAA → fail.
    auto route = make({"EGPH", "BELZU", "M14"});
    std::string chk;
    EXPECT_FALSE(normaliseAndValidateRoute(route, "EGAA", "EIDW", "", "", chk));
    EXPECT_EQ(chk, "Different Origin in Route");
}

// ============================================================
// Pass 2 — trailing STAR designator, "STAR" literal, destination ICAO
// ============================================================

TEST(RouteSyntax_Pass2, StarDesignator_StrippedFromBack)
{
    // "TLA1N" is a SID/STAR designator (letters + digit + letter).
    auto route = make({"BELZU", "M14", "TLA1N"});
    ASSERT_TRUE(norm(route, "EGAA", "EIDW"));
    EXPECT_EQ(route.back(), "M14");
}

TEST(RouteSyntax_Pass2, STAR_Literal_StrippedFromBack)
{
    auto route = make({"BELZU", "M14", "STAR"});
    ASSERT_TRUE(norm(route, "EGAA", "EIDW"));
    EXPECT_EQ(route.back(), "M14");
}

TEST(RouteSyntax_Pass2, DestinationIcao_StrippedFromBack)
{
    auto route = make({"BELZU", "M14", "EIDW"});
    ASSERT_TRUE(norm(route, "EGAA", "EIDW"));
    EXPECT_EQ(route.back(), "M14");
}

TEST(RouteSyntax_Pass2, DestinationIcao_WithRunway_StrippedFromBack)
{
    auto route = make({"BELZU", "M14", "EIDW/28R"});
    ASSERT_TRUE(norm(route, "EGAA", "EIDW"));
    EXPECT_EQ(route.back(), "M14");
}

TEST(RouteSyntax_Pass2, DifferentDestinationIcao_ReturnsFalse)
{
    auto route = make({"BELZU", "M14", "EGLL"});
    std::string chk;
    EXPECT_FALSE(normaliseAndValidateRoute(route, "EGAA", "EIDW", "", "", chk));
    EXPECT_EQ(chk, "Different Destination in Route");
}

// ============================================================
// Pass 3 — DCT removal and inline speed/level-change stripping
// ============================================================

TEST(RouteSyntax_Pass3, DCT_Tokens_Removed)
{
    auto route = make({"BELZU", "DCT", "WOD", "DCT", "GANLO"});
    ASSERT_TRUE(norm(route, "EGAA", "EIDW"));
    EXPECT_EQ(route, (make({"BELZU", "WOD", "GANLO"})));
}

TEST(RouteSyntax_Pass3, InlineSpeedLevelChange_Stripped)
{
    // "GANLO/N0420F380" → keep "GANLO", drop the speed/level change suffix.
    auto route = make({"BELZU", "GANLO/N0420F380", "WOD"});
    ASSERT_TRUE(norm(route, "EGAA", "EIDW"));
    EXPECT_EQ(route, (make({"BELZU", "GANLO", "WOD"})));
}

TEST(RouteSyntax_Pass3, Airway_Kept)
{
    // "M14" and "UN601" are airway designators — kept as-is.
    auto route = make({"BELZU", "M14", "WOD", "UN601", "GANLO"});
    ASSERT_TRUE(norm(route, "EGAA", "EIDW"));
    EXPECT_EQ(route, (make({"BELZU", "M14", "WOD", "UN601", "GANLO"})));
}

TEST(RouteSyntax_Pass3, DCTSpeedLevel_ReturnsFalse)
{
    // "DCT/N0450F370" is an invalid token — triggers return false.
    auto route = make({"BELZU", "DCT/N0450F370", "WOD"});
    std::string chk;
    EXPECT_FALSE(normaliseAndValidateRoute(route, "EGAA", "EIDW", "", "", chk));
}

TEST(RouteSyntax_Pass3, InvalidSlashSuffix_ReturnsFalse)
{
    // Slash present but suffix is not a valid speed/level change → fail.
    auto route = make({"BELZU", "WOD/INVALID", "GANLO"});
    std::string chk;
    EXPECT_FALSE(normaliseAndValidateRoute(route, "EGAA", "EIDW", "", "", chk));
    EXPECT_EQ(chk, "Invalid Speed/Level Change");
}

// ============================================================
// Pass 4 — SID first_wp stripping
// ============================================================

TEST(RouteSyntax_Pass4, SidFirstWp_StrippedFromFront)
{
    // With a non-empty sid, the first remaining token must be first_wp and is stripped.
    auto route = make({"BELZU", "WOD", "M14"});
    ASSERT_TRUE(normSid(route, "EGAA", "EIDW", "BELZU1J", "BELZU"));
    EXPECT_EQ(route.front(), "WOD");
}

TEST(RouteSyntax_Pass4, SidFirstWp_Wrong_ReturnsFalse)
{
    // First token is not first_wp — fail.
    auto route = make({"GOMUP", "WOD", "M14"});
    std::string chk;
    EXPECT_FALSE(normaliseAndValidateRoute(route, "EGAA", "EIDW", "BELZU1J", "BELZU", chk));
    EXPECT_EQ(chk, "Route Not From First Waypoint");
}

TEST(RouteSyntax_Pass4, NoSid_Pass4_Skipped)
{
    // sid="" → pass 4 is entirely skipped; nothing is stripped.
    auto route = make({"BELZU", "WOD", "M14"});
    ASSERT_TRUE(normSid(route, "EGAA", "EIDW", "", ""));
    EXPECT_EQ(route.front(), "BELZU");
}

// ============================================================
// End-to-end normalisation: realistic ICAO route combinations
// ============================================================

TEST(RouteSyntax_E2E, TypicalSidRoute_FullyNormalised)
{
    // Simulates a realistic EuroScope route string tokenised into route_points.
    // Input:  N0450F370 EGAA/28 BELZU1J BELZU DCT WOD M14 GANLO/N0420F380 EIDW
    // Origin=EGAA, dest=EIDW, sid=BELZU1J, first_wp=BELZU
    //
    // Expected passes:
    //  0: N0450F370 stripped
    //  1: EGAA/28 (origin with runway) stripped, BELZU1J (SID designator) stripped
    //  2: EIDW stripped
    //  3: DCT stripped, GANLO/N0420F380 → GANLO
    //  4: BELZU (first_wp) stripped
    // Result: [WOD, M14, GANLO]
    auto route = make({"N0450F370", "EGAA/28", "BELZU1J", "BELZU",
                        "DCT", "WOD", "M14", "GANLO/N0420F380", "EIDW"});
    std::string chk;
    ASSERT_TRUE(normaliseAndValidateRoute(route, "EGAA", "EIDW", "BELZU1J", "BELZU", chk));
    EXPECT_EQ(route, (make({"WOD", "M14", "GANLO"})));
}

TEST(RouteSyntax_E2E, NoSidRoute_FullyNormalised)
{
    // No SID departure — pass 4 skipped; first_wp is kept.
    // Input: EGAA BELZU DCT WOD M14 EIDW
    // Result: [BELZU, WOD, M14]
    auto route = make({"EGAA", "BELZU", "DCT", "WOD", "M14", "EIDW"});
    std::string chk;
    ASSERT_TRUE(normaliseAndValidateRoute(route, "EGAA", "EIDW", "", "", chk));
    EXPECT_EQ(route, (make({"BELZU", "WOD", "M14"})));
}

// ============================================================
// Mid-stack integration: normalisation feeds ValidatorCore round 2
// ============================================================
// These tests verify that route normalisation connects correctly to the
// constraint evaluation loop inside ValidatorCore::validateFlightPlan.
// The route check (round 2) uses the normalised route — not the raw route_points.

TEST(RouteSyntax_MidStack, RouteCheck_PassesOnNormalisedWaypoint)
{
    // Constraint uses the "route" field (prefix match against normalised route).
    // Round 2 uses normalised_route, not raw route_points.
    //
    // Raw route_points = ["N0450F370", "BELZU", "DCT", "WOD", "EIDW"].
    // Normalisation (origin=EGAA, dest=EIDW, sid=BELZU1J, first_wp=BELZU):
    //   pass 0: N0450F370 stripped
    //   pass 1: BELZU1J stripped (SID designator) — not present; front is BELZU
    //           BELZU is not sidstarrwy or icaorwy → stops
    //   pass 2: EIDW stripped
    //   pass 3: DCT removed
    //   pass 4: BELZU (first_wp) stripped
    //   Result: ["WOD"]
    //
    // Round 1 (exit point, raw route_points): no "points" field → passes trivially.
    // Round 2 (route, normalised ["WOD"]): "route": ["WOD"] matches → passes.
    rapidjson::Document airport;
    airport.Parse(R"({
        "icao": "EGAA",
        "sids": [{
            "point": "BELZU",
            "constraints": [{
                "min": 60,
                "max": 660,
                "route": ["WOD"]
            }]
        }]
    })");
    ASSERT_FALSE(airport.HasParseError());

    FlightPlanRow row;
    row.origin         = "EGAA";
    row.destination    = "EIDW";
    row.first_waypoint = "BELZU";
    row.sid            = "BELZU1J";
    row.sid_suffix     = "J";
    row.engine_type    = "J";
    row.aircraft_type  = "B738";
    row.rfl            = 24000;
    row.route_points   = {"N0450F370", "BELZU", "DCT", "WOD", "EIDW"};
    row.is_ifr         = true;

    const ValidationOutcome result = validateFlightPlan(row, airport, 0, 12, 0);

    EXPECT_TRUE(result.syntax_ok) << result.syntax_error;
    EXPECT_TRUE(result.sid_found);
    EXPECT_TRUE(result.overall_pass);
    EXPECT_EQ(result.fail_round, 7);
}

TEST(RouteSyntax_MidStack, RouteCheck_FailsWhenWaypointAbsent)
{
    // Same "route" constraint requiring waypoint "WOD" as a prefix,
    // but the raw route has "GOW" instead. After normalisation: ["GOW"].
    // Round 1 (exit point): no "points" field → passes trivially.
    // Round 2 (route check): cc_routeHas(["GOW"], ["WOD"]) → "WOD"≠"GOW" → fails.
    // fail_round should be 2.
    rapidjson::Document airport;
    airport.Parse(R"({
        "icao": "EGAA",
        "sids": [{
            "point": "BELZU",
            "constraints": [{
                "min": 60,
                "max": 660,
                "route": ["WOD"]
            }]
        }]
    })");
    ASSERT_FALSE(airport.HasParseError());

    FlightPlanRow row;
    row.origin         = "EGAA";
    row.destination    = "EIDW";
    row.first_waypoint = "BELZU";
    row.sid            = "BELZU1J";
    row.sid_suffix     = "J";
    row.engine_type    = "J";
    row.aircraft_type  = "B738";
    row.rfl            = 24000;
    row.route_points   = {"N0450F370", "BELZU", "DCT", "GOW", "EIDW"};
    row.is_ifr         = true;

    const ValidationOutcome result = validateFlightPlan(row, airport, 0, 12, 0);

    EXPECT_TRUE(result.syntax_ok) << result.syntax_error;
    EXPECT_TRUE(result.sid_found);
    EXPECT_FALSE(result.overall_pass);
    EXPECT_EQ(result.fail_round, 2); // round 1 passes (no exit restriction); round 2 fails
}

TEST(RouteSyntax_MidStack, BadSyntax_RoundsStillRun_ButRouteCheckIsNoop)
{
    // If the route has a different origin ICAO, normalisation fails.
    // The plugin treats syntax_ok=false as a route-syntax error but still runs
    // rounds 0, 1, and 3-6 (rounds 1 and 2 are no-ops when syntax failed).
    // In this case there are no destination/exit/restriction/min-max/direction
    // constraints on the constraint — so rounds 0,1,3,4,5,6 all pass, round 2
    // is a no-op (validity unchanged), and overall_pass=true.
    // (This mirrors the plugin behaviour: syntax failure is reported in VF_ROUTE
    // but does NOT stop the full evaluation if other rounds pass.)
    rapidjson::Document airport;
    airport.Parse(R"({
        "icao": "EGAA",
        "sids": [{
            "point": "BELZU",
            "constraints": [{
                "min": 60,
                "max": 660
            }]
        }]
    })");
    ASSERT_FALSE(airport.HasParseError());

    FlightPlanRow row;
    row.origin        = "EGAA";
    row.destination   = "EIDW";
    row.first_waypoint = "BELZU";
    row.sid           = "BELZU1J";
    row.sid_suffix    = "J";
    row.engine_type   = "J";
    row.aircraft_type = "B738";
    row.rfl           = 24000;
    // Route starts with EGPH (different origin) — normalisation will fail.
    row.route_points  = {"EGPH", "BELZU", "WOD", "EIDW"};
    row.is_ifr        = true;

    const ValidationOutcome result = validateFlightPlan(row, airport, 0, 12, 0);

    EXPECT_FALSE(result.syntax_ok);
    EXPECT_EQ(result.syntax_error, "Different Origin in Route");
    EXPECT_TRUE(result.sid_found);
    // Rounds 1 and 2 are no-ops — constraint stays alive through all 7 rounds.
    EXPECT_TRUE(result.overall_pass);
    EXPECT_EQ(result.fail_round, 7);
}
