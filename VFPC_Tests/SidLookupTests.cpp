// SidLookupTests.cpp
// Tests for the out.json -> airport -> SID selection seam.
//
// These functions are extracted from CVFPCPlugin::TryFindSidIndex_ and
// CVFPCPlugin::ExtractSingleAirportToObject_ (src/analyzeFP.cpp).
// They operate on plain RapidJSON types with no EuroScope dependency.
//
// SCOPE NOTE: This covers the selection step only — navigating the loaded JSON
// to find the right airport and the right SID. It does NOT test the constraint
// evaluation that follows (covered by ApiConstraintTests + ConstraintBatchTests)
// or the active-airport locking / IFR gating that precedes it (EuroScope-dependent).

#include <gtest/gtest.h>

#include "SidLookup.hpp"
#include "rapidjson/document.h"

using namespace SidLookup;

// Convenience macro: parse a JSON literal and assert it parsed without error.
#define JSON(var, literal)                                      \
    rapidjson::Document var;                                    \
    var.Parse<rapidjson::kParseDefaultFlags>(literal);          \
    ASSERT_FALSE(var.HasParseError()) << "JSON parse error in " #var

// ============================================================
// TryFindSidIndex — point-field matching
// ============================================================

TEST(SidLookup_TryFindSidIndex, SingleSid_ExactPointMatch)
{
    JSON(doc, R"([{"point":"BELZU","constraints":[{"min":50}]}])");
    std::size_t pos = std::string::npos;
    EXPECT_TRUE(TryFindSidIndex(doc, "BELZU", pos));
    EXPECT_EQ(pos, 0u);
}

TEST(SidLookup_TryFindSidIndex, MultipleSids_MatchesSecond)
{
    JSON(doc, R"([
        {"point":"GOMUP","constraints":[{"min":100}]},
        {"point":"BELZU","constraints":[{"min":50}]}
    ])");
    std::size_t pos = std::string::npos;
    EXPECT_TRUE(TryFindSidIndex(doc, "BELZU", pos));
    EXPECT_EQ(pos, 1u);
}

TEST(SidLookup_TryFindSidIndex, NoMatch_ReturnsNpos)
{
    JSON(doc, R"([{"point":"BELZU","constraints":[{"min":50}]}])");
    std::size_t pos = 99u;
    EXPECT_FALSE(TryFindSidIndex(doc, "GOMUP", pos));
    EXPECT_EQ(pos, std::string::npos);
}

// ============================================================
// TryFindSidIndex — alias matching
// ============================================================

TEST(SidLookup_TryFindSidIndex, AliasMatch_FirstAlias)
{
    // SID with aliases array — first alias matches.
    JSON(doc, R"([{"point":"BELZU","aliases":["BEL","BLZ"],"constraints":[{"min":50}]}])");
    std::size_t pos = std::string::npos;
    EXPECT_TRUE(TryFindSidIndex(doc, "BEL", pos));
    EXPECT_EQ(pos, 0u);
}

TEST(SidLookup_TryFindSidIndex, AliasMatch_SecondAlias)
{
    // Second entry in the aliases array also matches.
    JSON(doc, R"([{"point":"BELZU","aliases":["BEL","BLZ"],"constraints":[{"min":50}]}])");
    std::size_t pos = std::string::npos;
    EXPECT_TRUE(TryFindSidIndex(doc, "BLZ", pos));
    EXPECT_EQ(pos, 0u);
}

TEST(SidLookup_TryFindSidIndex, AliasMatch_WrongSid_NoMatch)
{
    // An alias exists, but for a different SID; the searched waypoint doesn't match any.
    JSON(doc, R"([
        {"point":"GOMUP","aliases":["GOM"],"constraints":[{"min":100}]},
        {"point":"BELZU","aliases":["BEL"],"constraints":[{"min":50}]}
    ])");
    std::size_t pos = 99u;
    EXPECT_FALSE(TryFindSidIndex(doc, "NATEB", pos));
    EXPECT_EQ(pos, std::string::npos);
}

// ============================================================
// TryFindSidIndex — usability guard (no constraints = skip)
// ============================================================

TEST(SidLookup_TryFindSidIndex, SidWithNoConstraints_Skipped)
{
    // A SID whose "point" matches but has no "constraints" array is treated as
    // unusable and skipped. This mirrors the guard in analyzeFP.cpp:
    // "SID must have a constraints array to be considered usable."
    JSON(doc, R"([{"point":"BADENTRY"}])");
    std::size_t pos = 99u;
    EXPECT_FALSE(TryFindSidIndex(doc, "BADENTRY", pos));
    EXPECT_EQ(pos, std::string::npos);
}

TEST(SidLookup_TryFindSidIndex, SkipsNoConstraints_FindsNext)
{
    // First SID has matching point but no constraints (skipped);
    // second SID has matching point with constraints (found).
    JSON(doc, R"([
        {"point":"BELZU"},
        {"point":"BELZU","constraints":[{"min":50}]}
    ])");
    std::size_t pos = std::string::npos;
    EXPECT_TRUE(TryFindSidIndex(doc, "BELZU", pos));
    EXPECT_EQ(pos, 1u);
}

// ============================================================
// TryFindSidIndex — guard conditions
// ============================================================

TEST(SidLookup_TryFindSidIndex, EmptySids_ReturnsFalse)
{
    JSON(doc, R"([])");
    std::size_t pos = 99u;
    EXPECT_FALSE(TryFindSidIndex(doc, "BELZU", pos));
    EXPECT_EQ(pos, std::string::npos);
}

TEST(SidLookup_TryFindSidIndex, EmptyFirstWp_ReturnsFalse)
{
    // Empty first_wp is rejected before the loop (matches plugin behavior: the
    // plugin only calls TryFindSidIndex_ when first_waypoint is non-empty).
    JSON(doc, R"([{"point":"BELZU","constraints":[{"min":50}]}])");
    std::size_t pos = 99u;
    EXPECT_FALSE(TryFindSidIndex(doc, "", pos));
    EXPECT_EQ(pos, std::string::npos);
}

// ============================================================
// ExtractAirportFromDocument — single-object root
// ============================================================

TEST(SidLookup_Extract, SingleObject_MatchingIcao)
{
    JSON(in, R"({"icao":"EGAA","sids":[]})");
    rapidjson::Document out;
    EXPECT_TRUE(ExtractAirportFromDocument(in, "EGAA", out));
    ASSERT_TRUE(out.IsObject());
    ASSERT_TRUE(out.HasMember("icao"));
    EXPECT_STREQ(out["icao"].GetString(), "EGAA");
}

TEST(SidLookup_Extract, SingleObject_MismatchedIcao)
{
    JSON(in, R"({"icao":"EGAA","sids":[]})");
    rapidjson::Document out;
    EXPECT_FALSE(ExtractAirportFromDocument(in, "EGPH", out));
}

TEST(SidLookup_Extract, SingleObject_NoIcaoField)
{
    JSON(in, R"({"sids":[]})");
    rapidjson::Document out;
    EXPECT_FALSE(ExtractAirportFromDocument(in, "EGAA", out));
}

// ============================================================
// ExtractAirportFromDocument — array root (full out.json structure)
// ============================================================

TEST(SidLookup_Extract, ArrayRoot_AirportFound)
{
    JSON(in, R"([{"icao":"EGAA","sids":[]},{"icao":"EGPH","sids":[]}])");
    rapidjson::Document out;
    EXPECT_TRUE(ExtractAirportFromDocument(in, "EGPH", out));
    ASSERT_TRUE(out.IsObject());
    EXPECT_STREQ(out["icao"].GetString(), "EGPH");
}

TEST(SidLookup_Extract, ArrayRoot_AirportNotFound)
{
    JSON(in, R"([{"icao":"EGAA","sids":[]},{"icao":"EGPH","sids":[]}])");
    rapidjson::Document out;
    EXPECT_FALSE(ExtractAirportFromDocument(in, "EGLL", out));
}

TEST(SidLookup_Extract, EmptyIcao_ReturnsFalse)
{
    JSON(in, R"({"icao":"EGAA","sids":[]})");
    rapidjson::Document out;
    EXPECT_FALSE(ExtractAirportFromDocument(in, "", out));
}

// ============================================================
// Full selection-chain integration test
//
// Simulates the complete path: full document -> extract airport ->
// find SID by first waypoint -> access constraints array.
// This is the exact sequence that ValidateFlightPlan_ performs after
// loading data, minus the EuroScope-dependent gating steps.
// ============================================================

TEST(SidLookup_Chain, ExtractAirport_FindSid_ReadConstraints)
{
    JSON(fullDoc, R"([
        {"icao":"EGAA","sids":[
            {"point":"BELZU","constraints":[{"min":50,"max":660}]}
        ]},
        {"icao":"EGPH","sids":[
            {"point":"TLA","constraints":[{"min":60,"max":410}]},
            {"point":"GOMUP","aliases":["GOM"],"constraints":[{"min":70}]}
        ]}
    ])");

    // Step 1: extract EGPH airport object from the full array.
    rapidjson::Document airportOut;
    ASSERT_TRUE(ExtractAirportFromDocument(fullDoc, "EGPH", airportOut));
    ASSERT_TRUE(airportOut.IsObject());
    EXPECT_STREQ(airportOut["icao"].GetString(), "EGPH");

    // Step 2: find the TLA SID by first waypoint.
    ASSERT_TRUE(airportOut.HasMember("sids") && airportOut["sids"].IsArray());
    std::size_t sidPos = std::string::npos;
    ASSERT_TRUE(TryFindSidIndex(airportOut["sids"], "TLA", sidPos));
    EXPECT_EQ(sidPos, 0u);

    // Step 3: confirm constraints are accessible at the found index.
    const auto& sid = airportOut["sids"][static_cast<rapidjson::SizeType>(sidPos)];
    ASSERT_TRUE(sid.HasMember("constraints") && sid["constraints"].IsArray());
    ASSERT_EQ(sid["constraints"].Size(), 1u);
    EXPECT_EQ(sid["constraints"][0]["min"].GetInt(), 60);
    EXPECT_EQ(sid["constraints"][0]["max"].GetInt(), 410);

    // Step 4: alias match on the second SID in the same airport.
    std::size_t aliasPos = std::string::npos;
    ASSERT_TRUE(TryFindSidIndex(airportOut["sids"], "GOM", aliasPos));
    EXPECT_EQ(aliasPos, 1u);
}
