// ApiConstraintTests.cpp
// Direct unit coverage for the extracted pure constraint check helpers.

#include <gtest/gtest.h>

#include "ConstraintChecks.hpp"
#include "rapidjson/document.h"

using namespace ConstraintChecks;

#define JSON(var, literal) \
    rapidjson::Document var; \
    var.Parse<rapidjson::kParseDefaultFlags>(literal)

static std::vector<std::string> vec(std::initializer_list<const char*> il)
{
    std::vector<std::string> v;
    for (auto s : il) {
        v.push_back(s);
    }
    return v;
}

TEST(ApiConstraint_Destination, NoDests_EG_BlocksEGLL)
{
    JSON(c, R"({"nodests":["EG","EI"],"dests":[]})");
    EXPECT_FALSE(checkOneDestination(c, "EGLL"));
    EXPECT_FALSE(checkOneDestination(c, "EIDW"));
    EXPECT_TRUE(checkOneDestination(c, "LFPG"));
    EXPECT_TRUE(checkOneDestination(c, "KLAX"));
}

TEST(ApiConstraint_Destination, Dests_WithNodests_MeansDeadCombination)
{
    JSON(c, R"({"nodests":["EG","EI"],"dests":["EGPF","EGPK"]})");
    EXPECT_FALSE(checkOneDestination(c, "EGPF"));
    EXPECT_FALSE(checkOneDestination(c, "EGPK"));
    EXPECT_FALSE(checkOneDestination(c, "EGLL"));
    EXPECT_FALSE(checkOneDestination(c, "EIDW"));
    EXPECT_FALSE(checkOneDestination(c, "LFPG"));
}

TEST(ApiConstraint_Destination, NeitherField_AlwaysPasses)
{
    JSON(c, R"({"dir":"even","min":255})");
    EXPECT_TRUE(checkOneDestination(c, "EGLL"));
    EXPECT_TRUE(checkOneDestination(c, "KLAX"));
}

TEST(ApiConstraint_ExitPoint, PointsMatch)
{
    JSON(c, R"({"points":["GOMUP"]})");
    EXPECT_TRUE(checkOneExitPoint(c, vec({"GOMUP"})));
    EXPECT_FALSE(checkOneExitPoint(c, vec({"BALIX"})));
    EXPECT_FALSE(checkOneExitPoint(c, vec({"ETILO", "BALIX"})));
}

TEST(ApiConstraint_ExitPoint, NoPoints_BlocksMatch)
{
    JSON(c, R"({"nopoints":["GOMUP"]})");
    EXPECT_FALSE(checkOneExitPoint(c, vec({"GOMUP"})));
    EXPECT_TRUE(checkOneExitPoint(c, vec({"BALIX"})));
}

TEST(ApiConstraint_Route, RouteMatch_Prefix)
{
    JSON(c, R"({"route":["EBOTO UN601 RIBEL"]})");
    EXPECT_TRUE(checkOneRoute(c, vec({"EBOTO", "UN601", "RIBEL", "LATLO"})));
    EXPECT_TRUE(checkOneRoute(c, vec({"EBOTO", "UN601", "RIBEL"})));
    EXPECT_FALSE(checkOneRoute(c, vec({"EBOTO", "DTY", "RIBEL"})));
    EXPECT_FALSE(checkOneRoute(c, vec({"BALIX", "UN601", "RIBEL"})));
}

TEST(ApiConstraint_Route, Wildcard_MatchesAny)
{
    JSON(c, R"({"route":["*"]})");
    EXPECT_TRUE(checkOneRoute(c, vec({"EBOTO", "UN601"})));
    EXPECT_TRUE(checkOneRoute(c, vec({})));
}

TEST(ApiConstraint_Route, NoRoute_Blocks)
{
    JSON(c, R"({"noroute":["EBOTO UN601"]})");
    EXPECT_FALSE(checkOneRoute(c, vec({"EBOTO", "UN601", "RIBEL"})));
    EXPECT_TRUE(checkOneRoute(c, vec({"EBOTO", "DTY", "RIBEL"})));
}

TEST(ApiConstraint_MinMax, MinAndMax_Band)
{
    JSON(c, R"({"min":105,"max":245})");
    EXPECT_TRUE(checkOneMinMax(c, 18000));
    EXPECT_FALSE(checkOneMinMax(c, 10000));
    EXPECT_FALSE(checkOneMinMax(c, 25000));
}

TEST(ApiConstraint_MinMax, ZeroMin_NoLowerLimit)
{
    JSON(c, R"({"min":0,"max":245})");
    EXPECT_TRUE(checkOneMinMax(c, 5000));
    EXPECT_FALSE(checkOneMinMax(c, 25000));
}

TEST(ApiConstraint_Direction, Even_RVSM)
{
    JSON(c, R"({"dir":"even"})");
    EXPECT_TRUE(checkOneDirection(c, 36000));
    EXPECT_TRUE(checkOneDirection(c, 32000));
    EXPECT_FALSE(checkOneDirection(c, 35000));
    EXPECT_FALSE(checkOneDirection(c, 29000));
}

TEST(ApiConstraint_Direction, Odd_RVSM)
{
    JSON(c, R"({"dir":"odd"})");
    EXPECT_TRUE(checkOneDirection(c, 35000));
    EXPECT_FALSE(checkOneDirection(c, 36000));
}

TEST(ApiConstraint_Alerts, BanAlert_ReturnsFalse)
{
    JSON(c, R"({
        "alerts":[{"srd":440,
                   "note":"Only available when pre-approved during events. Reroute at all other times.",
                   "warn":false,"ban":true}]
    })");
    bool warn = false;
    EXPECT_FALSE(checkOneAlerts(c, warn));
    EXPECT_FALSE(warn);
}

TEST(ApiConstraint_Alerts, WarnAlert_ReturnsTrueAndSetsWarn)
{
    JSON(c, R"({"alerts":[{"srd":489,"note":"Pilot discretion.","warn":true,"ban":false}]})");
    bool warn = false;
    EXPECT_TRUE(checkOneAlerts(c, warn));
    EXPECT_TRUE(warn);
}

TEST(ApiConstraint_Alerts, NoAlerts_AlwaysPasses)
{
    JSON(c, R"({"dir":"even","min":255})");
    bool warn = false;
    EXPECT_TRUE(checkOneAlerts(c, warn));
    EXPECT_FALSE(warn);
}
