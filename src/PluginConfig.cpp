#include "stdafx.h"
#include "Log.h"
#include "PluginConfig.h"
#include "JsonConfigLoader.h"


// Initialization function that fills out the Config struct
bool InitialiseConfig(const std::string& filepath, PluginConfig& plugin_config) {

	JsonConfigLoader config;

    if (!config.load(filepath)) return false;
    if (!config.validate_and_apply_defaults()) return false;

	// --- Set up spdlog configuration ---
	plugin_config.log_path = config.LogDirectory();
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
	JsonConfigLoader::ColourRGB cyan = config.Cyan();
	

	plugin_config.TAG_RED = RGB(red.r, red.g, red.b);
	plugin_config.TAG_AMBER = RGB(amber.r, amber.g, amber.b);
	plugin_config.TAG_GREEN = RGB(green.r, green.g, green.b);
	plugin_config.TAG_YELLOW = RGB(yellow.r, yellow.g, yellow.b);
	plugin_config.TAG_GREY = RGB(grey.r, grey.g, grey.b);
	plugin_config.TAG_CYAN = RGB(cyan.r, cyan.g, cyan.b);

	plugin_config.config_warnings = config.warnings();

	plugin_config.defaulted_settings_count =
		config.DefaultedSettingsCount();

	plugin_config.unknown_keys_count =
		config.UnknownKeysCount();

    return true;
}

void LogConfiguration(const PluginConfig& pc)
{
	LOG_INFO("Configuration loaded:");

	LOG_INFO("SPDLOG:");

	LOG_INFO("  log_directory        = {}", pc.log_path);
	LOG_INFO("  max_files            = {}", pc.max_files);
	LOG_INFO("  flush_interval_sec   = {}", pc.flush_interval_sec);
	LOG_INFO("  pattern              = {}", pc.pattern);
	LOG_INFO("  level                = {}", pc.level);
	LOG_INFO("  flush_level          = {}", pc.flush_level);
	LOG_INFO("  truncate             = {}", pc.truncate);

	LOG_INFO("CURL:");

	LOG_INFO("  base_url             = {}", pc.base_url);
	LOG_INFO("  airport_endpoint     = {}", pc.airport_endpoint);
	LOG_INFO("  version_endpoint     = {}", pc.version_endpoint);
	LOG_INFO("  timeout_sec          = {}", pc.timeout_sec);
	LOG_INFO("  verify_ssl           = {}", pc.verify_ssl);
	LOG_INFO("  user_agent           = {}", pc.user_agent);
	LOG_INFO("  Accept               = {}", pc.header_accept);
	LOG_INFO("  Content-Type         = {}", pc.header_content_type);

	if (!pc.config_warnings.empty()) {
		LOG_WARN("{} configuration setting(s) defaulted:",
			pc.config_warnings.size());

		for (const auto& warning : pc.config_warnings) {
			LOG_WARN("  {}", warning);
		}
	}
}