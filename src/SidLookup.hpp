// SidLookup.hpp
// Pure, standalone functions for the out.json -> airport -> SID selection step.
//
// These mirror the logic in CVFPCPlugin::ExtractSingleAirportToObject_ and
// CVFPCPlugin::TryFindSidIndex_ (analyzeFP.cpp), extracted as free functions
// with no EuroScope, spdlog, or curl dependency.
//
// WHY THIS EXISTS
// The selection path in the live plugin (extracting an airport from the loaded
// document, then finding the SID whose "point" or "aliases" matches the flight's
// first waypoint) is entirely pure logic operating on plain RapidJSON types.
// Making those functions free-standing allows the harness to test the selection
// step from a standalone executable without instantiating CVFPCPlugin.
//
// LIMITATION: This header is a test-support mirror of logic that also lives in
// analyzeFP.cpp. If the selection behavior in analyzeFP.cpp is changed, this
// header must be kept in sync. Logging calls present in the original member
// functions are omitted here; the data logic is otherwise identical.

#pragma once

#include <cstddef>
#include <string>

#include "rapidjson/document.h"

namespace SidLookup {

// ============================================================
// Internal helpers (mirror the file-local helpers in analyzeFP.cpp)
// ============================================================
namespace detail {

    inline const rapidjson::Value* FindMemberValue(const rapidjson::Value& obj,
                                                    const char* name)
    {
        if (!obj.IsObject()) return nullptr;
        auto it = obj.FindMember(name);
        if (it == obj.MemberEnd()) return nullptr;
        return &it->value;
    }

    inline const rapidjson::Value* FindArrayMember(const rapidjson::Value& obj,
                                                    const char* name)
    {
        const auto* v = FindMemberValue(obj, name);
        return (v && v->IsArray()) ? v : nullptr;
    }

} // namespace detail

// ============================================================
// TryFindSidIndex
//
// Search the sids array for the entry whose "point" field (or any entry in its
// "aliases" array) matches first_wp. Sets pos to the zero-based index on match
// and returns true; sets pos to npos and returns false when no match is found.
//
// A SID entry that has no "constraints" array is treated as unusable and skipped,
// even when its "point" field matches — mirroring the behavior in
// CVFPCPlugin::TryFindSidIndex_ (analyzeFP.cpp lines 2033-2073).
// ============================================================
inline bool TryFindSidIndex(const rapidjson::Value& sids,
                             const std::string& first_wp,
                             std::size_t& pos)
{
    pos = std::string::npos;

    if (!sids.IsArray() || sids.Empty() || first_wp.empty()) {
        return false;
    }

    for (rapidjson::SizeType i = 0; i < sids.Size(); ++i) {
        const auto& sid = sids[i];
        if (!sid.IsObject()) {
            continue;
        }

        // SID must have a constraints array to be considered usable.
        if (!detail::FindArrayMember(sid, "constraints")) {
            continue;
        }

        // Primary match: exact "point" field.
        const auto* pointVal = detail::FindMemberValue(sid, "point");
        if (pointVal && pointVal->IsString() &&
            first_wp == pointVal->GetString()) {
            pos = i;
            return true;
        }

        // Secondary match: any entry in the "aliases" array.
        const auto* aliases = detail::FindArrayMember(sid, "aliases");
        if (!aliases) {
            continue;
        }

        for (rapidjson::SizeType j = 0; j < aliases->Size(); ++j) {
            const auto& alias = (*aliases)[j];
            if (alias.IsString() && first_wp == alias.GetString()) {
                pos = i;
                return true;
            }
        }
    }

    return false;
}

// ============================================================
// ExtractAirportFromDocument
//
// Copy the airport entry matching airport_icao out of the loaded JSON document.
// The document may be either a single-airport object (already extracted) or an
// array of airport objects (the full {cycle, airports:[...]} payload).
//
// Returns true and deep-copies the matched entry into out on success.
// Returns false when the ICAO is empty, not found, or the root type is unexpected.
//
// Mirrors CVFPCPlugin::ExtractSingleAirportToObject_ (analyzeFP.cpp lines
// 974-1033). Logging calls are omitted; data logic is otherwise identical.
// ============================================================
inline bool ExtractAirportFromDocument(const rapidjson::Document& in,
                                        const std::string& airport_icao,
                                        rapidjson::Document& out)
{
    out.SetObject();
    auto& alloc = out.GetAllocator();

    if (airport_icao.empty()) {
        return false;
    }

    // Case 1: root is a single-airport object.
    if (in.IsObject()) {
        const auto* icao = detail::FindMemberValue(in, "icao");
        if (!icao || !icao->IsString()) {
            return false;
        }
        if (airport_icao != icao->GetString()) {
            return false;
        }
        out.CopyFrom(in, alloc);
        return true;
    }

    // Case 2: root is an array of airport objects.
    if (in.IsArray()) {
        for (rapidjson::SizeType i = 0; i < in.Size(); ++i) {
            const auto& airport = in[i];
            if (!airport.IsObject()) {
                continue;
            }
            const auto* icao = detail::FindMemberValue(airport, "icao");
            if (!icao || !icao->IsString()) {
                continue;
            }
            if (airport_icao == icao->GetString()) {
                out.CopyFrom(airport, alloc);
                return true;
            }
        }
        return false;
    }

    // Root is neither object nor array.
    return false;
}

} // namespace SidLookup
