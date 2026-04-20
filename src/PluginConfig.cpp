#include "stdafx.h"
#include "PluginConfig.h"
#include "JsonConfigLoader.h"


// Initialization function that fills out the Config struct
bool InitialiseConfig(const std::string& filepath, PluginConfig& plugin_config) {

	JsonConfigLoader config;

    if (!config.load(filepath)) return false;
    if (!config.validate_and_apply_defaults()) return false;

	// --- Set up spdlog configuration ---
	plugin_config.log_path = config.LogDirectory();
	plugin_config.log_file = config.LogFilename();
	plugin_config.max_files = config.MaxFiles();
	plugin_config.flush_interval_sec = config.FlushIntervalSec();
	plugin_config.pattern = config.Pattern();
	plugin_config.level = config.Level();
	plugin_config.flush_level = config.FlushLevel();
	plugin_config.truncate = config.Truncate();

	// --- Set up curl configuration ---	
	plugin_config.base_url = config.BaseUrl();
	plugin_config.airport_endpoint = config.AirportEndpoint();
	plugin_config.version_endpoint = config.VersionEndpoint();
	plugin_config.timeout_sec = config.CurlTimeoutSec();
	plugin_config.verify_ssl = config.CurlVerifySSL();
	plugin_config.user_agent = config.CurlUserAgent();
	plugin_config.header_accept = config.DefaultHeader("Accept");
	plugin_config.header_content_type = config.DefaultHeader("Content-Type");

    
	JsonConfigLoader::ColourRGB red = config.Red();
	JsonConfigLoader::ColourRGB amber = config.Amber();
	JsonConfigLoader::ColourRGB green = config.Green();
	JsonConfigLoader::ColourRGB yellow = config.Yellow();
	JsonConfigLoader::ColourRGB grey = config.Grey();
	

	plugin_config.TAG_RED = RGB(red.r, red.g, red.b);
	plugin_config.TAG_AMBER = RGB(amber.r, amber.g, amber.b);
	plugin_config.TAG_GREEN = RGB(green.r, green.g, green.b);
	plugin_config.TAG_YELLOW = RGB(yellow.r, yellow.g, yellow.b);
	plugin_config.TAG_GREY = RGB(grey.r, grey.g, grey.b);

	plugin_config.console_results_view = config.ConsoleResultsView();
	plugin_config.config_warnings = config.warnings();

    return true;
}
