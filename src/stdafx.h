#ifndef STDAFX_H	
#define STDAFX_H

#pragma once

#include "targetver.h"

#if !defined(_MSVC_LANG) || _MSVC_LANG < 202002L
#error VFPC requires C++20 or later.
#endif

#define WIN32_LEAN_AND_MEAN

#define NOMINMAX

#define SPDLOG_USE_STD_FORMAT				   // Tells the spdlog library to use C++20’s std::format for formatting log messages.

// --- Windows ---
#include <windows.h>

// --- C++ Standard ---
// string
#include <format>
#include <fmt/core.h>
#include <fmt/ranges.h>
#include <string>
#include <string_view>
#include <regex>
#include <sstream>
#include <fstream>
#include <iostream>

// threads and synchronization
#include <mutex>
#include <atomic>
#include <thread>
#include <future>

// container
#include <vector>
#include <set>
#include <queue>
#include <map>
#include <tuple>
#include <unordered_map>
#include <unordered_set>

// others
#include <optional>
#include <memory>
#include <chrono>
#include <filesystem>
#include <cctype>
#include <algorithm>

// --- EuroScope SDK ---
#include "EuroScopePlugIn.h"

// --- Third Party ---
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <fmt/format.h>
#include <curl/curl.h>

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#endif // STDAFX_H