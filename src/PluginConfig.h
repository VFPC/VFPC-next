#ifndef PLUGINCONFIG_H
#define PLUGINCONFIG_H

// --- C++ Standard ---
#include <string>
#include <vector>
#include <Windows.h>

struct PluginConfig {
    // spdlog
    std::string log_path;
    int max_files;
    int flush_interval_sec;
    std::string pattern;
    std::string level;
    std::string flush_level;
    bool truncate;

    // curl
    std::string base_url;
	std::string airport_endpoint;
	std::string version_endpoint;
    int timeout_sec;
    bool verify_ssl;
    std::string user_agent;
    std::string header_accept;
    std::string header_content_type;

    //Colours
    COLORREF TAG_GREEN = RGB(0, 190, 0);
    COLORREF TAG_YELLOW = RGB(241, 121, 0);
    COLORREF TAG_RED = RGB(190, 0, 0);
    COLORREF TAG_AMBER = RGB(255, 170, 0);
    COLORREF TAG_GREY = RGB(150, 150, 150);
	COLORREF TAG_CYAN = RGB(0, 255, 255);

	// Results view
    std::string console_results_view = "summary";

    // Non-fatal config warnings collected during load/defaulting
    std::vector<std::string> config_warnings;

};

bool InitialiseConfig(const std::string& filepath, PluginConfig& plugin_config);


#endif //PLUGINCONFIG_H
