#include <iostream>
#include <stdexcept>
#include <string>

#include "rapidjson/document.h"

#include "../src/SidApplicability.hpp"

using rapidjson::Document;

namespace {

void Expect(bool condition, const std::string& message)
{
    if (!condition) {
        throw std::runtime_error(message);
    }
}

void RunCase(
    const char* name,
    bool expected,
    bool sid_restrictions_passed,
    const char* json,
    const std::string& engine_type,
    const std::string& aircraft_type,
    const std::string& sid_suffix,
    int day,
    int hour,
    int minute)
{
    Document doc;
    doc.Parse<0>(json);
    Expect(!doc.HasParseError(), std::string(name) + ": JSON parse failed");

    const bool actual = SidApplicability::ShouldEvaluateConstraints(
        sid_restrictions_passed,
        doc,
        engine_type,
        aircraft_type,
        sid_suffix,
        day,
        hour,
        minute);

    Expect(
        actual == expected,
        std::string(name) + ": expected " + (expected ? "true" : "false") +
        ", got " + (actual ? "true" : "false"));
}

} // namespace

int main()
{
    try {
        const char* egph_tla_sid_root = R"([
            {
                "types": ["T", "P", "E"],
                "alt": ["GOSAM"]
            }
        ])";

        RunCase(
            "egph_tla_jet_type_mismatch_allows_constraints",
            true,
            false,
            egph_tla_sid_root,
            "J",
            "B738",
            "",
            0,
            14,
            0);

        RunCase(
            "egph_tla_turboprop_does_not_bypass_sid_gate",
            false,
            false,
            egph_tla_sid_root,
            "T",
            "DH8D",
            "",
            0,
            14,
            0);

        const char* time_gated_sid = R"([
            {
                "types": ["J"],
                "start": {"date": 0, "time": "0800"},
                "end":   {"date": 4, "time": "1800"}
            }
        ])";

        RunCase(
            "time_gated_sid_outside_window_allows_constraints",
            true,
            false,
            time_gated_sid,
            "J",
            "B738",
            "",
            5,
            12,
            0);

        const char* banned_sid = R"([
            {
                "types": ["J"],
                "start": {"date": 0, "time": "0800"},
                "end":   {"date": 4, "time": "1800"},
                "banned": true
            }
        ])";

        RunCase(
            "active_banned_sid_keeps_rst_path",
            false,
            false,
            banned_sid,
            "J",
            "B738",
            "",
            0,
            9,
            0);

        const char* suffix_restricted_sid = R"([
            {
                "suffix": ["Y"]
            }
        ])";

        RunCase(
            "suffix_mismatch_allows_constraints",
            true,
            false,
            suffix_restricted_sid,
            "J",
            "B738",
            "1C",
            0,
            12,
            0);

        std::cout << "SidApplicabilityRegression: PASS" << std::endl;
        return 0;
    }
    catch (const std::exception& ex) {
        std::cerr << "SidApplicabilityRegression: FAIL: " << ex.what() << std::endl;
        return 1;
    }
}
