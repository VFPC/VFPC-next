//==============================================================
// 1. Includes / globals / file-local state
//==============================================================
#include "stdafx.h"
#include "analyzeFP.hpp"
#include "JsonConfigLoader.h"
#include "PluginConfig.h"
#include "Utils.h"
#include "Log.h"
#include "UrlData.h"
#include "VersionInfo.h"
#include "TimeWindow.h"

extern "C" IMAGE_DOS_HEADER __ImageBase;

//==============================================================
// 2. File-local utility helpers
//==============================================================
namespace
{
	//bool debugMode = true;
	bool autoLoad = false;
	bool fileLoad = false;
	bool apiUpdated = false;
	bool is_initialised = false;

	std::mutex checks_disabled_mtx_;
	std::unordered_set<std::string> checks_disabled_callsigns_;

	std::mutex controller_override_mtx_;
	std::unordered_set<std::string> controller_override_callsigns_;

	std::mutex skipped_airport_log_mtx_;
	std::unordered_set<std::string> skipped_airport_logged_;

	int relCount = 0;

	PluginConfig plugin_config_;

	vector<int> timedata;
	vector<int> lastupdate;

	/***********************************************************
	* The following are used with the state machine to track the
	* state of the plugin's connection to the API and whether
	* it has a callsign ready to check flight plans for.
	***********************************************************/
	enum class SessionState {
		Disconnected,
		Connected_NoCallsign,
		Connected_CallsignReady
	};

	SessionState session_state_{ SessionState::Disconnected };

	struct CurlStopCtx {
		std::stop_token st;
	};

	static int CurlXferInfo(void* clientp,
		curl_off_t /*dltotal*/, curl_off_t /*dlnow*/,
		curl_off_t /*ultotal*/, curl_off_t /*ulnow*/)
	{
		auto* ctx = static_cast<CurlStopCtx*>(clientp);
		return ctx->st.stop_requested() ? 1 : 0; // non-zero aborts transfer
	}

	//Stores output of HTTP request in string
	static size_t curlCallback(void* contents, size_t size, size_t
		nmemb, void* outString)
	{
		static_cast<std::string*>(outString)->append(
			reinterpret_cast<char*>(contents), size * nmemb);
		return size * nmemb;
	}

	//Gets path to current directory
	static std::string GetPath()
	{
		char DllPathFile[_MAX_PATH];
		GetModuleFileNameA(HINSTANCE(&__ImageBase), DllPathFile,
			sizeof(DllPathFile));
		std::string path = DllPathFile;
		path.resize(path.size() - strlen(PLUGIN_FILE.c_str()));
		return path;
	}

	const rapidjson::Value* FindMemberValue(const rapidjson::Value& obj, const char* name)
	{
		if (!obj.IsObject()) return nullptr;
		auto it = obj.FindMember(name);
		if (it == obj.MemberEnd()) return nullptr;
		return &it->value;
	}

	const rapidjson::Value* FindArrayMember(const rapidjson::Value& obj, const char* name)
	{
		const auto* v = FindMemberValue(obj, name);
		return (v && v->IsArray()) ? v : nullptr;
	}

	static bool GetIntMember(const rapidjson::Value& obj, const char* name, int& out)
	{
		const auto* v = FindMemberValue(obj, name);
		if (!v || !v->IsInt()) return false;
		out = v->GetInt();
		return true;
	}

	std::string JoinStringArray(const rapidjson::Value* arr, const std::string& sep)
	{
		if (!arr || !arr->IsArray()) return "";
		std::string out;
		bool first = true;
		for (const auto& item : arr->GetArray()) {
			if (!item.IsString()) continue;
			if (!first) out += sep;
			out += item.GetString();
			first = false;
		}
		return out;
	}

	bool ArrayContainsStringPrefixMatch(const rapidjson::Value& arr, const std::string& value)
	{
		if (!arr.IsArray()) return false;
		for (const auto& item : arr.GetArray()) {
			if (item.IsString() && startsWith(item.GetString(),
				value.c_str())) {
				return true;
			}
		}
		return false;
	}

	bool LooksLikeSpeedLevelSuffix_(std::string_view s)
	{
		// Accept common ICAO change annotations after "/"
		// Examples:
		//   /F350
		//   /N0450F350
		//   /M082F330
		//   /A080
		//   /K0450S1130
		//
		// We deliberately keep this permissive:
		// if it begins with a valid speed/level designator, treat it as
		// a suffix to strip for validation purposes.

		if (s.empty()) {
			return false;
		}

		switch (s.front()) {
		case 'N': // knots true airspeed
		case 'K': // km/h
		case 'M': // mach
		case 'F': // flight level
		case 'A': // altitude 100s ft
		case 'S': // metric level
			return true;
		default:
			return false;
		}
	}

	std::string StripRouteTokenSuffixForValidation_(std::string token)
	{
		const auto slash = token.find('/');
		if (slash == std::string::npos) {
			return token;
		}

		// Only strip when the part after "/" looks like speed/level data.
		// This avoids removing "/" from anything unexpected.
		const std::string_view suffix(token.c_str() + slash + 1, token.size() - slash - 1);
		if (!LooksLikeSpeedLevelSuffix_(suffix)) {
			return token;
		}

		token.erase(slash);
		return token;
	}

	std::vector<std::string> BuildValidationRoutePoints_(
		const std::vector<std::string>& route_points)
	{
		std::vector<std::string> out;
		out.reserve(route_points.size());

		for (const auto& raw : route_points) {
			if (raw.empty()) {
				continue;
			}

			std::string cleaned = StripRouteTokenSuffixForValidation_(raw);
			if (!cleaned.empty()) {
				out.push_back(std::move(cleaned));
			}
		}

		return out;
	}

	std::string TruncateForDisplay_(const std::string& text, std::size_t max_len)
	{
		if (text.size() <= max_len) {
			return text;
		}

		if (max_len <= 3) {
			return text.substr(0, max_len);
		}

		return text.substr(0, max_len - 3) + "...";
	}

	std::string ExtractPbnFromRemarks(std::string remarks)
	{
		std::transform(remarks.begin(), remarks.end(), remarks.begin(),
			[](unsigned char c)
			{
				return static_cast<char>(std::toupper(c));
			});

		constexpr std::string_view key = "PBN/";

		const size_t pos = remarks.find(key);

		if (pos == std::string::npos)
			return {};

		const size_t start = pos + key.size();

		size_t end = remarks.find(' ', start);

		if (end == std::string::npos)
			end = remarks.size();

		return remarks.substr(start, end - start);
	}

	bool HasRnav1FromPbn(const std::string& pbn)
	{
		return pbn.find("D1") != std::string::npos ||
			pbn.find("D2") != std::string::npos ||
			pbn.find("D3") != std::string::npos ||
			pbn.find("D4") != std::string::npos;
	}

} // end namespace


bool CVFPCPlugin::IsAirwayToken_(const std::string& token)
{
	// Allow common ATS route designators:
	// L151, UL151, N859, UN859, M605, UM605, Q63, UQ63, etc.
	static const std::regex airway_re(R"(^U?[A-Z]{1,2}\d{1,4}[A-Z]?$)");
	return std::regex_match(token, airway_re);
}

std::string CVFPCPlugin::StripUpperAirwayPrefix_(const std::string& token)
{
	if (token.size() >= 2 && token[0] == 'U' && std::isalpha(static_cast<unsigned char>(token[1]))) {
		return token.substr(1);
	}

	return token;
}

bool CVFPCPlugin::IsUpperLowerAirwayEquivalent_(
	const std::string& expected,
	const std::string& filed)
{
	if (expected == filed) {
		return false; // exact match, not a warning case
	}

	if (!IsAirwayToken_(expected) || !IsAirwayToken_(filed)) {
		return false;
	}

	return StripUpperAirwayPrefix_(expected) ==
		StripUpperAirwayPrefix_(filed);
}

bool CVFPCPlugin::RouteTokenMatchesStrictOrAirwayEquivalent_(
	const std::string& expected,
	const std::string& filed,
	std::vector<std::string>* warnings)
{
	if (expected == filed) {
		return true;
	}

	if (IsUpperLowerAirwayEquivalent_(expected, filed)) {
		if (warnings) {
			warnings->push_back(
				fmt::format("Airway level mismatch: filed {}, expected {}.",
					filed, expected));
		}

		return true;
	}

	return false;
}

//==============================================================
// 3. Construction / destruction / basic console helpers
//==============================================================
CVFPCPlugin::CVFPCPlugin(void) :CPlugIn(EuroScopePlugIn::COMPATIBILITY_CODE,
	VFPC_PLUGIN_NAME,
	VFPC_VERSION_STR,
	VFPC_COMPANY_NAME,
	VFPC_COPYRIGHT_TEXT)
{
	validVersion = true; //Reset in first timer call
	autoLoad = true;
	fileLoad = false; // Reset counters
	relCount = 0;
	timedata = { 0, 0, 0, 0, 0, 0 }; // 0 = Year, 1 = Month, 2 = Day, 3 = Hour, 4 = Minute, 5 = Day of Week
	lastupdate = { 0, 0, 0, 0, 0 }; // 0 = Year, 1 = Month, 2 = Day, 3 = Hour, 4 = Minute

	// Register TAG and Function Menu.
	RegisterTagItemType("VFPC", TAG_ITEM_CHECKFP);
	RegisterTagItemFunction("Options", TAG_FUNC_CHECKFP_MENU);

	vector<string> installed = split(VFPC_VERSION_STR, '.');

	thisVersion = (int*)calloc(installed.size(), sizeof(int));
	for (size_t i = 0; i < installed.size(); i++) {
		thisVersion[i] = stoi(installed[i]);
	}

	std::string path = GetPath();
	if (!InitialiseConfig(path + kConfigFileName, plugin_config_)) {
		SendToConsole(vfpc::urgent, "Configuration file error.");
		return;
	}

	std::string results_view = to_upper(plugin_config_.console_results_view);
	if (results_view == "FULL") {
		console_results_view_ = ResultsView::Full;
	}
	else {
		console_results_view_ = ResultsView::Summary;
	}

	if (!plugin_config_.config_warnings.empty()) {
		SendToConsole(vfpc::urgent,
			"Config warnings : {} setting(s) defaulted.  Use '.vfpc config' for details.",
			plugin_config_.config_warnings.size());
	}

	if (!InitializeLogging(plugin_config_)) {
		SendToConsole(vfpc::urgent, "Error initializing logging, logging disabled.");
		return;
	}

	// Start async version check (non-blocking)
	StartVersionCheckAsync();

	SendToConsole("Plugin Version: {} Loaded. {}", VFPC_DISPLAY_VERSION, PLUGIN_WELCOME_MESSAGE);

	is_initialised = true;
	return;
}

CVFPCPlugin::~CVFPCPlugin()
{
	LOG_DEBUG("Stopping version thread ....");
	if (version_thread_.joinable())
		version_thread_.request_stop();

	LOG_DEBUG("Stopping airport worker thread ....");

	if (airport_thread_.joinable())
		airport_thread_.request_stop();

	// Release published snapshot before shutdown (helps DLL unload determinism)
	airport_data_.store(nullptr, std::memory_order_release);

	LOG_INFO("Plugin logging shutting down ....");
	spdlog::shutdown();
}

void CVFPCPlugin::SendToConsole(const char* msg, bool urgent)
{
	DisplayUserMessage("VFPC", VFPC_LOGGER_NAME, msg,
		false, true, urgent, urgent,
		false);
}

void CVFPCPlugin::ApplyTagDisplayFromResult_(const ValidationResult& result,
	char sItemString[16],
	int* pColorCode,
	COLORREF* pRGB) const
{
	const std::string item = result.itemString.empty() ? "ERR" : result.itemString;
	strncpy_s(sItemString, 16, item.c_str(), _TRUNCATE);

	// Tell EuroScope to use the RGB colour, if required by the SDK.
	if (pColorCode) {
		*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED; // use the correct SDK	constant here
	}
	if (!pRGB) {
		return;
	}
	// Set color in departure list by mnemonic.
	if (item == "OK!") {
		*pRGB = plugin_config_.TAG_GREEN;
		return;
	}
	if (item == "VFR" || item == "OFF") {
		*pRGB = plugin_config_.TAG_GREY;
		return;
	}
	if (item == "WRN" || item == "PDG") {
		*pRGB = plugin_config_.TAG_YELLOW;

		return;
	}
	if (item == "RST") {
		*pRGB = (result.status == ValidationStatus::Failed)
			? plugin_config_.TAG_RED
			: plugin_config_.TAG_AMBER;
		return;
	}
	if (item == "LVL" || item == "DIR" || item == "SFX") {
		*pRGB = plugin_config_.TAG_AMBER;
		return;
	}
	if (item == "BAN" || item == "SYN" || item == "SID" || item ==
		"DST" || item == "XPT" || item == "RTE") {
		*pRGB = plugin_config_.TAG_RED;
		return;
	}
	else {
		*pRGB = RGB(255, 255, 255); // Default to white for unknown statuses
	}
}

void CVFPCPlugin::OutputValidationResultToConsole_(const ValidationResult& result, 
	ResultsView view)
{
	auto text = [&](ValidationField field) -> const std::string& {
		return (view == ResultsView::Full)
			? result.fields[field].full
			: result.fields[field].summary;
		};

	if (view == ResultsView::Full) {
		SendToConsole("{} {}", text(VF_CALLSIGN), result.itemString);
		SendToConsole("SID: {}", text(VF_SID));
		SendToConsole("Destination: {}", text(VF_DESTINATION));
		SendToConsole("Exit Point: {}", text(VF_EXIT_POINT));
		SendToConsole("Route: {}", text(VF_ROUTE));
		SendToConsole("Level: {}", text(VF_MIN_MAX));
		SendToConsole("Odd/Even: {}", text(VF_ODD_EVEN));
		SendToConsole("Suffix: {}", text(VF_SUFFIX));
		SendToConsole("Restrictions: {}", text(VF_RESTRICTIONS));
		SendToConsole("Warnings: {}", text(VF_WARNINGS));
		SendToConsole("Bans: {}", text(VF_BANS));
		SendToConsole("Syntax: {}", text(VF_SYNTAX));
		SendToConsole("Status: {}", text(VF_STATUS));
		return;
	}

	const std::string callsign =
		!result.fields[VF_CALLSIGN].summary.empty()
		? result.fields[VF_CALLSIGN].summary
		: "UNKNOWN";

	SendToConsole("{} {}", callsign, result.itemString);

	switch (result.status) {
	case ValidationStatus::Passed:
		if (FieldNeedsAttention_(result, VF_WARNINGS)) {
			SendToConsole("Warning: {}", SummaryDetail_(result, VF_WARNINGS));
		}
		else {
			SendToConsole("All checks passed");
		}
		break;

	case ValidationStatus::Failed:
	{
		const std::vector<std::pair<ValidationField, std::string>> priority = {
			{ VF_SYNTAX,       "Syntax" },
			{ VF_BANS,         "Ban" },
			{ VF_SID,          "SID" },
			{ VF_DESTINATION,  "Destination" },
			{ VF_EXIT_POINT,   "Exit Point" },
			{ VF_ROUTE,        "Route" },
			{ VF_MIN_MAX,      "Level" },
			{ VF_ODD_EVEN,     "Odd/Even" },
			{ VF_SUFFIX,       "Suffix" },
			{ VF_RESTRICTIONS, "Restrictions" },
			{ VF_WARNINGS,     "Warning" }
		};

		for (const auto& [field, label] : priority) {
			const std::string detail = SummaryDetail_(result, field);
			if (!detail.empty()) {
				SendToConsole("{}: {}", label, detail);
				return;
			}
		}

		SendToConsole(vfpc::urgent,"FAIL: Validation error");
		break;
	}

	case ValidationStatus::Pending:
		SendToConsole("Validation pending");
		SendToConsole("Airport data loading");
		break;

	case ValidationStatus::NotChecked:
		SendToConsole("Not checked");
		SendToConsole("Flight plan is VFR");
		break;

	default:
		SendToConsole(vfpc::urgent, "Validation error");
		break;
	}
}

std::string CVFPCPlugin::CompactFieldName_(ValidationField field) const
{
	switch (field) {
	case VF_SID: return "SID";
	case VF_DESTINATION: return "DEST";
	case VF_EXIT_POINT: return "EXIT";
	case VF_ROUTE: return "RTE";
	case VF_MIN_MAX: return "LVL";
	case VF_ODD_EVEN: return "DIR";
	case VF_SUFFIX: return "SFX";
	case VF_RESTRICTIONS: return "RSTR";
	case VF_WARNINGS: return "WARN";
	case VF_BANS: return "BAN";
	case VF_SYNTAX: return "SYN";
	default: return "";
	}
}

bool CVFPCPlugin::FieldNeedsAttention_(const ValidationResult& result, ValidationField field) const
{
	const std::string& s = result.fields[field].summary;
	return !s.empty() &&
		s != "-" &&
		s != "OK" &&
		s != "Passed" &&
		s != "No Warnings." &&
		s != "No Route Ban." &&
		s != "Not Checked";
}

std::string CVFPCPlugin::TagMnemonicFromResult_(const ValidationResult& result) const
{
	if (result.status == ValidationStatus::Pending) {
		return "PDG";
	}
	if (result.status == ValidationStatus::NotChecked) {
		return "VFR";
	}
	// Priority order: most important / most actionable first
	if (FieldNeedsAttention_(result, VF_SYNTAX)) return "SYN";
	if (FieldNeedsAttention_(result, VF_BANS)) return "BAN";
	if (FieldNeedsAttention_(result, VF_SID)) return "SID";
	if (FieldNeedsAttention_(result, VF_DESTINATION)) return "DST";
	if (FieldNeedsAttention_(result, VF_EXIT_POINT)) return "XIT";
	if (FieldNeedsAttention_(result, VF_ROUTE)) return "RTE";
	if (FieldNeedsAttention_(result, VF_MIN_MAX)) return "LVL";
	if (FieldNeedsAttention_(result, VF_ODD_EVEN)) return "DIR";
	if (FieldNeedsAttention_(result, VF_SUFFIX)) return "SFX";
	if (FieldNeedsAttention_(result, VF_RESTRICTIONS)) return "RST";
	// Warning only, no hard failure
	if (FieldNeedsAttention_(result, VF_WARNINGS)) return "WRN";
	return "OK!";
}

bool CVFPCPlugin::ConstraintAllowsDestination_(const rapidjson::Value& cond,
	const std::string& destination) const
{
	if (!cond.IsObject()) {
		return false;
	}

	const auto* dests = FindArrayMember(cond, "dests");
	if (dests && !dests->Empty()) {
		if (destArrayContains(*dests, destination).empty()) {
			return false;
		}
	}

	const auto* noDests = FindArrayMember(cond, "nodests");
	if (noDests && !noDests->Empty()) {
		if (!destArrayContains(*noDests, destination).empty()) {
			return false;
		}
	}

	return true;
}

bool CVFPCPlugin::ConstraintAllowsLevel_(const rapidjson::Value& cond,
	int rfl) const
{
	if (!cond.IsObject()) {
		return false;
	}

	const int flight_level = rfl / 100;

	int min_level = 0;
	if (GetIntMember(cond, "min", min_level)) {
		if (flight_level < min_level) {
			return false;
		}
	}

	int max_level = 0;
	if (GetIntMember(cond, "max", max_level)) {
		if (flight_level > max_level) {
			return false;
		}
	}

	return true;
}

//==============================================================
// 4. HTTP / JSON / file loading
//==============================================================
bool CVFPCPlugin::webCall(string url, string& out, std::stop_token st)
{
	out.clear();

	LOG_TRACE("Web Call: Initiating call to {}", url);

	CURL* curl = curl_easy_init();
	if (!curl) {
		LOG_ERROR("Web Call: curl_easy_init() failed");
		return false;
	}

	char errbuf[CURL_ERROR_SIZE]{};
	curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, errbuf);

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());

	// Timeouts: split connect vs total
	curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
	curl_easy_setopt(curl, CURLOPT_TIMEOUT, 15L);

	// Windows/thread friendliness
	curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

	// Follow redirects (common with https / CDNs)
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);

	// Write response
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curlCallback);

	// Optional but good practice: identify yourself
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "VFPC/3.x (EuroScope)");

	// Cancellation hook
	CurlStopCtx stopCtx{ st };
	curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, CurlXferInfo);
	curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &stopCtx);
	curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);

	// Perform
	CURLcode result = curl_easy_perform(curl);

	long httpCode = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);

	char* contentType = nullptr;
	curl_easy_getinfo(curl, CURLINFO_CONTENT_TYPE, &contentType);
	std::string contentTypeStr = contentType ? contentType : "(null)";

	curl_easy_cleanup(curl);

	LOG_TRACE("Web Call: HTTP {} Content-Type={} bytes={}",
		httpCode, contentTypeStr, out.size());

	if (st.stop_requested()) {
		LOG_INFO("Web Call: Cancelled call to {}", url);
		return false;
	}

	if (result != CURLE_OK) {
		std::string detail = (errbuf[0] != '\0') ? errbuf :
			curl_easy_strerror(result);
		LOG_ERROR("Web Call: Call To {} Failed - CURL Error: {} (HTTP Code: {})",
			url, detail, httpCode);
		return false;
	}

	// Treat non-2xx as failure (your choice; but usually correct for JSON fetch)
	if (httpCode < 200 || httpCode >= 300) {
		LOG_DEBUG("Web Call: Call To {} HTTP: {} Body prefix: {}",
			url, std::to_string(httpCode), out.substr(0, 200));
		return false;
	}

	return true;
}

bool CVFPCPlugin::APICall(const std::string& base_url,
	const std::string& endpoint,
	std::stop_token st,
	rapidjson::Document& out)
{

	std::string url = base_url + endpoint;
	std::string buf;

	// Always reset to a safe value first
	out.SetArray();

	LOG_TRACE("API Call: Calling url: {} endpoint {}...",
		url, endpoint);

	if (!webCall(url, buf, st))
	{
		SendToConsole(vfpc::urgent, "An error occurred whilst downloading data.The plugin has been disabled.");
		SendToConsole(vfpc::urgent, "Please restart EuroScope to try again. (Note:.vfpc load will NOT work.");

		LOG_ERROR("API Call to{} : Failed - No Data Returned", url);

		return false;
	}

	if (st.stop_requested()) {
		LOG_TRACE("API Call To {}: Cancelled before JSON parsing", url);
		return false;
	}

	LOG_TRACE("API Call To {}: Data Retrieved ({} bytes), Parsing JSON...", url, buf.size());

	out.Parse(buf.data(), buf.size());

	if (out.HasParseError()) {
		SendToConsole(vfpc::urgent,
			"An error occurred whilst reading data. The plugin will not automatically attempt to reload from the API. "
			"To restart data fetching, type \".vfpc load\".");

		LOG_ERROR("API Call To {}: JSON parse error: {} (Offset: {})",
			url, rapidjson::GetParseError_En(out.GetParseError()), out.GetErrorOffset());

		LOG_DEBUG("API Call To {}: Body prefix: {}", url, buf.substr(0, 200));

		// Ensure 'out' is safe even after a parse failure
		out.SetArray();
		return false;
	}

	LOG_TRACE("API Call: JSON parsed successfully (root type={})", (int)out.GetType());
	return true;
}

bool CVFPCPlugin::fileCall(rapidjson::Document& out)
{
	std::stringstream ss;
	std::ifstream ifs;

	std::string path = GetPath();
	path += DATA_FILE;
	LOG_TRACE("Opening File: {}", DATA_FILE);

	ifs.open(path.c_str(), ios::binary);
	if (ifs.is_open()) {
		ss << ifs.rdbuf();
		ifs.close();
		if (out.Parse<0>(ss.str().c_str()).HasParseError()) {
			SendToConsole(vfpc::urgent, "An error occurred whilst parsing data. The plugin will not automatically attempt to reload.");
			SendToConsole(vfpc::urgent, "To restart data fetching from the API, type '{}{}'", COMMAND_PREFIX, LOAD_COMMAND);
			SendToConsole(vfpc::urgent, "To load data from sid.json, type '{}{}'", COMMAND_PREFIX, FILE_COMMAND);

			LOG_DEBUG("Config Parse: {} (Offset: {})\n'", rapidjson::GetParseError_En(out.GetParseError()),
				out.GetErrorOffset());

			LOG_ERROR("File Data Parse Failed - Data Found But Unreadable");

			out.Parse<0>("[]");
			return false;
		}
	}
	else {
		SendToConsole(vfpc::urgent, "File {} not found. To restart data fetching from the API, type '{}{}'", 
			path, COMMAND_PREFIX, LOAD_COMMAND);

		LOG_WARN("File not found {}", DATA_FILE);

		out.Parse<0>("[]");
		return false;
	}
	return true;
}

std::string CVFPCPlugin::BuildShortRouteExample_(const rapidjson::Value& cond) const
{
	if (!cond.IsObject()) {
		return {};
	}

	const auto* routeArr = FindArrayMember(cond, "route");
	if (!routeArr || routeArr->Empty()) {
		return {};
	}

	std::string out;
	for (rapidjson::SizeType i = 0; i < routeArr->Size(); ++i) {
		const auto& item = (*routeArr)[i];
		if (!item.IsString()) {
			continue;
		}

		if (!out.empty()) {
			out += ' ';
		}
		out += item.GetString();
	}

	return out;
}

std::vector<std::string> CVFPCPlugin::CollectRouteExamples_(
	const rapidjson::Value& constraints,
	const std::string& destination,
	int rfl,
	const std::vector<std::string>& route_points) const
{
	std::vector<std::string> examples;

	if (!constraints.IsArray() || constraints.Empty()) {
		return examples;
	}

	auto routeContainsPoint = [&](const rapidjson::Value& cond) {
		const auto* points = FindArrayMember(cond, "points");
		if (!points || points->Empty()) {
			return false;
		}

		for (const auto& p : points->GetArray()) {
			if (!p.IsString()) continue;

			if (std::find(route_points.begin(), route_points.end(),
				std::string(p.GetString())) != route_points.end()) {
				return true;
			}
		}

		return false;
		};

	auto addMatchingExamples = [&](auto predicate) {
		for (rapidjson::SizeType i = 0; i < constraints.Size(); ++i) {
			const auto& cond = constraints[i];

			if (!cond.IsObject()) continue;
			if (!predicate(cond)) continue;

			std::string example = BuildShortRouteExample_(cond);
			if (example.empty()) continue;

			if (std::find(examples.begin(), examples.end(), example) == examples.end()) {
				examples.push_back(std::move(example));
			}
		}
		};

	// Best: matching destination + level + filed exit/point.
	addMatchingExamples([&](const rapidjson::Value& cond) {
		return ConstraintAllowsDestination_(cond, destination)
			&& ConstraintAllowsLevel_(cond, rfl)
			&& routeContainsPoint(cond);
		});

	// Next best: matching route point + level.
	if (examples.empty()) {
		addMatchingExamples([&](const rapidjson::Value& cond) {
			return ConstraintAllowsLevel_(cond, rfl)
				&& routeContainsPoint(cond);
			});
	}

	// Existing behaviour: destination + level.
	if (examples.empty()) {
		addMatchingExamples([&](const rapidjson::Value& cond) {
			return ConstraintAllowsDestination_(cond, destination)
				&& ConstraintAllowsLevel_(cond, rfl);
			});
	}

	// Final fallback.
	if (examples.empty()) {
		addMatchingExamples([&](const rapidjson::Value&) {
			return true;
			});
	}

	return examples;
}

//==============================================================
// 5. Version-check worker
//==============================================================
void CVFPCPlugin::StartVersionCheckAsync()
{
	LOG_TRACE("Starting asynchronous version check...");

	// Reset state
	version_checked.store(false, std::memory_order_release);
	validVersion.store(false, std::memory_order_release);

	// Snapshot config so worker doesn’t read mutable members
	const PluginConfig plugin_config_copy = plugin_config_;

	// Start jthread; stop_token is provided automatically
	version_thread_ = std::jthread([this, plugin_config_copy]
	(std::stop_token st) mutable
		{
			LOG_DEBUG("Version thread: entered worker");
			const bool ok = VersionCall_Worker(plugin_config_copy, st);
			LOG_DEBUG("Version thread: worker returned {}", ok);
			validVersion.store(ok, std::memory_order_release);
			version_checked.store(true, std::memory_order_release);
		});
}

bool CVFPCPlugin::VersionCall_Worker(PluginConfig plugin_config, std::stop_token st)
{
	rapidjson::Document version;

	LOG_TRACE("VersionCall_Worker_: running. stop_requested={}", st.stop_requested());

	if (!APICall(plugin_config.base_url,
		plugin_config.version_endpoint,
		st, version)) return false;

	if (st.stop_requested()) return false;

	bool out = false;

	const auto* vfpcVersion = FindMemberValue(version, "vfpc_version");
	const auto* minVersionVal = FindMemberValue(version, "min_version");

	if (vfpcVersion && vfpcVersion->IsString() &&
		minVersionVal && minVersionVal->IsString()) {
		vector<string> minver = split(version["min_version"].GetString(), '.');
		bool minchange = false;
		vector<string> curver = split(version["vfpc_version"].GetString(), '.');
		bool curchange = false;
		bool check = true;

		for (size_t i = 0; i < minver.size(); i++) {
			int temp = stoi(minver[i]);
			if (i < minVersion.size()) {
				if (check && temp > minVersion[i]) {
					minchange = true;
				}
				else if (temp != minVersion[i]) {
					check = false;
				}
				minVersion[i] = temp;
			}
			else {
				minchange = true;
				minVersion.push_back(temp);
			}
		}
		if (minVersion.size() > minver.size()) {
			minVersion.resize(minver.size());
		}

		for (size_t i = 0; i < curver.size(); i++) {

			int temp = stoi(curver[i]);
			if (i < curVersion.size()) {

				if (check && temp > curVersion[i]) {
					curchange = true;
				}
				else if (temp != curVersion[i]) {
					check = false;
				}
				curVersion[i] = temp;
			}
			else {
				curchange = true;
				curVersion.push_back(temp);
			}
		}
		if (curVersion.size() > curver.size()) {
			curVersion.resize(curver.size());
		}

		if (minVersion[0] > thisVersion[0] || (minVersion[0] ==
			thisVersion[0] && minVersion[1] > thisVersion[1]) ||
			(minVersion[0] == thisVersion[0] && minVersion[1] ==
				thisVersion[1] && minVersion[2] > thisVersion[2])) {

			LOG_ERROR("Version Call: Discontinued Version In Use. Please update plugin.");
			if (minchange) {

				SendToConsole(vfpc::urgent, "Plugin Update required - the plugin has been disabled.");
			}
		}
		else if (curVersion[0] > thisVersion[0] || (curVersion[0] ==
			thisVersion[0] && curVersion[1] > thisVersion[1]) ||
			(curVersion[0] == thisVersion[0] && curVersion[1] ==
				thisVersion[1] && curVersion[2] > thisVersion[2])) {

			LOG_INFO("Version Call: Outdated Version In Use");
			if (curchange) {

				SendToConsole(vfpc::urgent,"Update available - you may continue using the plugin, but please update as soon as possible.");
			}
			out = true;
		}
		else {
			LOG_DEBUG("Version Call: No New Version Since Last Check");
			out = true;
		}
	}
	else {
		LOG_ERROR("Version Call: Plugin Version Data Not Found");
		SendToConsole(vfpc::urgent, "Failed to check for updates - the plugin has been disabled.");
	}

	const auto* airacCycle = FindMemberValue(version, "cycle");
	const auto* airacDate = FindMemberValue(version, "date");

	if (airacCycle && airacCycle->IsString() &&
		airacDate && airacDate->IsString()) {

		std::string currentAirac = airacCycle->GetString();
		std::string currentAiracDate = airacDate->GetString();
		LOG_INFO("Version Call: Current AIRAC Cycle {} Date {}",
			currentAirac, currentAiracDate);
	}
	else {
		LOG_ERROR("Version Call: AIRAC Cycle/Date Data Not Found");
		SendToConsole(vfpc::urgent, "Failed to read AIRAC cycle/date - the plugin has been disabled.");
	}

	bool updatefail = false;
	vector<int> newdate = { 0, 0, 0 };
	const auto* dateVal = FindMemberValue(version, "date");
	const auto* lastUpdatedDate = FindMemberValue(version, "last_updated_date");
	const auto* lastUpdatedTime = FindMemberValue(version, "last_updated_time");

	if (dateVal && dateVal->IsString() &&
		lastUpdatedDate && lastUpdatedDate->IsString() &&
		lastUpdatedTime && lastUpdatedTime->IsString()) {

		string lastdate = version["last_updated_date"].GetString();
		if (lastdate.size() == 10) {
			try {
				int lastday = stoi(lastdate.substr(0, 2));
				int lastmonth = stoi(lastdate.substr(3, 2));
				int lastyear = stoi(lastdate.substr(6, 4));
				lastupdate[0] = lastyear;
				lastupdate[1] = lastmonth;
				lastupdate[2] = lastday;
			}
			catch (...) {
				LOG_ERROR("Version Call: Last Updated Date Data Unreadable - String->Int Failed");
				updatefail = true;
			}
		}
		else {
			LOG_ERROR("Version Call: Last Updated Date Data Unreadable - Wrong Size");
			updatefail = true;
		}
		string lasttime = version["last_updated_time"].GetString();
		if (lasttime.size() == 8) {
			try {
				int lasthour = stoi(lasttime.substr(0, 2));
				int lastmins = stoi(lasttime.substr(3, 2));
				lastupdate[3] = lasthour;
				lastupdate[4] = lastmins;
			}
			catch (...) {
				LOG_ERROR("Version Call: Last Updated Time Data Unreadable - String->Int Failed");
				updatefail = true;
			}
		}
		else {
			LOG_ERROR("Version Call: Last Updated Time Data Unreadable - Wrong Size");
			updatefail = true;
		}

		string date = version["date"].GetString();
		if (date.size() == 10) {
			try {
				int day = stoi(date.substr(0, 2));
				int month = stoi(date.substr(3, 2));
				int year = stoi(date.substr(6, 4));
				newdate[0] = year;
				newdate[1] = month;
				newdate[2] = day;
			}
			catch (...) {
				LOG_ERROR("Version Call: Date Data Unreadable - String->Int Failed");
				updatefail = true;
			}
		}
		else {
			LOG_ERROR("Version Call: Date Data Unreadable - Wrong Size");
			updatefail = true;
		}
	}
	else {
		LOG_ERROR("Version Call: Update Data Not Found");
		updatefail = true;
	}
	if (updatefail) {
		SendToConsole(vfpc::urgent, "Failed to read date/last update record from API.");
		apiUpdated = true;
	}
	else {
		bool stop = false;
		for (size_t i = 0; i < lastupdate.size(); i++) {
			if (!stop) {
				if (lastupdate[i] > timedata[i]) {
					apiUpdated = true;
					stop = true;
					LOG_DEBUG("Version Call: Update Has Occurred - Pull From API Next Pass.");
				}
				else if (lastupdate[i] != timedata[i]) {
					stop = true;
					LOG_DEBUG("Version Call: Update Has Not Occurred.");
				}
			}
		}
		if (!stop) {
			apiUpdated = true;
		}
	}
	for (size_t i = 0; i < newdate.size(); i++) {
		timedata[i] = newdate[i];
	}

	bool timefail = false;
	if (version.HasMember("time") && version["time"].IsString() &&
		version.HasMember("day") && version["day"].IsInt()) {

		int day = version["day"].GetInt();
		day += 6;
		day %= 7;
		string time = version["time"].GetString();
		if (time.size() == 8) {
			try {
				int hour = stoi(time.substr(0, 2));
				int mins = stoi(time.substr(3, 2));
				timedata[3] = hour;
				timedata[4] = mins;
				timedata[5] = day;
			}
			catch (...) {
				LOG_ERROR("Version Call: Time Data Unreadable - String->Int Failed");
				timefail = true;
			}
		}
		else {
			LOG_ERROR("Version Call: Time Data Unreadable - Wrong Size");
			timefail = true;
		}
	}
	else {
		LOG_ERROR("Version Call: Time Data Not Found");
		timefail = true;
	}

	if (timefail) {
		SendToConsole(vfpc::urgent, "Failed to read day/time from API.");
	}
	return out;
}

std::string CVFPCPlugin::RouteOutputShort_(const rapidjson::Value& constraints,
	const std::string& destination,
	int rfl,
	const std::vector<std::string>& route_points,
	std::size_t max_examples) const
{
	const auto examples = CollectRouteExamples_(constraints, destination, rfl, route_points);

	if (examples.empty()) {
		return "No permitted route match.";
	}

	std::string out = "No permitted route match. Examples: ";

	const std::size_t shown = std::min(max_examples, examples.size());
	for (std::size_t i = 0; i < shown; ++i) {
		if (i > 0) {
			out += " / ";
		}
		out += examples[i];
	}

	if (examples.size() > shown) {
		out += ". +" + std::to_string(examples.size() - shown) + " more.";
	}
	else {
		out += ".";
	}

	return TruncateForDisplay_(out, 160);
}

std::string CVFPCPlugin::RouteOutputFailureDetail_(const rapidjson::Value& constraints,
	const std::string& destination,
	int rfl,
	const std::vector<std::string>& route_points,
	std::size_t max_examples) const
{
	const auto examples = CollectRouteExamples_(constraints, destination, rfl, route_points);

	if (examples.empty()) {
		return "Valid Initial Routes: None.";
	}

	std::string out = "Valid Initial Routes: ";

	const std::size_t shown = std::min(max_examples, examples.size());
	for (std::size_t i = 0; i < shown; ++i) {
		if (i > 0) {
			out += " / ";
		}
		out += examples[i];
	}

	if (examples.size() > shown) {
		out += " / ... +" + std::to_string(examples.size() - shown) + " more";
	}

	return out;
}

//==============================================================
// 6. Airport snapshot loading / worker helpers
//==============================================================
void CVFPCPlugin::getSids()
{
	const bool already_requested =
		airport_reload_requested_.exchange(true,
			std::memory_order_acq_rel);
	if (!already_requested) {
		LOG_TRACE("Airport/SID reload requested.");
	}
}

bool CVFPCPlugin::FetchSidsInto_(SidSource source,
	const std::string& activeAirportIcao,
	std::stop_token st,
	rapidjson::Document& out)
{
	out.SetObject();

	if (activeAirportIcao.empty()) {
		LOG_WARN("FetchSidsInto_: Active airport ICAO is empty");
		return false;
	}

	if (source == SidSource::File) {
		LOG_TRACE("FetchSidsInto_: File Mode requested airport = '{}'", activeAirportIcao);

		rapidjson::Document full_file_data;
		if (!fileCall(full_file_data)) {

			LOG_ERROR("FetchSidsInto_: fileCall failed");
			out.SetObject();
			return false;
		}

		if (!ExtractSingleAirportToObject_(full_file_data, activeAirportIcao, out)) {

			SendToConsole(vfpc::urgent,
				"Airport '{}' was not found in {}. Use '{}{}' to return to API loading.",
				activeAirportIcao,
				DATA_FILE,
				COMMAND_PREFIX,
				LOAD_COMMAND);

			LOG_ERROR("FetchSidsInto_: ExtractSingleAirportToObject_ failed for {}", activeAirportIcao);

			out.SetObject();
			return false;
		}

		LOG_INFO("FetchSidsInto_: file data filtered to airport '{}'.", activeAirportIcao);
		return out.IsObject();
	}

	std::string endpoint = plugin_config_.airport_endpoint + activeAirportIcao;
	LOG_DEBUG("FetchSidsInto_: API mode requested airport='{}' endpoint='{}'",
		activeAirportIcao, endpoint);
	rapidjson::Document api_data;
	if (!APICall(plugin_config_.base_url, endpoint, st, api_data)) {

		LOG_ERROR("FetchSidsInto_: APICall failed for {}", activeAirportIcao);
		out.SetObject();
		return false;
	}

	if (!ExtractSingleAirportToObject_(api_data, activeAirportIcao, out)) {

		LOG_WARN("FetchSidsInto_: airport '{}' was not found in {}", activeAirportIcao, DATA_FILE);
		out.SetObject();
		return false;
	}


	return out.IsObject();
}

bool CVFPCPlugin::ExtractSingleAirportToObject_(const rapidjson::Document& in,
	const std::string& airport_icao,
	rapidjson::Document& out) const
{
	out.SetObject();

	LOG_TRACE("ExtractSingleAirportToObject_: requested airport='{}' root type = {}",
		airport_icao,
		static_cast<int>(in.GetType()));

	if (airport_icao.empty()) {
		LOG_ERROR("ExtractSingleAirportToObject_: airport ICAO is empty");
		return false;
	}

	auto& alloc = out.GetAllocator();
	if (in.IsObject()) {

		const auto* airports = FindArrayMember(in, "airports");
		if (airports) {
			for (rapidjson::SizeType i = 0; i < airports->Size(); ++i) {
				const auto& airport = (*airports)[i];

				if (!airport.IsObject()) {
					continue;
				}

				const auto* icao = FindMemberValue(airport, "icao");
				if (!icao || !icao->IsString()) {
					continue;
				}

				if (airport_icao == icao->GetString()) {
					out.CopyFrom(airport, alloc);
					return true;
				}
			}

			LOG_ERROR("ExtractSingleAirportToObject_: airport '{}' not found in airports array",
				airport_icao);
			return false;
		}
		
		const auto* icao = FindMemberValue(in, "icao");
		if (!icao || !icao->IsString()) {

			LOG_ERROR("ExtractSingleAirportToObject_: single-airport JSON has no valid 'icao'");
			return false;
		}

		if (airport_icao != icao->GetString()) {

			LOG_ERROR("ExtractSingleAirportToObject_: requested airport '{}' does not match airport '{}'",
				airport_icao, icao->GetString());
			return false;
		}

		out.CopyFrom(in, alloc);
		return true;
	}

	if (in.IsArray()) {
		for (rapidjson::SizeType i = 0; i < in.Size(); ++i) {
			const auto& airport = in[i];
			if (!airport.IsObject()) {
				continue;
			}
			const auto* icao = FindMemberValue(airport, "icao");
			if (!icao || !icao->IsString()) {
				continue;
			}
			if (airport_icao == icao->GetString()) {
				out.CopyFrom(airport, alloc);
				return true;
			}
		}
		LOG_ERROR("ExtractSingleAirportToObject_: airport '{}' not found in array", airport_icao);
		return false;
	}

	LOG_ERROR("ExtractSingleAirportToObject_: root JSON is neither object nor array(type = {})",
		static_cast<int>(in.GetType()));

	return false;
}

//==============================================================
// 7. Validation helper passes
//==============================================================
std::vector<bool> CVFPCPlugin::checkDestination(const rapidjson::Value& conditions,
	const std::string& destination,
	std::vector<bool>& in) const
{
	std::vector<bool> out{};
	out.reserve(conditions.Size());

	for (size_t i = 0; i < conditions.Size(); i++) {
		if (!in[i]) {
			out.push_back(false);
			continue;
		}

		bool res = true;

		const auto& cond = conditions[i];
		const auto* nodestsArr = FindArrayMember(cond, "nodests");
		if (nodestsArr && !nodestsArr->Empty()) {
			if (destArrayContains(*nodestsArr, destination.c_str()).size()) {
				res = false;
			}
		}
		const auto* destsArr = FindArrayMember(cond, "dests");
		if (destsArr && !destsArr->Empty()) {
			if (!destArrayContains(*destsArr, destination.c_str()).size()) {
				res = false;
			}
		}
		out.push_back(res);
	}
	return out;
}

std::vector<bool> CVFPCPlugin::checkExitPoint(const rapidjson::Value& conditions,
	const std::vector<std::string>& points,
	std::vector<bool>& in) const
{
	std::vector<bool> out{};
	out.reserve(conditions.Size());

	for (rapidjson::SizeType i = 0; i < conditions.Size(); i++) {
		if (!in[i]) {
			out.push_back(false);
			continue;
		}

		bool res = true;

		const auto& cond = conditions[i];
		const auto* pointsArr = FindArrayMember(cond, "points");
		if (pointsArr && !pointsArr->Empty()) {
			bool temp = false;
			for (const std::string& each : points) {
				if (arrayContainsString(*pointsArr, each)) {
					temp = true;
					break;
				}
			}
			if (!temp) {
				res = false;
			}
		}
		const auto* nopointsArr = FindArrayMember(cond, "nopoints");
		if (nopointsArr && !nopointsArr->Empty()) {
			bool temp = false;
			for (const std::string& each : points) {
				if (arrayContainsString(*nopointsArr, each)) {
					temp = true;
					break;
				}
			}
			if (temp) {
				res = false;
			}
		}
		out.push_back(res);
	}
	return out;
}

std::vector<bool> CVFPCPlugin::checkRoute(
	const rapidjson::Value& sid_constraints,
	const std::vector<std::string>& filed_route,
	std::vector<bool>& in,
	std::vector<std::string>* route_advisories) const
{
	std::vector<bool> out{};
	out.reserve(sid_constraints.Size());

	for (size_t i = 0; i < sid_constraints.Size(); i++) {
		if (!in[i]) {
			out.push_back(false);
			continue;
		}

		bool result = true;

		const auto& constraint = sid_constraints[i];
		if (!constraint.IsObject()) {
			out.push_back(false);
			continue;
		}

		const auto* route_array = FindArrayMember(constraint, "route");
		if (route_array && !route_array->Empty()) {
			if (!routeContains(filed_route, *route_array, route_advisories)) { // route from FP, routeArr from condition.
				result = false;
			}
		}

		const auto* noroute_array = FindArrayMember(constraint, "noroute");
		if (noroute_array && !noroute_array->Empty()) {
			if (routeContains(filed_route, *noroute_array, route_advisories)) {
				result = false;
			}
		}

		out.push_back(result);
		LOG_TRACE("Route Check: Completed with {} result", BoolToString(result));
	}
	return out;
}

std::vector<bool> CVFPCPlugin::checkMinMax(const rapidjson::Value& conditions,
	int RFL,
	std::vector<bool>& in) const
{
	std::vector<bool> out{};
	for (size_t i = 0; i < conditions.Size(); i++) {
		if (!in[i]) {
			out.push_back(false);
			continue;
		}

		bool res = true;

		int Min, Max;

		const auto& condition = conditions[i];
		if (!condition.IsObject()) {
			out.push_back(false);
			continue;
		}

		const auto* minObj = FindMemberValue(condition, "min");
		if (minObj && minObj->IsInt()) {
			if ((Min = minObj->GetInt()) > 0 && (RFL / 100) < Min) {
				res = false;

			}
		}

		//Max Level
		const auto* maxObj = FindMemberValue(condition, "max");
		if (maxObj && maxObj->IsInt()) {
			if ((Max = maxObj->GetInt()) > 0 && (RFL / 100) > Max) {
				res = false;
			}
		}
		out.push_back(res);
	}
	return out;
}

std::vector<bool> CVFPCPlugin::checkDirection(const rapidjson::Value& conditions,
	int RFL,
	vector<bool>& in)const
{
	vector<bool> out{};
	out.reserve(conditions.Size());

	for (size_t i = 0; i < conditions.Size(); i++) {
		if (!in[i]) {
			out.push_back(false);
			continue;
		}

		//Assume any level valid if no "EVEN" or "ODD" declaration
		bool res = true;

		const auto& condition = conditions[i];
		const auto* dirObj = FindMemberValue(condition, "dir");
		if (!dirObj || !dirObj->IsString()) {
			out.push_back(res);
			continue;
		}

		string direction = dirObj->GetString();
		direction = to_upper(direction);
		LOG_TRACE("Direction Check: RFL={} dir='{}'", RFL, direction);
		if (direction == EVEN_DIRECTION) {
			//Assume invalid until condition matched
			res = false;
			//Non-RVSM (Above FL410)
			if ((RFL > RVSM_UPPER && ((RFL - RVSM_UPPER) / 1000) % 4
				== 2)) {

				res = true;
			}
			//RVSM (FL290-410) or Below FL290
			else if (RFL <= RVSM_UPPER && (RFL / 1000) % 2 == 0) {
				res = true;
			}
		}
		else if (direction == ODD_DIRECTION) {
			//Assume invalid until condition matched
			res = false;
			//Non-RVSM (Above FL410)
			if ((RFL > RVSM_UPPER && ((RFL - RVSM_UPPER) / 1000) % 4
				== 0)) {
				res = true;
			}
			//RVSM (FL290-410) or Below FL290
			else if (RFL <= RVSM_UPPER && (RFL / 1000) % 2 == 1) {
				res = true;
			}
		}
		LOG_TRACE("Direction Check: Completed with {} result", res ? "PASSED" : "FAILED");
		out.push_back(res);
	}
	return out;
}

namespace
{
	bool AlertWarningApplies_(const rapidjson::Value& condition,
		const rapidjson::Value& alert,
		int rfl)
	{
		if (!alert.IsObject()) {
			return false;
		}

		const auto* alertMin = FindMemberValue(alert, "min");
		const auto* alertMax = FindMemberValue(alert, "max");

		const auto* conditionMin = FindMemberValue(condition, "min");
		const auto* conditionMax = FindMemberValue(condition, "max");

		const auto* minVal =
			(alertMin && alertMin->IsInt()) ? alertMin :
			(conditionMin && conditionMin->IsInt()) ? conditionMin :
			nullptr;

		const auto* maxVal =
			(alertMax && alertMax->IsInt()) ? alertMax :
			(conditionMax && conditionMax->IsInt()) ? conditionMax :
			nullptr;

		// No level condition means this is a general warning.
		if (!minVal && !maxVal) {
			return true;
		}

		const int rfl_fl = rfl / 100;

		if (minVal && minVal->GetInt() > 0 && rfl_fl < minVal->GetInt()) {
			return true;
		}

		if (maxVal && maxVal->GetInt() > 0 && rfl_fl > maxVal->GetInt()) {
			return true;
		}

		return false;
	}
}

std::vector<bool> CVFPCPlugin::checkAlerts(const rapidjson::Value& constraint,
	bool* warn,
	vector<bool>& in,
	int rfl) const
{
	vector<bool> out{};
	out.reserve(constraint.Size());

	for (rapidjson::SizeType i = 0; i < constraint.Size(); ++i) {
		// If a constraint for this index is already false, 
		// skip processing and keep it false in output.
		if (!in[i]) {
			out.push_back(false);
			continue;
		}

		const auto& condition = constraint[i];
		if (!condition.IsObject()) {
			out.push_back(false);
			continue;
		}

		bool res = true;

		const auto* alerts = FindArrayMember(condition, "alerts");
		if (!alerts || alerts->Empty()) {
			out.push_back(res);
			continue;
		}

		for (rapidjson::SizeType j = 0; j < alerts->Size(); ++j) {
			const auto& alert = (*alerts)[j];
			if (!alert.IsObject()) {
				continue;
			}

			if (const auto* ban = FindMemberValue(alert, "ban");
				ban && ban->IsBool() && ban->GetBool()) {
				res = false;
			}

			if (const auto* warnValue = FindMemberValue(alert, "warn");
				warnValue && warnValue->IsBool() && warnValue->GetBool()) {

				const bool applies = AlertWarningApplies_(condition, alert, rfl);

				LOG_TRACE("Alert warning check: rfl_fl={} applies={}",
					rfl / 100,
					applies);

				if (applies) {
					*warn = true;
				}
			}
		}

		out.push_back(res);
	}

	return out;
}

bool CVFPCPlugin::RestrictionTypeSelectorMatches_(
	const rapidjson::Value& typesArr,
	const FlightPlanRow& row) const
{
	if (!typesArr.IsArray() || typesArr.Empty()) {
		return true;
	}

	if (arrayContainsString(typesArr, row.aircraft_type)) {
		return true;
	}

	if (arrayContainsString(typesArr, row.engine_type)) {
		return true;
	}

	return false;
}

bool CVFPCPlugin::RestrictionSelectorsMatch_(const FlightPlanRow& row,
	const rapidjson::Value& restriction) const
{
	if (!restriction.IsObject()) {
		return false;
	}

	// ----------------------------------------------------------
	// suffix
	// ----------------------------------------------------------
	if (const auto* suffixArr = FindArrayMember(restriction, "suffix")) {
		if (!suffixArr->Empty()) {
			if (!arrayContainsEnding(*suffixArr, row.sid_suffix)) {
				return false;
			}
		}
	}

	// ----------------------------------------------------------
	// aircraft / engine types
	// ----------------------------------------------------------
	if (const auto* typesArr = FindArrayMember(restriction, "types")) {

		const bool type_match =
			RestrictionTypeSelectorMatches_(*typesArr, row);

		LOG_TRACE(
			"RestrictionTypesMatch_: callsign='{}' aircraft_type='{}' engine_type='{}' result={}",
			row.callsign,
			row.aircraft_type,
			row.engine_type,
			type_match);

		if (!type_match) {
			return false;
		}
	}

	// ----------------------------------------------------------
	// Route selector (SID-level applicability by initial airway)
	// ----------------------------------------------------------
	LOG_TRACE("RestrictionSelectorsMatch_: callsign='{}' has_route={} type='{}' ac_type='{}' engine_type='{}'",
		row.callsign,
		FindMemberValue(restriction, "route") != nullptr,
		restriction.HasMember("route") ? "member" : "no-member",
		row.aircraft_type,
		row.engine_type);

	if (const auto* routeArr = FindArrayMember(restriction, "route")) {
		const std::string route_text = fmt::format("{}", fmt::join(row.route_points, " "));

		LOG_TRACE("RestrictionSelectorsMatch_: route selector present size={} first_waypoint='{}' route='{}'",
			routeArr->Size(),
			row.first_waypoint,
			route_text);

		if (!routeArr->Empty()) {
			const auto cleaned_route = BuildValidationRoutePoints_(row.route_points);

			if (!SidLevelRestrictionRouteMatches_(
				*routeArr,
				cleaned_route,
				row.first_waypoint)) {
				return false;
			}
		}
	}

	return true;
}

bool CVFPCPlugin::RestrictionObjectMatches_(const FlightPlanRow& row,
	const rapidjson::Value& restriction,
	RestrictionFailureFlags& fails) const
{
	if (!restriction.IsObject()) {
		return false;
	}

	RestrictionFailureFlags local{};

// ----------------------------------------------------------
// suffix / route applicability first
// ----------------------------------------------------------

	// Suffix mismatch means this restriction does not apply.
	if (const auto* suffixArr = FindArrayMember(restriction, "suffix")) {
		if (!suffixArr->Empty()) {
			if (!arrayContainsEnding(*suffixArr, row.sid_suffix)) {
				return false;
			}
		}
	}

	// Route mismatch means this restriction does not apply.
	if (const auto* routeArr = FindArrayMember(restriction, "route")) {
		if (!routeArr->Empty()) {
			const auto cleaned_route = BuildValidationRoutePoints_(row.route_points);

			if (!SidLevelRestrictionRouteMatches_(
				*routeArr,
				cleaned_route,
				row.first_waypoint)) {
				return false;
			}
		}
	}

	// ----------------------------------------------------------
	// aircraft / engine type eligibility
	// ----------------------------------------------------------

	// Type mismatch means the restriction applies, but the aircraft is not eligible.
	if (const auto* typesArr = FindArrayMember(restriction, "types")) {
		if (!typesArr->Empty()) {
			const bool type_match =
				RestrictionTypeSelectorMatches_(*typesArr, row);

			LOG_TRACE(
				"RestrictionObjectMatches_: callsign='{}' aircraft_type='{}' engine_type='{}' type_match={}",
				row.callsign,
				row.aircraft_type,
				row.engine_type,
				type_match);

			if (!type_match) {
				fails.type = true;
				return false;
			}
		}
	}

	// ----------------------------------------------------------
	// time window: applicable restriction must pass time
	// ----------------------------------------------------------
	const auto* startObj = FindMemberValue(restriction, "start");
	const auto* endObj = FindMemberValue(restriction, "end");

	if (startObj && endObj && startObj->IsObject() && endObj->IsObject()) {
		if (!checkTimeWindow(row.eobt, restriction)) {
			local.time = true;

			fails.time = true;
			return false;
		}
	}

	//// ----------------------------------------------------------
	//// banned
	//// ----------------------------------------------------------
	//if (const auto* bannedValue = FindMemberValue(restriction, "banned");
	//	bannedValue && bannedValue->IsBool() && bannedValue->GetBool()) {
	//	local.banned = true;

	//	fails.suffix = fails.suffix || local.suffix;
	//	fails.type = fails.type || local.type;
	//	fails.time = fails.time || local.time;
	//	fails.banned = fails.banned || local.banned;

	//	return false;
	//}

	// ----------------------------------------------------------
	// banned
	// ----------------------------------------------------------
	// A matching banned restriction is still a MATCH.
	// Do not return false here, otherwise the constraint is removed
	// before route matching and the result becomes RTE instead of BAN.
	if (const auto* bannedValue = FindMemberValue(restriction, "banned");
		bannedValue && bannedValue->IsBool() && bannedValue->GetBool()) {
		fails.banned = true;
		return true;
	}

	return true;
}

CVFPCPlugin::SidRestrictionResult
CVFPCPlugin::EvaluateSidRestrictions_(const FlightPlanRow& row,
	const rapidjson::Value& sid_restrictions) const
{
	SidRestrictionResult result{};

	if (!sid_restrictions.IsArray() || sid_restrictions.Empty()) {
		result.has_restrictions = false;
		result.any_applicable = false;
		result.passed = true;
		return result;
	}

	result.has_restrictions = true;
	result.any_applicable = false;
	result.passed = false;

	for (rapidjson::SizeType i = 0; i < sid_restrictions.Size(); ++i) {
		const auto& restriction = sid_restrictions[i];
		if (!restriction.IsObject()) {
			continue;
		}

		RestrictionFailureFlags local_fails{};

		if (RestrictionObjectMatches_(row, restriction, local_fails)) {
			result.any_applicable = true;
			result.passed = true;
			return result;
		}

		const bool applicable_but_failed =
			local_fails.suffix ||
			local_fails.type ||
			local_fails.time ||
			local_fails.banned;

		if (applicable_but_failed) {
			result.any_applicable = true;

			result.fails.suffix = result.fails.suffix || local_fails.suffix;
			result.fails.type = result.fails.type || local_fails.type;
			result.fails.time = result.fails.time || local_fails.time;
			result.fails.banned = result.fails.banned || local_fails.banned;
		}
	}

	return result;
}

bool CVFPCPlugin::SidLevelFailureCanBeOverriddenByConstraint_(
	const FlightPlanRow& row,
	const rapidjson::Value& sid_restrictions) const
{
	if (!sid_restrictions.IsArray() || sid_restrictions.Empty()) {
		return false;
	}

	bool has_broad_type_only_failure = false;

	for (const auto& restriction : sid_restrictions.GetArray()) {
		if (!restriction.IsObject()) {
			continue;
		}

		RestrictionFailureFlags fails{};

		if (RestrictionObjectMatches_(row, restriction, fails)) {
			continue;
		}

		const bool applicable_but_failed =
			fails.suffix ||
			fails.type ||
			fails.time ||
			fails.banned;

		if (!applicable_but_failed) {
			continue;
		}

		const bool has_suffix_selector =
			FindMemberValue(restriction, "suffix") != nullptr;

		const bool has_route_selector =
			FindMemberValue(restriction, "route") != nullptr;

		const bool has_time_selector =
			FindMemberValue(restriction, "start") != nullptr ||
			FindMemberValue(restriction, "end") != nullptr;

		// If the failed restriction is suffix-specific, route-specific,
		// time-specific, or banned, it is a hard SID-level failure.
		if (has_suffix_selector ||
			has_route_selector ||
			has_time_selector ||
			fails.time ||
			fails.banned)
		{
			return false;
		}

		// Only broad type-only failures may be overridden.
		if (fails.type && FindArrayMember(restriction, "types")) {
			has_broad_type_only_failure = true;
		}
	}

	return has_broad_type_only_failure;
}

CVFPCPlugin::ConstraintRestrictionResult
CVFPCPlugin::EvaluateConstraintRestrictions_(const FlightPlanRow& row,
	const rapidjson::Value& restriction_array) const
{
	ConstraintRestrictionResult result{};

	if (!restriction_array.IsArray() || restriction_array.Empty()) {
		result.has_restrictions = false;
		result.passed = true;
		return result;
	}

	result.has_restrictions = true;
	result.passed = false;

	for (rapidjson::SizeType i = 0; i < restriction_array.Size(); ++i) {
		const auto& restriction = restriction_array[i];
		RestrictionFailureFlags local_fails{};
		if (RestrictionObjectMatches_(row, restriction, local_fails))
		{
			result.passed = true;
			result.fails.banned = result.fails.banned || local_fails.banned;
			return result;
		}
		result.fails.suffix = result.fails.suffix || local_fails.suffix;
		result.fails.type = result.fails.type || local_fails.type;
		result.fails.time = result.fails.time || local_fails.time;
		result.fails.banned = result.fails.banned || local_fails.banned;
	}
	return result;
}

std::vector<bool> CVFPCPlugin::CheckConstraintRestrictions_(const FlightPlanRow& row,
	const rapidjson::Value& conditions,
	RestrictionFailureFlags& aggregate_failures,
	std::vector<bool>& in) const
{
	std::vector<bool> out{};
	out.reserve(conditions.Size());

	for (rapidjson::SizeType i = 0; i < conditions.Size(); ++i) {
		if (!in[i]) {
			out.push_back(false);
			continue;
		}

		const auto& condition = conditions[i];
		if (!condition.IsObject()) {
			out.push_back(false);
			continue;
		}

		const auto* restrictionArr = FindArrayMember(condition, "restrictions");
		if (!restrictionArr) {
			out.push_back(true); // no constraint-level narrowing
			continue;
		}

		const auto restriction_result = EvaluateConstraintRestrictions_(row, *restrictionArr);

		if (!restriction_result.passed) {
			aggregate_failures.suffix = aggregate_failures.suffix || restriction_result.fails.suffix;
			aggregate_failures.type = aggregate_failures.type || restriction_result.fails.type;
			aggregate_failures.time = aggregate_failures.time || restriction_result.fails.time;
			aggregate_failures.banned = aggregate_failures.banned || restriction_result.fails.banned;
		}

		out.push_back(restriction_result.passed);
	}
	return out;
}

//==============================================================
// 8. Validation support helpers
//==============================================================
// 8a. Flight-plan parsing and normalisation helpers.
CVFPCPlugin::ValidationResult CVFPCPlugin::ValidateFlightPlan_(const FlightPlanRow& row) const
{
	LOG_TRACE("ValidateFlightPlan_: callsign='{}' eobt='{}' origin='{}' dest='{}' sid='{}' fwp='{}'",
		row.callsign,
		row.eobt,
		row.origin,
		row.destination,
		row.sid,
		row.first_waypoint);
	LOG_TRACE("ValidateFlightPlan_: callsign='{}' is_ifr={} sid_suffix='{}'",
		row.callsign,
		row.is_ifr,
		row.sid_suffix);
	LOG_TRACE("ValidateFlightPlan_: callsign='{}' aircraft_type='{}' engine_type='{}' pbn='{}' has_rnav1={}",
		row.callsign,
		row.aircraft_type,
		row.engine_type,
		row.pbn,
		row.has_rnav1);

	ValidationResult validationResult = initialiseResults(row.callsign);

	// -----------------------------
	// 1. IFR gating
	// -----------------------------

	if (!row.is_ifr) {
		SetNotChecked_(validationResult, "VFR");

		SetField_(validationResult, VF_SID, "Not Checked",
			"Flight plan type is not IFR; SID validation not applicable.");

		LOG_TRACE("ValidateFlightPlan_: callsign='{}' skipped because flight plan is not IFR",
			row.callsign);

		return validationResult;
	}

	// Read immutable validation data safely across threads.
	auto validation_data = airport_data_.load(std::memory_order_acquire);

	// -----------------------------
	// 2. Snapshot availability
	// -----------------------------
	if (!validation_data) {
		SetPending_(validationResult, "PDG");

		SetField_(validationResult, VF_SID, "Loading", "Airport data not loaded yet.");

		LOG_TRACE("ValidateFlightPlan_: callsign='{}' pending because airport snapshot is not loaded",
			row.callsign);

		return validationResult;
	}

	const rapidjson::Value& airport = validation_data->airport_json;

	// -----------------------------
	// 3. Active airport guard
	// -----------------------------
	std::string locked_airport;
	bool airport_locked = false;
	{
		std::lock_guard<std::mutex> g(active_mtx_);
		locked_airport = active_airport_;
		airport_locked = active_airport_locked_;
	}
	if (airport_locked && !locked_airport.empty() && row.origin != locked_airport) {
		SetNotChecked_(validationResult, "OFF");

		SetField_(validationResult, VF_ORIGIN, "Not Checked",
			"Flight plan origin does not match the active airport for this session.");

		SetField_(validationResult, VF_SID, "Not Checked",
			"Validation skipped because this flight plan is outside the active airport for the session.");

		LOG_TRACE("ValidateFlightPlan_: callsign='{}' skipped because flight plan is outside the active airport for the session",
			row.callsign);

		return validationResult;
	}

	// ----------------------------------------------------------
	// 4. Basic field checks
	// ----------------------------------------------------------
	const bool have_origin = !row.origin.empty();
	const bool have_destination = !row.destination.empty();
	const bool have_route_text = !row.route.empty();
	const bool have_route_points = !row.route_points.empty();

	if (!have_origin) {
		SetField_(validationResult, VF_ORIGIN, "ERR", "Origin missing.");
		SetField_(validationResult, VF_SID, "ERR", "Origin missing;	SID lookup not possible.");
		SetBlockedFields_(validationResult,
			{
				VF_DESTINATION,
				VF_EXIT_POINT,
				VF_ROUTE,
				VF_MIN_MAX,

				VF_ODD_EVEN,
				VF_SUFFIX,
				VF_RESTRICTIONS,
				VF_WARNINGS,
				VF_BANS
			},
			"Blocked because origin is missing.");
	}

	if (!have_destination) {
		SetField_(validationResult, VF_DESTINATION, "ERR", "Destination missing.");
	}

	if (!have_route_text) {
		SetField_(validationResult, VF_SYNTAX, "ERR", "Route missing.");
		SetBlockedFields_(validationResult,
			{
				VF_EXIT_POINT,
				VF_ROUTE
			},
			"Blocked because route is missing.");
	}

	// ----------------------------------------------------------
	// 5. Single-airport snapshot checks
	// ----------------------------------------------------------
	bool can_use_airport_data = false;

	if (have_origin) {
		if (!airport.IsObject()) {

			SetField_(validationResult, VF_ORIGIN, "Data Error",
				"Loaded airport data is not an object.");

			SetFieldIfUnset_(validationResult, VF_SID, "ERR",
				"Loaded airport data is not a valid airport object.");

			SetBlockedFields_(validationResult,
				{
					VF_DESTINATION,
					VF_EXIT_POINT,
					VF_ROUTE,
					VF_MIN_MAX,
					VF_ODD_EVEN,
					VF_SUFFIX,
					VF_RESTRICTIONS,
					VF_WARNINGS,
					VF_BANS
				},
				"Blocked because loaded airport data is invalid.");
		}
		else {
			const auto* icao = FindMemberValue(airport, "icao");
			if (!icao || !icao->IsString()) {
				SetField_(validationResult, VF_ORIGIN, "Data Error",
					"Loaded airport data has no valid ICAO.");

				SetFieldIfUnset_(validationResult, VF_SID, "ERR",
					"Loaded airport data has no valid ICAO.");

				SetBlockedFields_(validationResult,
					{
						VF_DESTINATION,
						VF_EXIT_POINT,
						VF_ROUTE,
						VF_MIN_MAX,
						VF_ODD_EVEN,
						VF_SUFFIX,
						VF_RESTRICTIONS,
						VF_WARNINGS,
						VF_BANS
					},
					"Blocked because loaded airport data is invalid.");
			}
			else if (row.origin != icao->GetString()) {
				SetField_(validationResult, VF_ORIGIN, "Failed",
					"Flight plan origin '" + row.origin +
					"' does not match loaded airport '" + std::string(icao->GetString()) + "'.");

				SetFieldIfUnset_(validationResult, VF_SID, "ERR",
					"Flight plan origin does not match loaded airport.");

				SetBlockedFields_(validationResult,
					{
						VF_DESTINATION,
						VF_EXIT_POINT,
						VF_ROUTE,
						VF_MIN_MAX,
						VF_ODD_EVEN,
						VF_SUFFIX,
						VF_RESTRICTIONS,
						VF_WARNINGS,
						VF_BANS
					},
					"Blocked because flight plan origin does not match loaded airport.");
			}
			else {
				can_use_airport_data = true;

				LOG_TRACE("Origin airport '{}' matches the loaded airport.", row.origin);
				SetField_(validationResult, VF_ORIGIN, "Passed", "Origin matches the loaded airport.");
			}
		}
	}

	// ----------------------------------------------------------
	// 6. Syntax / normalisation checks
	// ----------------------------------------------------------
	std::vector<std::string> normalised_route =
		BuildValidationRoutePoints_(row.route_points);

	const std::string first_waypoint_for_validation =
		StripRouteTokenSuffixForValidation_(row.first_waypoint);

	std::string syntax_message;
	bool syntax_ok = false;

	if (have_route_text) {
		syntax_ok = NormaliseAndValidateRouteSyntax_(normalised_route,
			row.origin,
			row.destination,
			row.sid,
			first_waypoint_for_validation,
			syntax_message);

		if (!syntax_ok) {
			SetField_(validationResult, VF_SYNTAX, "Invalid Syntax", syntax_message);
			SetBlockedFields_(validationResult,
				{
					VF_EXIT_POINT,
					VF_ROUTE
				},
				"Blocked by route syntax error.");
		}
		else {
			LOG_TRACE("Route syntax valid for '{}'. Origin='{}' Dest = '{}' EOBT = '{}' SID = '{}' FWP = '{}' RoutePoints = {}",
				row.callsign,
				row.origin,
				row.destination,
				row.eobt,
				row.sid,
				row.first_waypoint,
				normalised_route.size());

			SetField_(validationResult, VF_SYNTAX, "Passed", "Route syntax valid.");
		}
	}

	// ----------------------------------------------------------
	// 7. SIDs defined for active airport?
	// ----------------------------------------------------------
	const rapidjson::Value* airportSids = nullptr;
	bool have_sids = false;

	if (can_use_airport_data) {

		airportSids = FindArrayMember(airport, "sids"); // Looking for { "icao": "XXXX", "sids" : [...] }
		if (!airportSids || airportSids->Empty()) {

			SetField_(validationResult, VF_SID, "No SIDs", row.origin +
				" exists in database but has no SIDs (or non - SID routes) defined.");

			SetBlockedFields_(validationResult,
				{
					VF_DESTINATION,
					VF_EXIT_POINT,
					VF_ROUTE,
					VF_MIN_MAX,
					VF_ODD_EVEN,
					VF_SUFFIX,
					VF_RESTRICTIONS,
					VF_WARNINGS,
					VF_BANS
				},
				"Blocked because no SID data is available.");
		}
		else {
			LOG_TRACE("Loaded airport has {} SID/non-SID routes defined.", airportSids->Size());
			have_sids = true;
		}
	}

	// Detect flights that appear to have filed a SID in the route,
	// but EuroScope has not recognised a SID. This sometimes happens
	// when the filed SID is not valid for the runway in use.
	if (have_sids &&
		row.sid.empty() &&
		!normalised_route.empty() &&
		DoesSidOrRoutePointExist_(*airportSids, normalised_route.front()))
	{
		const std::string first = normalised_route.front();

		SetField_(validationResult,
			VF_SID,
			"SID Not Recognised",
			"Filed route begins with '" + first +
			"' but no valid SID was recognised. "
			"Possible invalid SID for active runway.");

		SetBlockedFields_(validationResult,
			{
				VF_DESTINATION,
				VF_EXIT_POINT,
				VF_ROUTE,
				VF_MIN_MAX,
				VF_ODD_EVEN,
				VF_SUFFIX,
				VF_RESTRICTIONS,
				VF_WARNINGS,
				VF_BANS
			},
			"Blocked because the route appears to contain a SID that EuroScope did not recognise.");

		SetFailed_(validationResult, "SID");

		LOG_DEBUG(
			"ValidateFlightPlan_: callsign='{}' result='SID' status={} reason='route starts with SID point but row.sid is blank' first='{}'",
			row.callsign,
			static_cast<int>(validationResult.status),
			first);

		return validationResult;
	}

	// ----------------------------------------------------------
	// 8. Find matching SID / non-SID route
	// ----------------------------------------------------------
	/********************************************************************************
	* This section attempts to find a matching SID or non-SID departure 
	* route in the loaded airport data, if available.
	* 
	* The logic is as follows:
	*  1. Decide whether this is a SID or non-SID departure.
	*  2. If it is a SID departure, try to match by SID/first waypoint.
	*  3. If it is non-SID, look for entry "point": "".
	*  4. Do not use row.first_waypoint.empty() alone as evidence that a SID exists.
	*********************************************************************************/
	size_t matched_sid_pos = std::string::npos;
	const rapidjson::Value* matched_sid = nullptr;
	const rapidjson::Value* constraints = nullptr;
	bool usable_sid_found = false;

	if (have_sids) {
		const bool is_non_sid_departure = row.sid.empty();

		if (is_non_sid_departure) {
			// Look for an explicit non-SID entry: { "point": "" }
			for (rapidjson::SizeType i = 0; i < airportSids->Size(); ++i) {
				const auto& sid_entry = (*airportSids)[i];
				if (!sid_entry.IsObject()) {
					continue;
				}

				const auto* pointVal = FindMemberValue(sid_entry, "point");
				if (pointVal && pointVal->IsString() &&
					std::string(pointVal->GetString()).empty()) {
					matched_sid_pos = static_cast<size_t>(i);
					break;
				}
			}
		}
		else {
			// SID departure: try to match by first waypoint / SID point.
			if (!first_waypoint_for_validation.empty()) {
				TryFindSidIndex_(*airportSids, first_waypoint_for_validation, matched_sid_pos);
			}
		}

		if (matched_sid_pos == std::string::npos) {
			if (is_non_sid_departure) {
				SetField_(validationResult, VF_SID, "SID Required",
					"Non-SID departure route not in database.");
			}
			else {
				SetField_(validationResult, VF_SID, "SID Not Found",
					row.sid + " departure not in database.");
			}

			SetBlockedFields_(validationResult,
				{
					VF_DESTINATION,
					VF_EXIT_POINT,
					VF_ROUTE,
					VF_MIN_MAX,
					VF_ODD_EVEN,
					VF_SUFFIX,
					VF_RESTRICTIONS,
					VF_WARNINGS,
					VF_BANS
				},
				"Blocked because no matching SID or non-SID route was found.");
		}
		else {
			const auto sid_index = static_cast<rapidjson::SizeType>(matched_sid_pos);
			matched_sid = &(*airportSids)[sid_index];

			if (!matched_sid->IsObject()) {
				SetField_(validationResult, VF_SID, "SID Data Invalid",
					"SID data entry is not an object.");

				SetBlockedFields_(validationResult,
					{
						VF_DESTINATION,
						VF_EXIT_POINT,
						VF_ROUTE,
						VF_MIN_MAX,
						VF_ODD_EVEN,
						VF_SUFFIX,
						VF_RESTRICTIONS,
						VF_WARNINGS,
						VF_BANS
					},
					"Blocked because SID data entry is invalid.");
			}
			else {
				constraints = FindArrayMember(*matched_sid, "constraints");
				if (!constraints || constraints->Empty()) {
					SetField_(validationResult, VF_SID, "SID Data Invalid",
						"SID data entry has no valid constraints array.");

					SetBlockedFields_(validationResult,
						{
							VF_DESTINATION,
							VF_EXIT_POINT,
							VF_ROUTE,
							VF_MIN_MAX,
							VF_ODD_EVEN,
							VF_SUFFIX,
							VF_RESTRICTIONS,
							VF_WARNINGS,
							VF_BANS
						},
						"Blocked because SID constraints are invalid.");
				}
				else {
					usable_sid_found = true;

					if (is_non_sid_departure) {
						SetField_(validationResult, VF_SID, "Passed", "Non-SID Route.");
					}
					else {
						SetField_(validationResult, VF_SID, "Passed", "SID - " + row.sid + ".");
					}
				}
			}
		}
	}
	
	// ----------------------------------------------------------
	// 9. Constraint / restriction processing
	// ----------------------------------------------------------
	int round = 0;
	std::vector<bool> validity;
	std::vector<bool> new_validity;
	std::vector<std::string> route_advisories;
	std::vector<size_t> direction_input_successes;
	std::vector<size_t> minmax_input_successes;
	SidRestrictionResult sid_restriction_result{};
	RestrictionFailureFlags constraint_restriction_fails{};
	bool warn = false;
	bool sid_level_time_advisory = false;
	bool sid_level_pass = true;
	bool sid_level_failure_can_be_overridden = false;
	bool rounds_ran = false;

	auto CollectSuccesses = [&](const std::vector<bool>& flags) {
		std::vector<size_t> successes;
		for (size_t i = 0; i < flags.size(); ++i) {
			if (flags[i]) {
				successes.push_back(i);
			}
		}
		return successes;
		};

	// If we have a usable SID/non-SID route with constraints,
	// attempt to validate against those constraints.
	if (usable_sid_found && matched_sid && constraints) {
		const auto* sidLvlRestrictions = FindArrayMember(*matched_sid, "restrictions");
		if (sidLvlRestrictions) {

			for (rapidjson::SizeType i = 0; i < sidLvlRestrictions->Size(); ++i) {
				const auto& r = (*sidLvlRestrictions)[i];

				LOG_TRACE("SID restriction [{}]: suffix={} types={} route={} start={} end={} alt={}",
					i,
					FindMemberValue(r, "suffix") != nullptr,
					FindMemberValue(r, "types") != nullptr,
					FindMemberValue(r, "route") != nullptr,
					FindMemberValue(r, "start") != nullptr,
					FindMemberValue(r, "end") != nullptr,
					FindMemberValue(r, "alt") != nullptr);
			}

			sid_restriction_result = EvaluateSidRestrictions_(row, *sidLvlRestrictions);

			sid_level_failure_can_be_overridden =
				sid_restriction_result.any_applicable &&
				!sid_restriction_result.passed &&
				SidLevelFailureCanBeOverriddenByConstraint_(row, *sidLvlRestrictions);

			sid_level_time_advisory =
				sid_restriction_result.passed &&
				sid_restriction_result.any_applicable &&
				MatchingSidRestrictionHasTimeWindow_(row, *sidLvlRestrictions);

			LOG_TRACE("SID-Level Restrictions: has_restrictions={} any_applicable={} passed={} fails: suffix={} type={} time={} banned={}",
				sid_restriction_result.has_restrictions,
				sid_restriction_result.any_applicable,
				sid_restriction_result.passed,
				sid_restriction_result.fails.suffix,
				sid_restriction_result.fails.type,
				sid_restriction_result.fails.time,
				sid_restriction_result.fails.banned);

			// Continue into normal constraint evaluation if:
			// 1) a SID-level restriction passed, or
			// 2) SID-level restrictions exist, but none are applicable to this flight.
			/*sid_level_pass = sid_restriction_result.passed ||
				!sid_restriction_result.any_applicable;*/

			sid_restriction_result = EvaluateSidRestrictions_(row, *sidLvlRestrictions);

			sid_level_failure_can_be_overridden =
				sid_restriction_result.any_applicable &&
				!sid_restriction_result.passed &&
				SidLevelFailureCanBeOverriddenByConstraint_(row, *sidLvlRestrictions);

			sid_level_pass =
				sid_restriction_result.passed ||
				!sid_restriction_result.any_applicable ||
				sid_level_failure_can_be_overridden;

		}
		else {
			sid_restriction_result.has_restrictions = false;
			sid_restriction_result.any_applicable = false;
			sid_restriction_result.passed = true;
			sid_level_pass = true;
		}

		if (sid_level_pass) {
			validity.assign(constraints->Size(), true);
			rounds_ran = true;
			while (round < 7) {
				new_validity.clear();
				switch (round) {
				case 0:
					new_validity = checkDestination(*constraints, row.destination, validity);
					break;

				case 1:
					new_validity = syntax_ok ? checkExitPoint(*constraints, normalised_route, validity) : validity;
					break;

				case 2:
					new_validity = CheckConstraintRestrictions_(row, *constraints,
						constraint_restriction_fails,
						validity);
					break;

				case 3:
					new_validity = syntax_ok ? checkRoute(*constraints, normalised_route, validity, &route_advisories) : validity;
					break;

				case 4:
					minmax_input_successes = CollectSuccesses(validity); 
					new_validity = checkMinMax(*constraints, row.rfl, validity);
					break;

				case 5:
					direction_input_successes = CollectSuccesses(validity); 
					new_validity = checkDirection(*constraints, row.rfl, validity);
					break;

				case 6:
					new_validity = checkAlerts(*constraints, &warn, validity, row.rfl);
					break;

				}
				if (new_validity.empty()) {
					LOG_TRACE("{} validation produced empty validity vector on round{}",
						row.callsign, round);
					break;
				}

				auto count_true = [](const std::vector<bool>& v) {
					return static_cast<int>(std::count(v.begin(), v.end(), true));
					};

				const char* round_name = "";
				switch (round) {
				case 0: round_name = "destination"; break;
				case 1: round_name = "exit_point"; break;
				case 2: round_name = "constraint_restrictions"; break;
				case 3: round_name = "route"; break;
				case 4: round_name = "min_max"; break;
				case 5: round_name = "direction"; break;
				case 6: round_name = "alerts"; break;
				}
				LOG_TRACE("{} round {} ({}): in={} out={}",
					row.callsign, round, round_name,
					count_true(validity), count_true(new_validity));

				if (std::all_of(new_validity.begin(), new_validity.end(), [](bool v) { return !v; })) {
					LOG_TRACE("{} validation failed on round {}", row.callsign, round);
					break;
				}
				validity = new_validity;
				++round;

			}
		}
	}

	// ----------------------------------------------------------
	// 10. Output generation
	// ----------------------------------------------------------
	if (rounds_ran && matched_sid && constraints) {
		std::vector<size_t> successes = CollectSuccesses(validity);

		auto RestrictionBanOutputForSuccesses = [&]() -> std::string {
			std::vector<std::string> bans;

			for (size_t each : successes) {
				if (each >= constraints->Size()) {
					continue;
				}

				const auto& condition =
					(*constraints)[static_cast<rapidjson::SizeType>(each)];

				if (!condition.IsObject()) {
					continue;
				}

				const auto* restrictionArr = FindArrayMember(condition, "restrictions");
				if (!restrictionArr || restrictionArr->Empty()) {
					continue;
				}

				for (const auto& restriction : restrictionArr->GetArray()) {
					if (!restriction.IsObject()) {
						continue;
					}

					RestrictionFailureFlags fails{};

					if (!RestrictionObjectMatches_(row, restriction, fails)) {
						continue;
					}

					const auto* bannedValue = FindMemberValue(restriction, "banned");
					if (!bannedValue || !bannedValue->IsBool() || !bannedValue->GetBool()) {
						continue;
					}

					const auto* altArr = FindArrayMember(restriction, "alt");
					if (altArr && !altArr->Empty()) {
						for (const auto& alt : altArr->GetArray()) {
							if (alt.IsString()) {
								bans.push_back(alt.GetString());
							}
						}
					}
					else {
						bans.push_back("Route is prohibited by restriction.");
					}
				}
			}

			std::sort(bans.begin(), bans.end());
			bans.erase(std::unique(bans.begin(), bans.end()), bans.end());

			if (bans.empty()) {
				return {};
			}

			std::string out = "Route Banned: ";
			for (const auto& ban : bans) {
				out += ban + RESULT_SEP;
			}

			out.erase(out.size() - RESULT_SEP.size());
			out += ".";
			return out;
			};

		const std::string restriction_ban_text = RestrictionBanOutputForSuccesses();
		const bool has_restriction_ban = !restriction_ban_text.empty();

		LOG_TRACE("{} SID-level final check: passed={} applicable={} overrideable={}",
			row.callsign,
			sid_restriction_result.passed,
			sid_restriction_result.any_applicable,
			sid_level_failure_can_be_overridden);

		if (!sid_restriction_result.passed &&
			sid_restriction_result.any_applicable &&
			!sid_level_failure_can_be_overridden)
		{
			const std::string text =
				"Failed " +
				RestrictionsOutput(*matched_sid,
					row,
					sid_restriction_result.fails.type,
					sid_restriction_result.fails.time,
					sid_restriction_result.fails.banned,
					successes);

			SetField_(validationResult, VF_RESTRICTIONS,
				"Failed Restrictions.",
				text);

			validationResult.status = ValidationStatus::Failed;
			validationResult.passed = false;
			validationResult.itemString = "RST";

			LOG_TRACE("{} hard SID-level restriction failure reapplied after constraint checks",
				row.callsign);

			return validationResult;
		}
		
		switch (round) {
		case 7:
		{
			if (has_restriction_ban) {
				SetField_(validationResult, VF_BANS,
					"Route Ban Present.",
					restriction_ban_text);
			}
			else {
				SetField_(validationResult, VF_BANS,
					"No Route Ban.",
					"No Route Ban.");
			}
		}
		[[fallthrough]];

		case 6:
		{
			if (warn || sid_level_time_advisory) {
				std::string warning_text;

				if (warn) {
					warning_text = WarningsOutput(*constraints, successes, row.route_points, row.destination, row.rfl);
				}

				if (sid_level_time_advisory) {
					if (!warning_text.empty() &&
						warning_text.back() != ' ') {
						warning_text += " ";
					}

					std::string window_text;

					if (const auto* sidLvlRestrictions = FindArrayMember(*matched_sid, "restrictions")) {
						window_text = MatchingSidRestrictionTimeWindowText_(row, *sidLvlRestrictions);
					}

					if (!window_text.empty()) {
						warning_text +=
							"Valid per filed EOBT. Confirm actual departure time if operating outside "
							+ window_text + ".";
					}
					else {
						warning_text +=
							"Valid per filed EOBT. Confirm actual departure time if operating outside the permitted time window.";
					}
				}

				SetField_(validationResult, VF_WARNINGS, "Warnings Present.", warning_text);
			}
			else {
				SetField_(validationResult, VF_WARNINGS, "No Warnings.", "No Warnings.");
			}

			if (round == 6) {
				std::string ban_text =
					BansOutput(*constraints, successes, row.route_points, row.destination, row.rfl);

				if (has_restriction_ban) {
					if (!ban_text.empty() && ban_text.find(NO_RESULTS) == std::string::npos) {
						ban_text += " ";
					}
					else {
						ban_text.clear();
					}

					ban_text += restriction_ban_text;
				}

				SetField_(validationResult, VF_BANS,
					"Route Ban Present.",
					ban_text);
			}

			SetField_(validationResult, VF_ODD_EVEN, "Passed",
				"Passed " + DirectionOutput(*constraints, successes));
		}
		[[fallthrough]];

		case 5:
		{
			if (round == 5) {
				std::set<std::string> required_dirs;

				const auto direction_successes =
					!direction_input_successes.empty()
					? direction_input_successes
					: successes;

				for (size_t idx : direction_successes) {
					if (idx >= constraints->Size()) {
						continue;
					}

					const auto& cond = (*constraints)[static_cast<rapidjson::SizeType>(idx)];
					const auto* dirObj = FindMemberValue(cond, "dir");

					if (!dirObj || !dirObj->IsString()) {
						continue;
					}

					std::string dir = to_upper(std::string(dirObj->GetString()));

					if (dir == EVEN_DIRECTION) {
						required_dirs.insert("Even");
					}
					else if (dir == ODD_DIRECTION) {
						required_dirs.insert("Odd");
					}
				}

				std::string required_text = "Any";
				if (!required_dirs.empty()) {
					required_text = fmt::format("{}", fmt::join(required_dirs, " or "));
				}

				SetField_(validationResult, VF_ODD_EVEN,
					"Failed Odd-Even Rule.",
					"Failed Odd-Even Rule. Required: " + required_text + ".");
			}

			SetField_(validationResult, VF_MIN_MAX, "Passed",
				"Passed " + MinMaxOutput(*constraints, successes));
		}
		[[fallthrough]];

		case 4:
		{
			if (round == 4) {
				const std::string text =
					LevelFailureDetail_(*constraints, minmax_input_successes, row);

				SetField_(validationResult, VF_MIN_MAX, "Failed MIN_MAX.", text);
			}

			SetField_(validationResult, VF_ROUTE, "Passed",
				"Passed Route. " +
				RouteOutput(*constraints, row.route_points, row.destination, row.rfl, true));
		}
		[[fallthrough]];

		case 3:
		{
			if (round == 3 && syntax_ok) {
				const std::string text =
					"Failed Route. " +
					RouteOutputShort_(*constraints, row.destination, row.rfl, row.route_points, 2);

				SetField_(validationResult, VF_ROUTE, "Failed Route.", text);

				for (const auto& advisory : route_advisories) {
					LOG_TRACE("Route advisory for '{}': {}", row.callsign, advisory);
				}

			}

			SetField_(validationResult, VF_RESTRICTIONS,
				"Passed",
				"SID Restrictions: Passed.");
			SetField_(validationResult, VF_SUFFIX, "Passed",
				"Valid " + SuffixOutput(*matched_sid, successes));
		}
		[[fallthrough]];

		case 2:
		{
			if (round == 2) {
				if (constraint_restriction_fails.suffix) {
					const std::string text = "Invalid " + SuffixOutput(*matched_sid, successes);
					SetField_(validationResult, VF_SUFFIX, "Failed Suffix Restriction.", text);
				}
				else {
					const std::string text =
						"Failed " +
						RestrictionsOutput(*matched_sid,
							row,
							constraint_restriction_fails.type,
							constraint_restriction_fails.time,
							constraint_restriction_fails.banned,
							successes);

					SetField_(validationResult, VF_RESTRICTIONS, "Failed Restrictions.", text);
					SetField_(validationResult, VF_SUFFIX, "Passed",
						"Valid " + SuffixOutput(*matched_sid, successes));
				}
			}

			if (syntax_ok) {
				SetField_(validationResult, VF_EXIT_POINT, "Passed",
					"Passed " + ExitPointOutput(airport, row.route_points));
			}
		}
		[[fallthrough]];

		case 1:
		{
			if (round == 1 && syntax_ok) {
				const std::string text =
					"Failed " + ExitPointOutput(airport, row.route_points);
				SetField_(validationResult, VF_EXIT_POINT, "Failed Exit Point.", text);
			}

			SetField_(validationResult, VF_DESTINATION, "Passed",
				"Passed " + DestinationOutput(airport, row.destination));
		}
		[[fallthrough]];

		case 0:
		{
			if (round == 0) {
				const std::string text =
					"Failed " + DestinationOutput(airport, row.destination);
				SetField_(validationResult, VF_DESTINATION, "Failed Destination", text);
			}
			break;
		}

		default:
			break;
		}
	}
	else if (usable_sid_found && matched_sid && constraints) {
		if (sid_restriction_result.fails.suffix) {
			const std::string text = "Invalid " + SuffixOutput(*matched_sid);

			SetField_(validationResult, VF_SUFFIX, "Invalid Suffix.", text);
		}
		else {
			SetField_(validationResult, VF_SUFFIX, "Passed",
				"Valid " + SuffixOutput(*matched_sid));

			const std::string text =
				"Failed " + RestrictionsOutput(*matched_sid,
					row,
					sid_restriction_result.fails.type,
					sid_restriction_result.fails.time,
					sid_restriction_result.fails.banned);

			const bool hard_sid_restriction_failure =
				sid_restriction_result.fails.type ||
				sid_restriction_result.fails.banned;

			if (hard_sid_restriction_failure) {
				SetField_(validationResult, VF_RESTRICTIONS,
					"Failed Restrictions.", text);
			}
			else {
				SetField_(validationResult, VF_RESTRICTIONS,
					"Restriction Advisory.", text);
			}
		}
	}

	// ----------------------------------------------------------
	// 11. Fill any remaining unset fields
	// ----------------------------------------------------------
	SetFieldIfUnset_(validationResult, VF_ORIGIN, "Not Checked",
		"Origin not checked due to earlier validation gating.");

	SetFieldIfUnset_(validationResult, VF_DESTINATION, "Not Checked",
		"Destination not checked due to earlier validation failure.");

	SetFieldIfUnset_(validationResult, VF_EXIT_POINT, "Not Checked",
		"Exit point not checked due to earlier validation failure.");

	SetFieldIfUnset_(validationResult, VF_ROUTE, "Not Checked",
		"Route not checked due to earlier validation failure.");

	SetFieldIfUnset_(validationResult, VF_MIN_MAX, "Not Checked",
		"Level constraints not checked due to earlier validation failure.");

	SetFieldIfUnset_(validationResult, VF_ODD_EVEN, "Not Checked",
		"Odd/even constraints not checked due to earlier validation failure.");

	SetFieldIfUnset_(validationResult, VF_SUFFIX, "Not Checked",
		"Suffix not checked due to earlier validation failure.");

	SetFieldIfUnset_(validationResult, VF_RESTRICTIONS, "Not Checked",
		"Restrictions not checked due to earlier validation failure.");

	SetFieldIfUnset_(validationResult, VF_WARNINGS, "Not Checked",
		"Warnings not checked due to earlier validation failure.");

	SetFieldIfUnset_(validationResult, VF_BANS, "Not Checked",
		"Bans not checked due to earlier validation failure.");

	SetFieldIfUnset_(validationResult, VF_SYNTAX, "Not Checked",
		"Syntax not checked due to earlier validation failure.");

	if (!route_advisories.empty()) {
		std::string warning_text = "Route advisory: ";

		for (const auto& warning : route_advisories) {
			warning_text += warning + RESULT_SEP;
		}

		warning_text.erase(warning_text.size() - RESULT_SEP.size());
		warning_text += ".";

		SetField_(validationResult,
			VF_WARNINGS,
			"Warnings Present.",
			warning_text);
	}

	FinalizeValidationResult_(validationResult);

	const std::string mnemonic =
		validationResult.itemString.empty() ? TagMnemonicFromResult_(validationResult)
		: validationResult.itemString;

	LOG_DEBUG("ValidateFlightPlan_: callsign='{}' result='{}' status={} sid='{}' dest='{}' route='{}'",
		row.callsign,
		mnemonic,
		static_cast<int>(validationResult.status),
		validationResult.fields[VF_SID].summary,
		validationResult.fields[VF_DESTINATION].summary,
		validationResult.fields[VF_ROUTE].summary);

	return validationResult;
}

CVFPCPlugin::ParsedFlightPlanData
CVFPCPlugin::CParseFlightPlanData_(const EuroScopePlugIn::CFlightPlan& flightPlan) const
{
	ParsedFlightPlanData data;

	data.eobt = flightPlan.GetFlightPlanData().GetEstimatedDepartureTime();
	data.origin = to_upper(flightPlan.GetFlightPlanData().GetOrigin());
	data.destination = to_upper(flightPlan.GetFlightPlanData().GetDestination());
	data.rfl = flightPlan.GetFlightPlanData().GetFinalAltitude();
	data.rawroute = flightPlan.GetFlightPlanData().GetRoute();
	trim(data.rawroute);
	data.route = split(data.rawroute, ' ');
	data.route.erase(std::remove_if(data.route.begin(), data.route.end(),
		[](const std::string& s) { return s.empty(); }), data.route.end());

	for (auto& token : data.route) {
		token = to_upper(token);
	}
	EuroScopePlugIn::CFlightPlanExtractedRoute extracted = flightPlan.GetExtractedRoute();

	for (int i = 0; i < extracted.GetPointsNumber(); ++i) {
		data.points.push_back(extracted.GetPointName(i));
	}

	data.sid = to_upper(flightPlan.GetFlightPlanData().GetSidName());
	if (!data.sid.empty()) {
		erase(data.sid, '#');
		if (data.origin == "EGLL" && data.sid == "CHK") {
			data.first_wp = "CPT";
			data.sid_suffix = "CHK";
		}
		else {
			data.first_wp = data.sid.substr(0, data.sid.find_first_of
			("0123456789"));
			if (!data.first_wp.empty()) {
				data.first_wp = to_upper(data.first_wp);
			}

			if (!data.sid.empty()) {
				data.sid_suffix = std::string(1, data.sid.back());
			}
		}
	}
	return data;
}

bool CVFPCPlugin::TryFindSidIndex_(const rapidjson::Value& sids,
	const std::string& first_wp,
	size_t& pos) const
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

		const auto* constraints = FindArrayMember(sid, "constraints");
		if (!constraints) {
			continue;
		}

		const auto* pointVal = FindMemberValue(sid, "point");
		if (pointVal && pointVal->IsString() &&
			first_wp == pointVal->GetString()) {
			pos = i;
			return true;
		}

		const auto* aliases = FindArrayMember(sid, "aliases");
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

bool CVFPCPlugin::DoesSidOrRoutePointExist_(
	const rapidjson::Value& sids,
	const std::string& point) const
{
	if (!sids.IsArray() || sids.Empty() || point.empty()) {
		return false;
	}

	const std::string wanted = to_upper(point);

	for (rapidjson::SizeType i = 0; i < sids.Size(); ++i) {
		const auto& sid = sids[i];

		if (!sid.IsObject()) {
			continue;
		}

		const auto* constraints = FindArrayMember(sid, "constraints");
		if (!constraints) {
			continue;
		}

		const auto* pointVal = FindMemberValue(sid, "point");
		if (!pointVal || !pointVal->IsString()) {
			continue;
		}

		if (to_upper(pointVal->GetString()) == wanted) {
			return true;
		}
	}

	return false;
}

bool CVFPCPlugin::NormaliseAndValidateRouteSyntax_(std::vector<std::string>& route,
	const std::string& origin,
	const std::string& destination,
	const std::string& sid,
	const std::string& first_wp,
	std::string& outchk) const
{
	std::regex spdlvl("(N|M|K)[0-9]{3,4}((A|F)[0-9]{3}|(S|M)[0-9]{4})");
	std::regex spdlvlslash("\\/(N|M|K)[0-9]{3,4}((A|F)[0-9]{3}|(S|M)[0-9]{4})((A|F)[0-9]{3}|(S|M)[0-9]{4})?");
	std::regex icaorwy("[A-Z]{4}(\\/[0-9]{2}(L|C|R)?)?");
	std::regex sidstarrwy("[A-Z]{2,5}[0-9][A-Z](\\/[0-9]{2}(L|C|R)?)?");
	std::regex dctspdlvl("DCT\\/(N|M|K)[0-9]{3,4}((A|F)[0-9]{3}|(S|M)[0-9]{4})");
	std::regex awy("(U)?[A-Z][0-9]{1,3}([A-Z])?");
	bool success = true;
	bool repeat = false;
	std::vector<std::string> new_route;
	for (size_t i = 0; i < 5; ++i) {
		if (!success) break;

		if (route.empty()) {
			outchk = "No Route";
			return false;
		}

		switch (i) {
		case 0:
			if (std::regex_match(route.front(), spdlvl)) {
				route.erase(route.begin());
			}
			break;

		case 1:
			do {
				repeat = false;
				if (route.empty()) {
					outchk = "No Route";
					return false;
				}
				if (std::regex_match(route.front(), sidstarrwy) ||
					route.front() == "SID") {
					route.erase(route.begin());
					repeat = true;
				}
				else if (std::regex_match(route.front(), icaorwy)) {
					if (route.front().substr(0, 4) == origin) {
						route.erase(route.begin());
						repeat = true;
					}
					else {
						outchk = "Different Origin in Route";
						return false;
					}
				}
			} while (repeat);
			break;

		case 2:
			do {
				repeat = false;
				if (route.empty()) {
					outchk = "No Route";
					return false;
				}
				if (std::regex_match(route.back(), sidstarrwy) ||
					route.back() == "STAR") {
					route.pop_back();
					repeat = true;
				}
				else if (std::regex_match(route.back(), icaorwy)) {
					if (route.back().substr(0, 4) == destination) {
						route.pop_back();
						repeat = true;
					}
					else {
						outchk = "Different Destination in Route";
						return false;
					}
				}
			} while (repeat);
			break;

		case 3:
			new_route.clear();
			for (const auto& each : route) {
				if (std::regex_match(each, dctspdlvl)) {
					return false;
				}
				if (each == "DCT") {
					continue;
				}
				if (std::regex_match(each, awy)) {
					new_route.push_back(each);
					continue;
				}
				const size_t slash = each.find('/');
				if (slash == std::string::npos) {
					new_route.push_back(each);
					continue;
				}
				const std::string chng = each.substr(slash);
				if (std::regex_match(chng, spdlvlslash)) {
					new_route.push_back(each.substr(0, slash));
				}
				else {
					outchk = "Invalid Speed/Level Change";
					return false;
				}
			}
			route = std::move(new_route);
			break;

		case 4:
			if (!sid.empty()) {
				if (route.empty()) {
					outchk = "No Route";
					return false;
				}

				if (route.front() != first_wp) {
					outchk = "Route Not From First Waypoint";
					return false;
				}

				route.erase(route.begin());
			}
			break;
		}
	}
	return true;
}

bool CVFPCPlugin::IsIfrFlightPlan_(const EuroScopePlugIn::CFlightPlan& flightPlan) const {
	std::string fpType{ flightPlan.GetFlightPlanData().GetPlanType() };
	fpType = to_upper(fpType);
	return fpType == "I";
}

CVFPCPlugin::ValidationResult
CVFPCPlugin::MakePendingResult_(const std::string& callsign,
	const std::string& normal,
	const std::string& debug) const
{
	ValidationResult result = initialiseResults(callsign);

	SetPending_(result, "PDG");
	result.fields[VF_SID].summary = normal;
	result.fields[VF_SID].full = debug;
	return result;
}

// 8b. Validation result helpers.
void CVFPCPlugin::SetResultField(vector<vector<string>>& out,
	ValidationField field,
	const string& normal,
	const string& debug)
{
	out[0][field] = normal;
	out[1][field] = debug;
}

void CVFPCPlugin::SetResultBoth(vector<vector<string>>& out,
	ValidationField field,
	const string& text)
{
	out[0][field] = text;
	out[1][field] = text;
}

vector<vector<string>> CVFPCPlugin::ReturnWithField(vector<vector<string>>& out,
	ValidationField field,
	const string& normal,
	const string& debug)
{
	SetResultField(out, field, normal, debug);
	SetResultBoth(out, VF_STATUS, "Failed");
	return out;
}

vector<vector<string>> CVFPCPlugin::ReturnWithBoth(vector<vector<string>>& out,
	ValidationField field,
	const string& text)
{
	SetResultBoth(out, field, text);
	SetResultBoth(out, VF_STATUS, "Failed");
	return out;
}

// 8c. Active-airport session helpers
void CVFPCPlugin::ResetActiveAirportState()
{
	{
		std::lock_guard<std::mutex> g(active_mtx_);
		active_airport_.clear();
		active_airport_locked_ = false;
		airport_data_requested_ = false;
		origin_counts_.clear();
	}
}

void CVFPCPlugin::TryLockActiveAirportFromFlightPlan_(const	FlightPlanRow& row)
{
	if (!row.is_ifr || row.origin.empty()) {
		return;
	}

	bool should_lock = false;
	std::string selected_airport;

	{
		std::lock_guard<std::mutex> g(active_mtx_);
		if (active_airport_source_ ==
			ActiveAirportSource::ControllerIdentity) {
			return;
		}

		// If airport was only inferred, allow it to be corrected
		// when another origin repeatedly appears.
		if (active_airport_locked_ &&
			active_airport_source_ == ActiveAirportSource::Inferred &&
			row.origin != active_airport_) {

			int& count = origin_counts_[row.origin];
			count++;

			LOG_TRACE("Inference correction: origin '{}' count={} while locked to '{}'",
				row.origin, count, active_airport_);

			const int SWITCH_THRESHOLD = 3;

			if (count >= SWITCH_THRESHOLD) {
				LOG_WARN("Changing inferred active airport from '{}' to '{}' after repeated mismatch",
					active_airport_, row.origin);

				active_airport_ = row.origin;
				active_airport_locked_ = true;
				active_airport_source_ = ActiveAirportSource::Inferred;
				airport_data_requested_ = false;
				origin_counts_.clear();

				airport_data_.store(nullptr, std::memory_order_release);
				airport_reload_requested_.store(true, std::memory_order_release);
			}

			return;
		}

		if (active_airport_locked_) {
			return;
		}

		int& count = origin_counts_[row.origin];
		count++;

		LOG_TRACE("Inference: origin '{}' count={}", row.origin, count);

		const int LOCK_THRESHOLD = 3;
		if (count >= LOCK_THRESHOLD) {
			active_airport_ = row.origin;
			active_airport_locked_ = true;
			active_airport_source_ = ActiveAirportSource::Inferred;
			airport_data_requested_ = false;
			should_lock = true;
			selected_airport = row.origin;
		}
	}

	if (should_lock) {
		LOG_INFO("Active airport inferred as '{}' after threshold reached", selected_airport);
		EnsureAirportDataRequested();
	}
}

void CVFPCPlugin::EnsureAirportDataRequested()
{
	std::string airport;
	{
		std::lock_guard<std::mutex> g(active_mtx_);
		if (!active_airport_locked_ || active_airport_.empty() || airport_data_requested_) {
			return;
		}
		airport = active_airport_;
		airport_data_requested_ = true;
	}
	LOG_INFO("Requesting airport data for '{}'", airport);
	getSids();
}

CVFPCPlugin::OwnControllerInfo CVFPCPlugin::GetOwnControllerInfo_()const
{
	OwnControllerInfo info{};

	EuroScopePlugIn::CController me = ControllerMyself();
	if (!me.IsValid()) {
		return info;
	}

	info.valid = true;
	info.rating = me.GetRating();
	const char* cs = me.GetCallsign();
	info.callsign = cs ? cs : "";

	return info;
}

CVFPCPlugin::OwnIdentityInfo CVFPCPlugin::GetOwnIdentityInfo_() const
{
	OwnIdentityInfo info{};
	const OwnControllerInfo self = GetOwnControllerInfo_();

	info.valid = self.valid;
	info.rating = self.rating;
	info.callsign = self.callsign;

	if (!self.valid) {
		info.kind = OwnIdentityKind::None;
		return info;
	}

	std::string upper = self.callsign;
	std::transform(upper.begin(), upper.end(), upper.begin(),
		[](unsigned char c) { return static_cast<char>(std::toupper
		(c)); });

	const auto pos = upper.find('_');
	if (pos != std::string::npos) {
		const std::string suffix = upper.substr(pos + 1);
		if (suffix == "OBS") {
			info.kind = OwnIdentityKind::Observer;
			return info;
		}
	}

	if (TryExtractAirportFromControllerCallsign_(self.callsign,
		info.airport_icao)) {
		info.kind = OwnIdentityKind::Controller;
		return info;
	}

	info.kind = OwnIdentityKind::Unknown;
	return info;
}

bool CVFPCPlugin::TryExtractAirportFromControllerCallsign_(const std::string& callsign,
	std::string& out_icao) const
{
	out_icao.clear();

	if (callsign.empty()) {
		return false;
	}

	const auto pos = callsign.find('_');
	if (pos == std::string::npos) {
		return false;
	}

	std::string prefix = callsign.substr(0, pos);
	std::string suffix = callsign.substr(pos + 1);
	std::transform(prefix.begin(), prefix.end(), prefix.begin(),
		[](unsigned char c) { return static_cast<char>(std::toupper
		(c)); });

	std::transform(suffix.begin(), suffix.end(), suffix.begin(),
		[](unsigned char c) { return static_cast<char>(std::toupper
		(c)); });

	if (suffix == "OBS") {
		return false;
	}

	if (prefix.size() != 4) {
		return false;
	}

	if (!std::all_of(prefix.begin(), prefix.end(),
		[](unsigned char c) { return std::isalpha(c) != 0; })) {
		return false;
	}

	out_icao = prefix;
	return true;
}

bool CVFPCPlugin::UpdateOwnIdentityIfChanged_(OwnIdentityInfo& out_info)
{
	out_info = GetOwnIdentityInfo_();
	const bool changed =
		(out_info.valid != last_own_identity_valid_) ||
		(out_info.rating != last_own_rating_) ||
		(out_info.callsign != last_own_callsign_);

	if (!changed) {
		return false;
	}

	LOG_INFO("Own identity changed: valid={} callsign='{}' rating={}",
		out_info.valid ? "true" : "false",
		out_info.callsign,
		out_info.rating);

	last_own_identity_valid_ = out_info.valid;
	last_own_rating_ = out_info.rating;
	last_own_callsign_ = out_info.callsign;

	return true;
}

const char* CVFPCPlugin::OwnIdentityKindToString_(OwnIdentityKind kind) const
{
	switch (kind) {
	case OwnIdentityKind::None: return "None";
	case OwnIdentityKind::Controller: return "Controller";
	case OwnIdentityKind::Observer: return "Observer";
	case OwnIdentityKind::Unknown: return "Unknown";
	default: return "Unknown";
	}
}

const char* CVFPCPlugin::ActiveAirportSourceToString_(ActiveAirportSource source) const
{
	switch (source) {
	case ActiveAirportSource::None: return "None";
	case ActiveAirportSource::ControllerIdentity: return "ControllerIdentity";
	case ActiveAirportSource::Inferred: return "Inferred";
	default: return "Unknown";
	}
}

void CVFPCPlugin::SetActiveAirport_(const std::string& airport, ActiveAirportSource source)
{
	if (airport.empty()) {
		return;
	}

	bool changed = false;
	std::string old_airport;
	ActiveAirportSource old_source = ActiveAirportSource::None;

	{
		std::lock_guard<std::mutex> g(active_mtx_);
		if (active_airport_ == airport &&
			active_airport_locked_ &&
			active_airport_source_ == source) {
			return;
		}
		old_airport = active_airport_;
		old_source = active_airport_source_;
		active_airport_ = airport;
		active_airport_locked_ = true;
		active_airport_source_ = source;
		airport_data_requested_ = false;
		changed = true;
	}

	if (changed) {
		LOG_INFO("Active airport changed from '{}' ({}) to '{}' ({})",
			old_airport,
			ActiveAirportSourceToString_(old_source),
			airport,
			ActiveAirportSourceToString_(source));
	}
}

void CVFPCPlugin::ClearActiveAirport_()
{
	if (active_airport_.empty() && active_airport_source_ ==
		ActiveAirportSource::None) {
		return;
	}

	LOG_INFO("Clearing active airport '{}' ({})",
		active_airport_,
		ActiveAirportSourceToString_(active_airport_source_));

	active_airport_.clear();
	active_airport_locked_ = false;
	active_airport_source_ = ActiveAirportSource::None;
	airport_data_requested_ = false;
	origin_counts_.clear();

}

void CVFPCPlugin::ResolveActiveAirport_(const OwnIdentityInfo& self)
{
	LOG_TRACE("ResolveActiveAirport_: kind={} callsign='{}' rating={} airport = '{}'",
		OwnIdentityKindToString_(self.kind),
		self.callsign,
		self.rating,
		self.airport_icao);

	if (self.kind == OwnIdentityKind::Controller &&
		!self.airport_icao.empty()) {

		SetActiveAirport_(self.airport_icao, ActiveAirportSource::ControllerIdentity);
		return;
	}

}

// 8d. Flight-plan tracking helpers
FlightPlanRow CVFPCPlugin::BuildFlightPlanRow_(const EuroScopePlugIn::CFlightPlan& flightPlan) const
{
	FlightPlanRow row;
	
	row.callsign = flightPlan.GetCallsign();
	row.is_ifr = IsIfrFlightPlan_(flightPlan);
	row.aircraft_type = flightPlan.GetFlightPlanData().GetAircraftFPType();
	row.engine_type = flightPlan.GetFlightPlanData().GetEngineType();
	const auto parsed = CParseFlightPlanData_(flightPlan);
	row.eobt = parsed.eobt;
	row.origin = parsed.origin;
	row.destination = parsed.destination;
	row.rfl = parsed.rfl;
	row.route = parsed.rawroute;
	row.route_points = parsed.route;
	row.sid = parsed.sid;
	row.sid_suffix = parsed.sid_suffix;
	row.first_waypoint = parsed.first_wp;

	// future navigation capability support
	row.remarks = flightPlan.GetFlightPlanData().GetRemarks();
	row.pbn = ExtractPbnFromRemarks(row.remarks);
	row.has_rnav1 = HasRnav1FromPbn(row.pbn);

	return row;
}

string CVFPCPlugin::BuildFlightPlanFingerprint_(const FlightPlanRow& row) const
{
	return row.callsign + "|" +
		row.eobt + "|" +
		row.origin + "|" +
		row.destination + "|" +
		row.route + "|" +
		row.sid + "|" +
		row.sid_suffix + "|" +
		row.first_waypoint + "|" +
		row.aircraft_type + "|" +
		row.engine_type + "|" +
		std::to_string(row.rfl) + "|" +
		(row.is_ifr ? "I" : "V");
}

void CVFPCPlugin::ProcessTrackedFlightPlan_(const FlightPlanRow& row)
{
	// ----------------------------------------------------------
	// Active airport inference (runs until locked)
	// ----------------------------------------------------------
	if (!active_airport_locked_) {
		TryLockActiveAirportFromFlightPlan_(row);
	}

	const auto now = std::chrono::steady_clock::now();
	const std::string fingerprint = BuildFlightPlanFingerprint_(row);
	const bool can_validate_now = (!row.is_ifr) || CanValidateFlightPlans_();

	bool should_validate = false;
	bool should_store_pending = false;

	{
		std::lock_guard<std::mutex> g(tracked_flightplans_mtx_);
		auto it = tracked_flightplans_.find(row.callsign);
		if (it == tracked_flightplans_.end())
		{
			TrackedFlightPlan tracked;
			tracked.fingerprint = fingerprint;
			tracked.last_seen = now;
			tracked.last_row = row;

			LOG_INFO(
				"Flight plan first seen: callsign='{}' origin='{}' dest='{}' eobt='{}' sid='{}' route='{}' rfl={} aircraft='{}' type='{}'",
				row.callsign,
				row.origin,
				row.destination,
				row.eobt,
				row.sid,
				row.route,
				row.rfl,
				row.aircraft_type,
				row.engine_type
			);

			tracked_flightplans_[row.callsign] = std::move(tracked);
			if (can_validate_now) should_validate = true;
			else should_store_pending = true;
		}
		else
		{
			it->second.last_seen = now;
			const bool changed = (it->second.fingerprint != fingerprint);
			if (changed)
			{
				it->second.fingerprint = fingerprint;

				{
					std::lock_guard<std::mutex> og(controller_override_mtx_);
					controller_override_callsigns_.erase(row.callsign);
				}

				it->second.last_row = row;
				if (can_validate_now) should_validate = true;
				else should_store_pending = true;
			}
			else if (!it->second.last_result.ready && can_validate_now) {
				should_validate = true;
			}
		}
	}
	if (should_store_pending) {
		ValidationResult pending = initialiseResults(row.callsign);
		pending.fields[VF_SID].summary = "Loading";
		pending.fields[VF_SID].full = "Airport data not loaded yet.";
		pending.fields[VF_STATUS].summary = "Pending";
		pending.fields[VF_STATUS].full = "Pending";
		pending.itemString = "PDG";
		pending.ready = false;
		pending.passed = false;
		pending.status = ValidationStatus::Pending;
		std::lock_guard<std::mutex> g(tracked_flightplans_mtx_);
		auto it = tracked_flightplans_.find(row.callsign);
		if (it != tracked_flightplans_.end()) {
			it->second.last_result = std::move(pending);
		}
		return;
	}

	if (!should_validate) {
		return;
	}

	auto result = ValidateFlightPlan_(row);
	result.itemString = TagMnemonicFromResult_(result);

	const std::string result_fingerprint =
		result.itemString + "|" +
		result.fields[VF_STATUS].summary + "|" +
		result.fields[VF_SID].summary + "|" +
		result.fields[VF_DESTINATION].summary + "|" +
		result.fields[VF_EXIT_POINT].summary + "|" +
		result.fields[VF_ROUTE].summary + "|" +
		result.fields[VF_MIN_MAX].summary + "|" +
		result.fields[VF_ODD_EVEN].summary + "|" +
		result.fields[VF_SUFFIX].summary + "|" +
		result.fields[VF_RESTRICTIONS].summary + "|" +
		result.fields[VF_WARNINGS].summary + "|" +
		result.fields[VF_BANS].summary + "|" +
		result.fields[VF_SYNTAX].summary;

	{
		std::lock_guard<std::mutex> g(tracked_flightplans_mtx_);
		auto it = tracked_flightplans_.find(row.callsign);
		if (it != tracked_flightplans_.end()) {
			if (!it->second.has_logged_result ||
				it->second.last_logged_result_fingerprint != result_fingerprint)
			{
				LOG_INFO(
					"Validation result: callsign='{}' result='{}' status='{}' sid='{}' dest='{}' exit='{}' route='{}' level='{}' odd_even='{}' restrictions='{}' warnings='{}' bans='{}'",
					row.callsign,
					result.itemString,
					result.fields[VF_STATUS].summary,
					result.fields[VF_SID].summary,
					result.fields[VF_DESTINATION].summary,
					result.fields[VF_EXIT_POINT].summary,
					result.fields[VF_ROUTE].summary,
					result.fields[VF_MIN_MAX].summary,
					result.fields[VF_ODD_EVEN].summary,
					result.fields[VF_RESTRICTIONS].summary,
					result.fields[VF_WARNINGS].summary,
					result.fields[VF_BANS].summary
				);

				it->second.last_logged_result_fingerprint = result_fingerprint;
				it->second.has_logged_result = true;
			}

			it->second.last_result = std::move(result);
		}
	}
}

void CVFPCPlugin::RevalidateTrackedFlightPlans_(const char* reason)
{
	std::vector<FlightPlanRow> rows;

	{
		std::lock_guard<std::mutex> g(tracked_flightplans_mtx_);

		rows.reserve(tracked_flightplans_.size());

		for (const auto& [callsign, tracked] : tracked_flightplans_) {
			rows.push_back(tracked.last_row);
		}
	}

	LOG_INFO("Revalidating {} tracked flight plan(s): {}",
		rows.size(),
		reason ? reason : "airport data changed");

	for (const auto& row : rows) {
		auto result = ValidateFlightPlan_(row);
		result.itemString = TagMnemonicFromResult_(result);

		std::lock_guard<std::mutex> g(tracked_flightplans_mtx_);
		auto it = tracked_flightplans_.find(row.callsign);
		if (it != tracked_flightplans_.end()) {
			it->second.last_result = std::move(result);
		}
	}
}

void CVFPCPlugin::PruneTrackedFlightPlans_()
{
	const auto now = std::chrono::steady_clock::now();
	constexpr auto timeout = std::chrono::seconds(10);

	std::lock_guard<std::mutex> g(tracked_flightplans_mtx_);
	for (auto it = tracked_flightplans_.begin(); it != tracked_flightplans_.end(); ) {
		if (now - it->second.last_seen > timeout)
		{
			LOG_TRACE("Removing stale flight plan '{}'", it->first);
			it = tracked_flightplans_.erase(it);
		}
		else
		{
			++it;
		}
	}
}

void CVFPCPlugin::ResetTrackedFlightPlans_()
{
	{
		std::lock_guard<std::mutex> g(tracked_flightplans_mtx_);
		tracked_flightplans_.clear();
	}

	{
		std::lock_guard<std::mutex> g(checks_disabled_mtx_);
		checks_disabled_callsigns_.clear();
	}

	{
		std::lock_guard<std::mutex> g(skipped_airport_log_mtx_);
		skipped_airport_logged_.clear();
	}
}

bool CVFPCPlugin::CanValidateFlightPlans_() const
{
	if (!active_airport_locked_) {
		return false;
	}

	auto snap = airport_data_.load(std::memory_order_acquire);
	if (!snap) {
		return false;
	}

	if (!snap->airport_json.IsObject()) {
		return false;
	}

	return true;
}

// 8e. Validation result helpers
void CVFPCPlugin::SetPending_(ValidationResult& result, const std::string& text) const
{
	result.ready = false;
	result.passed = false;
	result.status = ValidationStatus::Pending;
	result.itemString = text.empty() ? "PDG" : text;
	result.fields[VF_STATUS].summary = "Pending";
	result.fields[VF_STATUS].full = "Pending";
}

void CVFPCPlugin::SetPassed_(ValidationResult& result, const std::string& text) const
{
	result.ready = true;
	result.passed = true;
	result.status = ValidationStatus::Passed;
	result.itemString = text;
	result.fields[VF_STATUS].summary = "Passed";
	result.fields[VF_STATUS].full = "Passed";
}

void CVFPCPlugin::SetFailed_(ValidationResult& result, const std::string& text) const
{
	result.ready = true;
	result.passed = false;
	result.status = ValidationStatus::Failed;
	result.itemString = text;
	result.fields[VF_STATUS].summary = "Failed";
	result.fields[VF_STATUS].full = "Failed";
}

void CVFPCPlugin::SetNotChecked_(ValidationResult& result, const std::string& text) const
{
	result.ready = true;
	result.passed = false;
	result.status = ValidationStatus::NotChecked;
	result.itemString = text;
	result.fields[VF_STATUS].summary = "Not Checked";
	result.fields[VF_STATUS].full = "Not Checked";
}

void CVFPCPlugin::SetField_(ValidationResult& result,
	ValidationField field,
	const std::string& normal,
	const std::string& debug) const
{
	result.fields[field].summary = normal;
	result.fields[field].full = debug;
}

void CVFPCPlugin::SetFieldIfUnset_(ValidationResult& result,
	ValidationField field,
	const std::string& normal,
	const std::string& debug) const
{
	if (result.fields[field].summary == "-" || result.fields[field].summary.empty()) {
		result.fields[field].summary = normal;
		result.fields[field].full = debug;
	}
}

void CVFPCPlugin::SetBlockedFields_(ValidationResult& result,
	std::initializer_list<ValidationField> fields,
	const std::string& reason) const
{
	for (ValidationField field : fields) {
		SetFieldIfUnset_(result, field, "Not Checked", reason);
	}
}

bool CVFPCPlugin::FieldIndicatesFailure_(const ValidationResult::Field& field) const
{
	if (field.summary == "ERR") return true;
	if (field.summary.rfind("Failed", 0) == 0) return true;
	if (field.summary == "Route Ban Present.") return true;
	if (field.summary == "No SIDs") return true;
	if (field.summary == "SID Required") return true;
	if (field.summary == "SID Not Found") return true;
	if (field.summary == "SID Data Invalid") return true;
	if (field.summary == "Invalid Suffix.") return true;
	return false;
}

void CVFPCPlugin::FinalizeValidationResult_(ValidationResult& result) const
{
	bool any_failure = false;
	bool any_pending = false;

	for (size_t i = 0; i < VF_FIELD_COUNT; ++i) {
		if (i == VF_CALLSIGN || i == VF_STATUS) {
			continue;
		}

		if (FieldIndicatesFailure_(result.fields[i])) {
			any_failure = true;
		}

		const std::string& summary = result.fields[i].summary;
		if (summary == "Loading" || summary == "Pending" || summary == "...") {
			any_pending = true;
		}
	}

	result.ready = true;

	if (any_failure) {
		result.status = ValidationStatus::Failed;
		result.passed = false;
		result.fields[VF_STATUS].summary = "Failed";
		result.fields[VF_STATUS].full = "Failed";
	}
	else if (any_pending) {
		result.status = ValidationStatus::Pending;
		result.passed = false;
		result.fields[VF_STATUS].summary = "Pending";
		result.fields[VF_STATUS].full = "Pending";
	}
	else {
		result.status = ValidationStatus::Passed;
		result.passed = true;
		result.fields[VF_STATUS].summary = "Passed";
		result.fields[VF_STATUS].full = "Passed";
	}
}

bool CVFPCPlugin::IsUsefulSummaryText_(const std::string& text) const
{
	return !text.empty() &&
		text != "-" &&
		text != "OK" &&
		text != "Passed" &&
		text != "No Warnings." &&
		text != "No Route Ban." &&
		text != "Not Checked";
}

std::string CVFPCPlugin::SummaryDetail_(const ValidationResult& result, 
ValidationField field) const
{
	const auto& f = result.fields[field];

	if (!FieldNeedsAttention_(result, field)) {
		return {};
	}

	if (IsUsefulSummaryText_(f.full)) {
		return f.full;
	}

	if (IsUsefulSummaryText_(f.summary)) {
		return f.summary;
	}

	return {};
}

//==============================================================
// 9. Validation logic helpers
//==============================================================
bool CVFPCPlugin::RouteContainsAnyPoint_(const rapidjson::Value& arr, const std::vector<std::string>& route) const
{
	if (!arr.IsArray()) return false;

	for (const auto& point : route) {
		if (arrayContainsString(arr, point)) {
			return true;
		}
	}
	return false;
}

bool CVFPCPlugin::SidLevelRestrictionRouteMatches_(
	const rapidjson::Value& restrictedRoutes,
	const std::vector<std::string>& route,
	const std::string& firstWaypoint) const
{
	const std::string route_text = fmt::format("{}", fmt::join(route, " "));

	LOG_TRACE("SidLevelRestrictionRouteMatches_: called firstWaypoint='{}' route='{}' restrictedRoutes size={}",
		firstWaypoint,
		route_text,
		restrictedRoutes.IsArray() ? restrictedRoutes.Size() : 0);

	if (!restrictedRoutes.IsArray() || route.empty() || firstWaypoint.empty()) {
		return false;
	}

	const std::string sidPoint =
		StripRouteTokenSuffixForValidation_(firstWaypoint);

	for (size_t i = 0; i + 1 < route.size(); ++i) {
		if (route[i] != sidPoint) {
			continue;
		}

		std::vector<std::string> filed_after_sid;
		for (size_t j = i + 1; j < route.size(); ++j) {
			filed_after_sid.push_back(StripRouteTokenSuffixForValidation_(route[j]));
		}

		for (const auto& item : restrictedRoutes.GetArray()) {
			if (!item.IsString()) {
				continue;
			}

			std::vector<std::string> restricted_tokens =
				split(item.GetString(), ' ');

			restricted_tokens.erase(
				std::remove_if(
					restricted_tokens.begin(),
					restricted_tokens.end(),
					[](const std::string& s) { return s.empty(); }),
				restricted_tokens.end());

			if (restricted_tokens.empty()) {
				continue;
			}

			if (restricted_tokens.size() > filed_after_sid.size()) {
				continue;
			}

			bool matches = true;

			for (size_t k = 0; k < restricted_tokens.size(); ++k) {
				if (StripRouteTokenSuffixForValidation_(restricted_tokens[k]) != filed_after_sid[k]) {
					matches = false;
					break;
				}
			}

			if (matches) {
				LOG_TRACE("SidLevelRestrictionRouteMatches_: matched restricted route '{}'",
					item.GetString());
				return true;
			}
		}

		return false;
	}

	return false;
}

bool CVFPCPlugin::MatchingSidRestrictionHasTimeWindow_(
	const FlightPlanRow& row,
	const rapidjson::Value& restrictions) const
{
	if (!restrictions.IsArray()) {
		return false;
	}

	for (const auto& restriction : restrictions.GetArray()) {
		if (!restriction.IsObject()) {
			continue;
		}

		RestrictionFailureFlags fails{};

		if (!RestrictionObjectMatches_(row, restriction, fails)) {
			continue;
		}

		const auto* startObj = FindMemberValue(restriction, "start");
		const auto* endObj = FindMemberValue(restriction, "end");

		return startObj && endObj &&
			startObj->IsObject() &&
			endObj->IsObject();
	}

	return false;
}

std::string CVFPCPlugin::FormatRestrictionTimeWindow_(
	const rapidjson::Value& restriction) const
{
	const auto* startObj = FindMemberValue(restriction, "start");
	const auto* endObj = FindMemberValue(restriction, "end");

	if (!startObj || !endObj ||
		!startObj->IsObject() ||
		!endObj->IsObject()) {
		return {};
	}

	const auto* startTime = FindMemberValue(*startObj, "time");
	const auto* endTime = FindMemberValue(*endObj, "time");

	if (!startTime || !endTime ||
		!startTime->IsString() ||
		!endTime->IsString()) {
		return {};
	}

	std::string start = startTime->GetString();
	std::string end = endTime->GetString();

	if (start.size() != 4 || end.size() != 4) {
		return {};
	}

	return start.substr(0, 2) + ":" + start.substr(2, 2) +
		" - " +
		end.substr(0, 2) + ":" + end.substr(2, 2);
}

std::string CVFPCPlugin::MatchingSidRestrictionTimeWindowText_(
	const FlightPlanRow& row,
	const rapidjson::Value& restrictions) const
{
	if (!restrictions.IsArray()) {
		return {};
	}

	for (const auto& restriction : restrictions.GetArray()) {
		if (!restriction.IsObject()) {
			continue;
		}

		RestrictionFailureFlags fails{};

		if (!RestrictionObjectMatches_(row, restriction, fails)) {
			continue;
		}

		const std::string window = FormatRestrictionTimeWindow_(restriction);
		if (!window.empty()) {
			return window;
		}
	}

	return {};
}

//==============================================================
// 10. Validation output builders
//==============================================================
//Outputs route bans as string
std::string CVFPCPlugin::BansOutput(const rapidjson::Value& constraints,
	std::vector<size_t> successes,
	std::vector<std::string> extracted_route,
	std::string dest,
	int rfl) const
{
	std::vector<std::string> bans{};

	for (size_t each : successes) {
		if (each >= constraints.Size()) {
			continue;
		}

		const auto& constraint = constraints
			[static_cast<rapidjson::SizeType>(each)];
		if (!constraint.IsObject()) {
			continue;
		}

		const auto* alertsArr = FindArrayMember(constraint, "alerts");
		if (!alertsArr || alertsArr->Empty()) {
			continue;
		}

		for (rapidjson::SizeType i = 0; i < alertsArr->Size(); ++i) {
			const auto& alert = (*alertsArr)[i];
			if (!alert.IsObject()) {
				continue;
			}

			const auto* banVal = FindMemberValue(alert, "ban");
			if (!banVal || !banVal->IsBool() || !banVal->GetBool()) {
				continue;
			}

			const auto* srdVal = FindMemberValue(alert, "srd");
			if (srdVal && srdVal->IsInt()) {
				bans.push_back("SRD Note " + to_string(srdVal->GetInt
				()));
			}

			const auto* noteVal = FindMemberValue(alert, "note");
			if (noteVal && noteVal->IsString()) {
				bans.push_back(noteVal->GetString());
			}
			else {
				bans.push_back("Alternative Route: " +
					RouteOutput(constraints, extracted_route, dest,
						rfl, true));
			}
		}
	}

	sort(bans.begin(), bans.end());
	auto itr = unique(bans.begin(), bans.end());
	bans.erase(itr, bans.end());
	string out;

	for (const string& each : bans) {
		out += each + RESULT_SEP;
	}

	if (out.empty()) {
		out = NO_RESULTS;
	}
	else {
		out.erase(out.size() - RESULT_SEP.size());
	}
	return "Route Banned: " + out + ".";
}

//Outputs route warnings as string
std::string CVFPCPlugin::WarningsOutput(const rapidjson::Value& constraints,
	std::vector<size_t> successes,
	std::vector<std::string> extracted_route,
	std::string dest,
	int rfl) const
{
	std::vector<std::string> warnings{};

	for (size_t each : successes) {
		if (each >= constraints.Size()) {
			continue;
		}

		const auto& constraint = constraints
			[static_cast<rapidjson::SizeType>(each)];
		if (!constraint.IsObject()) {
			continue;
		}

		const auto* alertsArr = FindArrayMember(constraint, "alerts");
		if (!alertsArr || alertsArr->Empty()) {
			continue;
		}

		for (size_t i = 0; i < alertsArr->Size(); i++) {

			const auto& alert = (*alertsArr)[i];
			if (!alert.IsObject()) {
				continue;
			}

			const auto* warnVal = FindMemberValue(alert, "warn");
			if (!warnVal || !warnVal->IsBool() || !warnVal->
				GetBool()) {
				continue;
			}

			const auto* srdVal = FindMemberValue(alert, "srd");
			if (srdVal && srdVal->IsInt()) {
				warnings.push_back("SRD Note " + to_string(srdVal->GetInt()));
			}

			const auto* noteVal = FindMemberValue(alert, "note");
			if (noteVal && noteVal->IsString()) {
				warnings.push_back(noteVal->GetString());
			}
			else {
				warnings.push_back("Alternative Route: " +
					RouteOutput(constraints, extracted_route, dest,
						rfl, true));
			}
		}
	}

	std::sort(warnings.begin(), warnings.end());
	vector<string>::iterator itr = unique(warnings.begin(), warnings.end());
	warnings.erase(itr, warnings.end());
	string out = "";

	for (string each : warnings) {
		out += each + RESULT_SEP;
	}

	if (out == "") {
		out = NO_RESULTS;
	}
	else {
		out = out.substr(0, out.length() - 2);
	}

	return "Warnings: " + out + ".";
}

std::string CVFPCPlugin::AlternativesOutput(const rapidjson::Value& sid_ele,
	std::vector<size_t> successes)const
{
	vector<string> alts{};

	const auto* sidRestrictions = FindArrayMember(sid_ele, "restrictions");
	if (sidRestrictions) {
		vector<string> temp = AlternativesSingle(*sidRestrictions);
		alts.insert(alts.end(), temp.begin(), temp.end());
	}

	const auto* constraints = FindArrayMember(sid_ele, "constraints");
	if (constraints) {
		for (size_t each : successes) {
			if (each >= constraints->Size()) {
				continue;
			}
			const auto& constraint = (*constraints)
				[static_cast<rapidjson::SizeType>(each)];
			if (!constraint.IsObject()) {
				continue;
			}
			const auto* restrictionArr = FindArrayMember(constraint, "restrictions");
			if (!restrictionArr) {
				continue;
			}
			std::vector<std::string> temp = AlternativesSingle(*restrictionArr);
			alts.insert(alts.end(), temp.begin(), temp.end());
		}
	}

	std::sort(alts.begin(), alts.end());
	std::vector<std::string>::iterator itr = unique(alts.begin(), alts.end());
	alts.erase(itr, alts.end());

	if (alts.empty()) {
		return "Recommended Alternatives: " + std::string(NO_RESULTS) +
			".";
	}

	std::string out = "Recommended Alternatives: ";
	for (const std::string& each : alts) {
		out += each + RESULT_SEP;
	}

	out.erase(out.size() - RESULT_SEP.size());
	out += ".";
	return out;
}

std::vector<std::string> CVFPCPlugin::AlternativesSingle(const rapidjson::Value& restrictions)const
{
	std::vector<std::string> alts{};

	if (!restrictions.IsArray() || restrictions.Empty()) {
		return alts;
	}

	for (rapidjson::SizeType i = 0; i < restrictions.Size(); ++i) {
		const auto& restriction = restrictions[i];
		if (!restriction.IsObject()) {
			continue;
		}

		const auto* altArr = FindArrayMember(restriction, "alt");
		if (!altArr || altArr->Empty()) {
			continue;
		}

		for (rapidjson::SizeType j = 0; j < altArr->Size(); ++j) {
			const auto& alt = (*altArr)[j];
			if (alt.IsString()) {
				alts.push_back(alt.GetString());
			}
		}
	}
	return alts;
}

//Outputs aircraft type and date/time restrictions (from Restrictions array) as string
std::string CVFPCPlugin::RestrictionsOutput(const rapidjson::Value& sid_ele,
	const FlightPlanRow& row,
	const bool check_type,
	const bool check_time,
	const bool check_ban,
	const std::vector<size_t> successes) const
{
	std::vector<std::vector<std::string>> rests{};

	if (const auto* sidRestrictions = FindArrayMember(sid_ele, "restrictions")) {
		auto temp = RestrictionsSingle(*sidRestrictions, row, check_type, check_time, check_ban);
		rests.insert(rests.end(), temp.begin(), temp.end());
	}

	if (const auto* constraints = FindArrayMember(sid_ele, "constraints")) {
		for (size_t each : successes) {
			if (each >= constraints->Size()) continue;

			const auto& constraint = (*constraints)[static_cast<rapidjson::SizeType>(each)];
			if (!constraint.IsObject()) continue;

			if (const auto* restrictionArr = FindArrayMember(constraint, "restrictions")) {
				auto temp = RestrictionsSingle(*restrictionArr, row, check_type, check_time, check_ban);
				rests.insert(rests.end(), temp.begin(), temp.end());
			}
		}
	}

	sort(rests.begin(), rests.end());
	auto itr = unique(rests.begin(), rests.end());
	rests.erase(itr, rests.end());

	// If we have both:
	//   "A21N is not permitted on this SID"
	// and:
	//   "Use GOSAM instead because A21N is not permitted on this SID"
	// keep only the more useful alternative-routing message.
	if (check_type && !row.aircraft_type.empty()) {
		const std::string generic_type_failure =
			row.aircraft_type + " is not permitted on this SID";

		const bool has_better_alt_message =
			std::any_of(rests.begin(), rests.end(),
				[&](const std::vector<std::string>& r) {
					return !r.empty()
						&& r[0].find("Use ") != std::string::npos
						&& r[0].find(" because " + generic_type_failure) != std::string::npos;
				});

		if (has_better_alt_message) {
			rests.erase(
				std::remove_if(rests.begin(), rests.end(),
					[&](const std::vector<std::string>& r) {
						return !r.empty() && r[0] == generic_type_failure;
					}),
				rests.end());
		}
	}

	std::string out;

	for (size_t i = 0; i < rests.size(); ++i) {
		std::string temp;

		if (check_ban && !rests[i][2].empty()) {
			temp += rests[i][2];
		}

		if (check_type && !rests[i][0].empty()) {
			if (!temp.empty()) temp += " for ";
			temp += rests[i][0];
		}

		if (check_time && !rests[i][1].empty()) {
			if (!temp.empty()) temp += " between ";
			else temp += "Between ";
			temp += rests[i][1];
		}

		if (!temp.empty()) {
			out += temp + RESULT_SEP;
		}
	}

	if (out.empty()) {
		return "SID Restrictions: " + std::string(NO_RESULTS) + ".";
	}

	out.erase(out.size() - RESULT_SEP.size());
	return "SID Restrictions: " + out + ".";
}

std::vector<std::vector<std::string>> CVFPCPlugin::RestrictionsSingle(
	const rapidjson::Value& restrictions,
	const FlightPlanRow& row,
	bool check_type,
	bool check_time,
	bool check_ban) const
{
	std::vector<std::vector<std::string>> rests{};

	if (!restrictions.IsArray() || restrictions.Empty()) {
		return rests;
	}

	std::string fallback_time_window;

	for (rapidjson::SizeType i = 0; i < restrictions.Size(); ++i) {
		const auto& r = restrictions[i];

		if (!r.IsObject()) {
			continue;
		}

		fallback_time_window = FormatRestrictionTimeWindow_(r);

		if (!fallback_time_window.empty()) {
			break;
		}
	}

	for (rapidjson::SizeType i = 0; i < restrictions.Size(); ++i) {
		const auto& restriction = restrictions[i];
		if (!restriction.IsObject()) {
			continue;
		}

		std::vector<std::string> this_rest{ "", "", "" };

		if (check_type) {
			if (const auto* typesArr = FindArrayMember(restriction, "types")) {
				std::string altText;

				if (const auto* altArr = FindArrayMember(restriction, "alt");
					altArr && !altArr->Empty()) {

					for (rapidjson::SizeType j = 0; j < altArr->Size(); ++j) {
						if (!(*altArr)[j].IsString()) {
							continue;
						}

						if (!altText.empty()) {
							altText += RESULT_SEP;
						}

						altText += (*altArr)[j].GetString();
					}
				}

				if (!altText.empty()) {
					this_rest[0] = fmt::format(
						"Use {} instead because {} is not permitted on this SID",
						altText,
						row.aircraft_type);
				}
				else {
					this_rest[0] = fmt::format(
						"{} is not permitted on this SID",
						row.aircraft_type);
				}
			}
		}

		// If the failure is time-based and an alternative exists,
		// do not say the aircraft type is invalid.
		// The type is only identifying which aircraft the time rule applies to.
		if (check_time) {
			std::string altText;

			if (const auto* altArr = FindArrayMember(restriction, "alt");
				altArr && !altArr->Empty()) {

				for (rapidjson::SizeType j = 0; j < altArr->Size(); ++j) {
					if (!(*altArr)[j].IsString()) {
						continue;
					}

					if (!altText.empty()) {
						altText += RESULT_SEP;
					}

					altText += (*altArr)[j].GetString();
				}
			}

			if (!altText.empty()) {
				std::string window = FormatRestrictionTimeWindow_(restriction);

				if (window.empty()) {
					window = fallback_time_window;
				}

				this_rest[0] = fmt::format(
					"Use {} instead because this SID is only available during {} Zulu",
					altText,
					window.empty() ? "the permitted time window (Zulu)" : window);
			}
		}
		else if (check_type) {
			std::string altText;

			if (const auto* altArr = FindArrayMember(restriction, "alt");
				altArr && !altArr->Empty()) {

				for (rapidjson::SizeType j = 0; j < altArr->Size(); ++j) {
					if (!(*altArr)[j].IsString()) {
						continue;
					}

					if (!altText.empty()) {
						altText += RESULT_SEP;
					}

					altText += (*altArr)[j].GetString();
				}
			}

			if (!altText.empty()) {
				this_rest[0] = fmt::format(
					"Use {} instead because {} is not permitted on this SID",
					altText,
					row.aircraft_type);
			}
			else {
				this_rest[0] = fmt::format(
					"{} is not permitted on this SID",
					row.aircraft_type);
			}
		}

		if (check_ban) {
			this_rest[2] = "Not permitted";
		}

		if (!this_rest[0].empty() || !this_rest[1].empty() || !this_rest[2].empty()) {
			rests.push_back(this_rest);
		}
	}

	return rests;
}

//Outputs valid suffices (from Restrictions array) as string
std::string CVFPCPlugin::SuffixOutput(const rapidjson::Value& sid_ele, const std::vector<size_t> successes) const
{
	std::vector<std::string> suffices{};
	// SID-level restrictions
	if (const auto* sidRestrictions = FindArrayMember(sid_ele, "restrictions")) {
		std::vector<std::string> temp = SuffixSingle(*sidRestrictions);
		suffices.insert(suffices.end(), temp.begin(), temp.end());
	}

	// Constraint-level restrictions
	if (const auto* constraints = FindArrayMember(sid_ele, "constraints")) {
		for (size_t each : successes) {
			if (each >= constraints->Size()) {
				continue;
			}

			const auto& constraint = (*constraints)
				[static_cast<rapidjson::SizeType>(each)];
			if (!constraint.IsObject()) {
				continue;
			}

			if (const auto* restrictionArr = FindArrayMember(constraint, "restrictions")) {
				std::vector<std::string> temp = SuffixSingle(*restrictionArr);
				suffices.insert(suffices.end(), temp.begin(), temp.end());
			}
		}
	}

	sort(suffices.begin(), suffices.end());
	auto itr = unique(suffices.begin(), suffices.end());
	suffices.erase(itr, suffices.end());
	std::string out = "Suffix. Valid Suffices: ";

	if (suffices.empty()) {
		out += "Any.";
	}
	else {
		for (const std::string& each : suffices) {
			out += each + RESULT_SEP;
		}
		out.erase(out.size() - RESULT_SEP.size());
		out += ".";
	}
	return out;
}

std::vector<std::string> CVFPCPlugin::SuffixSingle(const rapidjson::Value& restrictions) const
{
	std::vector<std::string> suffices{};

	if (!restrictions.IsArray() || restrictions.Empty()) {
		return suffices;
	}

	for (rapidjson::SizeType i = 0; i < restrictions.Size(); ++i) {
		const auto& restriction = restrictions[i];
		if (!restriction.IsObject()) {
			continue;
		}

		const auto* suffixArr = FindArrayMember(restriction, "suffix");
		if (!suffixArr || suffixArr->Empty()) {
			continue;
		}

		bool banned = false;
		if (const auto* bannedValue = FindMemberValue(restriction, "banned");
			bannedValue && bannedValue->IsBool()) {
			banned = bannedValue->GetBool();
		}

		for (rapidjson::SizeType j = 0; j < suffixArr->Size(); ++j) {
			const auto& suffix = (*suffixArr)[j];
			if (!suffix.IsString()) {
				continue;
			}

			std::string out;
			if (banned) {
				out += "Not ";
			}
			out += suffix.GetString();
			suffices.push_back(out);
		}
	}
	return suffices;
}

//Outputs valid cruise level direction (from Constraints array) as string
std::string CVFPCPlugin::DirectionOutput(const rapidjson::Value& constraints,
	std::vector<size_t> successes)const
{
	bool lvls[2]{ false, false };

	for (size_t each : successes) {
		if (each >= constraints.Size()) {
			continue;
		}

		const auto& constraint = constraints[static_cast<rapidjson::SizeType>(each)];
		const auto* dirVal = FindMemberValue(constraint, "dir");
		if (dirVal && dirVal->IsString()) {
			std::string val = dirVal->GetString();
			if (val == EVEN_DIRECTION) {
				lvls[0] = true;
			}
			else if (val == ODD_DIRECTION) {
				lvls[1] = true;
			}
		}
		else {
			lvls[0] = true;
			lvls[1] = true;
		}
	}

	std::string out = "Odd-Even Rule. Required: ";
	if (lvls[0] && lvls[1]) {
		out += "Any";
	}
	else if (lvls[0]) {
		out += "Even";
	}
	else if (lvls[1]) {
		out += "Odd";
	}
	else {
		out += "Any";
	}
	return out;
}

//Outputs valid cruise level blocks (from Constraints array) as string
std::string CVFPCPlugin::MinMaxOutput(const rapidjson::Value& constraints,
	std::vector<size_t> successes)const
{
	std::vector<std::vector<int>> raw_lvls{};

	for (size_t each : successes) {

		if (each >= constraints.Size()) {
			continue;
		}

		std::vector<int> lvls = { MININT, MAXINT };

		const auto& constraint = constraints[static_cast<rapidjson::SizeType>(each)];
		const auto* minVal = FindMemberValue(constraint, "min");
		if (minVal && minVal->IsInt()) {
			lvls[0] = minVal->GetInt();
		}

		const auto* maxVal = FindMemberValue(constraint, "max");
		if (maxVal && maxVal->IsInt()) {
			lvls[1] = maxVal->GetInt();
		}

		raw_lvls.push_back(lvls);
	}

	if (raw_lvls.empty()) {
		return "Min/Max Level" + string(NO_RESULTS) + ".";
	}

	size_t i = 0;
	while (i < raw_lvls.size() - 1) {
		bool changed = false;
		for (size_t j = 0; j < raw_lvls.size(); j++) {
			if (i == j) continue;
			//Item j is a subset of Item i
			if (raw_lvls[j][0] >= raw_lvls[i][0] && raw_lvls[j][1] <=
				raw_lvls[i][1]) {
				raw_lvls.erase(raw_lvls.begin() + j);
				changed = true;
				break;
			}
			//Item j extends higher than Item i
			else if (raw_lvls[j][0] >= raw_lvls[i][0]) {
				raw_lvls[i][1] = raw_lvls[j][1];
				raw_lvls.erase(raw_lvls.begin() + j);
				changed = true;
				break;
			}
			//Item j extends lower than Item i
			else if (raw_lvls[j][1] <= raw_lvls[i][1]) {
				raw_lvls[i][0] = raw_lvls[j][0];
				raw_lvls.erase(raw_lvls.begin() + j);
				changed = true;
				break;
			}
		}

		if (!changed) {
			i++;
		}
	}

	std::string out = "Min/Max Level: ";
	for (std::vector<int> each : raw_lvls) {
		if (each[0] == MININT && each[1] == MAXINT) {
			out += "Any Level, ";
		}
		else if (each[0] == MININT) {
			out += std::to_string(each[1]) + "-, ";
		}
		else if (each[1] == MAXINT) {
			out += std::to_string(each[0]) + "+, ";
		}
		else {
			out += std::to_string(each[0]) + "-" + std::to_string(each[1]);
			out += RESULT_SEP;
		}
	}

	out = out.substr(0, out.size() - 2) + ".";
	return out;
}

//Outputs more detail for a Min/Max failure to make it more user friendly.
std::string CVFPCPlugin::LevelFailureDetail_(
	const rapidjson::Value& constraints,
	const std::vector<size_t>& candidates,
	const FlightPlanRow& row) const
{
	if (candidates.empty()) {
		return "Filed FL" + std::to_string(row.rfl / 100) +
			" is outside the permitted level range.";
	}

	const auto& c = constraints[static_cast<rapidjson::SizeType>(candidates.front())];

	int min = 0;
	int max = 0;

	if (const auto* v = FindMemberValue(c, "min"); v && v->IsInt()) {
		min = v->GetInt();
	}

	if (const auto* v = FindMemberValue(c, "max"); v && v->IsInt()) {
		max = v->GetInt();
	}

	std::string reason;

	if (const auto* alerts = FindArrayMember(c, "alerts"); alerts && !alerts->Empty()) {
		for (const auto& alert : alerts->GetArray()) {
			if (!alert.IsObject()) {
				continue;
			}

			const auto* note = FindMemberValue(alert, "note");
			if (note && note->IsString() && std::strlen(note->GetString()) > 0) {
				reason = note->GetString();
			}

			const auto* srd = FindMemberValue(alert, "srd");
			if (srd && srd->IsInt()) {
				if (!reason.empty()) {
					reason += " ";
				}
				reason += "SRD Note " + std::to_string(srd->GetInt()) + ".";
			}

			break;
		}
	}

	std::string text = "Filed FL" + std::to_string(row.rfl / 100);

	if (max > 0 && (row.rfl / 100) > max) {
		text += " exceeds maximum permitted FL" + std::to_string(max);
	}
	else if (min > 0 && (row.rfl / 100) < min) {
		text += " is below minimum permitted FL" + std::to_string(min);
	}
	else {
		text += " is outside the permitted level range";
	}

	text += " for " + row.origin + "->" + row.destination;

	if (!reason.empty()) {
		text += " (" + reason + ")";
	}

	text += ".";

	return text;
}

//Outputs valid initial routes (from Constraints array) as string
std::string CVFPCPlugin::RouteOutput(const rapidjson::Value& constraints,
	const std::vector<std::string>&
	extracted_route,
	const std::string& dest,
	int rfl,
	bool req_lvl) const
{
	std::vector<size_t> pos;
	pos.reserve(constraints.Size());

	for (size_t i = 0; i < constraints.Size(); ++i) {
		pos.push_back(i);
	}

	bool found_level_match = false;

	auto apply_filter = [&](auto predicate) {
		std::vector<size_t> next;
		next.reserve(pos.size());

		for (size_t idx : pos) {
			if (predicate(constraints[idx])) {
				next.push_back(idx);
			}
		}

		if (!next.empty()) {
			pos = std::move(next);
			return true;
		}

		return false;
		};

	// 0. exact dest match, but only for records that don't depend on points / nopoints
	if (apply_filter([&](const rapidjson::Value& c) {
		bool res = false;
		if (const auto* dests = FindArrayMember(c, "dests")) {
			for (const auto& d : dests->GetArray()) {
				if (d.IsString() && dest == d.GetString()) {
					res = true;
					break;
				}
			}
		}

		if (const auto* nodests = FindArrayMember(c, "nodests")) {
			for (const auto& nd : nodests->GetArray()) {
				if (nd.IsString() && startsWith(nd.GetString(),
					dest.c_str())) {
					res = false;
					break;
				}
			}
		}

		if (FindArrayMember(c, "points") || FindArrayMember(c, "nopoints")) {
			res = false;
		}
		return res;
		}))
	{
		// same behaviour as your original code: skip to stage 4 next
	}
	else {
		apply_filter([&](const rapidjson::Value& c) {
			const auto* dests = FindArrayMember(c, "dests");
			if (!dests) return true;
			return ArrayContainsStringPrefixMatch(*dests, dest);
			});
		apply_filter([&](const rapidjson::Value& c) {
			const auto* nodests = FindArrayMember(c, "nodests");
			if (!nodests) return true;
			return !ArrayContainsStringPrefixMatch(*nodests, dest);
			});
		apply_filter([&](const rapidjson::Value& c) {
			const auto* points = FindArrayMember(c, "points");
			if (!points) return true;
			return RouteContainsAnyPoint_(*points, extracted_route);
			});
	}
	apply_filter([&](const rapidjson::Value& c) {
		const auto* nopoints = FindArrayMember(c, "nopoints");
		if (!nopoints) return true;
		return !RouteContainsAnyPoint_(*nopoints, extracted_route);
		});
	apply_filter([&](const rapidjson::Value& c) {
		bool res = true;
		const int fl = rfl / 100;
		int minlvl = 0;
		if (GetIntMember(c, "min", minlvl) && minlvl > fl) {
			res = false;
		}
		int maxlvl = 0;
		if (GetIntMember(c, "max", maxlvl) && maxlvl < fl) {
			res = false;
		}
		if (res) {
			found_level_match = true;
		}
		return res;
		});
	apply_filter([&](const rapidjson::Value& c) {
		const auto* alerts = FindArrayMember(c, "alerts");
		if (!alerts) return true;
		for (const auto& alert : alerts->GetArray()) {
			if (!alert.IsObject()) continue;
			auto it = alert.FindMember("ban");
			if (it != alert.MemberEnd() &&
				it->value.IsBool() &&
				it->value.GetBool()) {
				return false;
			}
		}
		return true;
		});
	std::vector<std::string> out;

	for (size_t idx : pos) {
		const rapidjson::Value& c = constraints[idx];

		std::string positem;
		std::string route_text = JoinStringArray(FindArrayMember(c, "route"), RESULT_SEP);

		if (!route_text.empty()) {
			positem += route_text;
		}

		std::string points_text = JoinStringArray(FindArrayMember(c, "points"), RESULT_SEP);
		if (!points_text.empty()) {
			if (!positem.empty()) {
				positem += " and ";
			}
			positem += "via " + points_text;
		}

		std::string negitem;
		std::string noroute_text = JoinStringArray(FindArrayMember(c, "noroute"), RESULT_SEP);
		std::string nopoints_text = JoinStringArray(FindArrayMember(c, "nopoints"), RESULT_SEP);

		if (!noroute_text.empty()) {
			negitem += noroute_text;
		}

		if (!nopoints_text.empty()) {
			if (!negitem.empty()) {
				negitem += " or ";
			}
			negitem += "via " + nopoints_text;
		}

		if (!negitem.empty()) {
			negitem = "not " + negitem;
		}

		int minlvl = MININT;
		int maxlvl = MAXINT;
		GetIntMember(c, "min", minlvl);
		GetIntMember(c, "max", maxlvl);
		std::string lvlitem;

		if (minlvl == MININT && maxlvl == MAXINT) {
			lvlitem = "Any Level";
		}
		else if (minlvl == MININT) {
			lvlitem = std::to_string(maxlvl) + "-";
		}
		else if (maxlvl == MAXINT) {
			lvlitem = std::to_string(minlvl) + "+";
		}
		else {
			lvlitem = std::to_string(minlvl) + "-" + std::to_string
			(maxlvl);
		}
		std::string item = positem;
		if (!negitem.empty()) {
			if (!item.empty()) item += " but ";
			item += negitem;
		}
		item += " (" + lvlitem + ")";
		out.push_back(std::move(item));
	}

	std::string outstring;
	if (pos.empty() || (req_lvl && !found_level_match)) {
		outstring = NO_RESULTS;
	}
	else {
		for (size_t i = 0; i < out.size(); ++i) {
			if (i > 0) outstring += " / ";
			outstring += out[i];
		}
	}
	return "Valid Initial Routes: " + outstring + ".";
}

//Outputs valid FIR exit points (from Constraints array) as string
std::string CVFPCPlugin::ExitPointOutput(const rapidjson::Value& airport,
	std::vector<std::string> points) const
{
	std::map<std::string, std::vector<std::string>> explicit_sids; // key = exit point
	std::vector<bool> implicit_sids;

	if (!airport.IsObject()) {
		return "Exit Point. Not Found.";
	}

	const auto* sids = FindArrayMember(airport, "sids");
	if (!sids || sids->Empty()) {
		return "Exit Point. Not Found.";
	}

	implicit_sids.assign(sids->Size(), false);
	for (rapidjson::SizeType i = 0; i < sids->Size(); ++i) {
		const auto& sid = (*sids)[i];
		if (!sid.IsObject()) {
			continue;
		}

		const auto* pointValue = FindMemberValue(sid, "point");
		if (!pointValue || !pointValue->IsString()) {
			continue;
		}

		const std::string sid_name = pointValue->GetString();
		const auto* conditions = FindArrayMember(sid, "constraints");
		if (!conditions) {
			continue;
		}

		for (rapidjson::SizeType j = 0; j < conditions->Size(); ++j) {
			const auto& condition = (*conditions)[j];
			if (!condition.IsObject()) {
				continue;
			}

			if (const auto* pointsArr = FindArrayMember(condition,
				"points")) {
				if (!pointsArr->Empty()) {
					for (const std::string& each : points) {
						if (arrayContainsString(*pointsArr, each)) {
							explicit_sids[each].push_back(sid_name);
						}
					}
					continue;
				}
			}

			if (const auto* noPointsArr = FindArrayMember(condition, "nopoints")) {
				if (!noPointsArr->Empty()) {
					implicit_sids[i] = true;
					for (const std::string& each : points) {
						if (arrayContainsString(*noPointsArr, each)) {
							implicit_sids[i] = false;
							break;
						}
					}
				}
			}
		}
	}

	std::vector<std::string> out;

	if (!explicit_sids.empty()) {
		for (auto& [exit_point, sid_list] : explicit_sids) {

			std::sort(sid_list.begin(), sid_list.end());
			sid_list.erase(std::unique(sid_list.begin(), sid_list.end()), sid_list.end());
			std::string single;

			for (const std::string& each : sid_list) {
				single += each.empty() ? "No SID" : each;
				single += RESULT_SEP;
			}

			if (!single.empty()) {
				single.erase(single.size() - RESULT_SEP.size());
			}
			else {
				single = "None";
			}
			out.push_back(exit_point + " is valid for: " + single);
		}
	}
	if (!std::all_of(implicit_sids.begin(), implicit_sids.end(), [](bool v) { return !v; })) {

		std::string single;

		for (rapidjson::SizeType i = 0; i < sids->Size(); ++i) {
			if (!implicit_sids[i]) {
				continue;
			}

			const auto& sid = (*sids)[i];
			const auto* pointValue = FindMemberValue(sid, "point");

			if (!pointValue || !pointValue->IsString()) {
				continue;
			}

			const std::string temp = pointValue->GetString();
			single += temp.empty() ? "No SID" : temp;
			single += RESULT_SEP;
		}

		if (!single.empty()) {
			single.erase(single.size() - RESULT_SEP.size());
		}
		else {
			single = "None";
		}

		std::string prefix;

		if (!out.empty()) {
			prefix = "Additionally, the following SIDs may perhaps be valid: ";
		}
		else {
			prefix = "The following SIDs may perhaps be valid: ";
		}

		out.push_back(prefix + single);
	}
	if (out.empty()) {
		return "Exit Point. Not Found.";
	}

	std::string outstring;
	for (const std::string& each : out) {
		outstring += each + ". ";
	}

	if (!outstring.empty()) {
		outstring.pop_back(); // trailing space
	}

	return "Exit Point. " + outstring;
}

//Outputs valid destinations (from Constraints array) as string
string CVFPCPlugin::DestinationOutput(const rapidjson::Value& airport,
	const string& dest) const
{
	std::vector<std::string> explicit_sids;
	std::vector<std::string> implicit_sids;

	if (!airport.IsObject()) {
		return "Destination. Not Found.";
	}

	const auto* sids = FindArrayMember(airport, "sids");
	if (!sids || sids->Empty()) {
		return "Destination. Not Found.";
	}

	for (rapidjson::SizeType i = 0; i < sids->Size(); ++i) {
		const auto& sid = (*sids)[i];
		if (!sid.IsObject()) {
			continue;
		}

		const auto* pointValue = FindMemberValue(sid, "point");
		if (!pointValue || !pointValue->IsString()) {
			continue;
		}

		const auto* conditions = FindArrayMember(sid, "constraints");
		if (!conditions || conditions->Empty()) {
			continue;
		}

		bool push_explicit = false;
		bool push_implicit = false;

		for (rapidjson::SizeType j = 0; j < conditions->Size(); ++j) {
			const auto& condition = (*conditions)[j];
			if (!condition.IsObject()) {
				continue;
			}
			if (const auto* dests = FindArrayMember(condition, "dests")) {
				if (!dests->Empty() && !destArrayContains(*dests, dest).empty()) {
					push_explicit = true;
				}
			}
			else if (const auto* noDests = FindArrayMember(condition, "nodests")) {
				if (!noDests->Empty() && destArrayContains(*noDests, dest).empty()) {
					push_implicit = true;
				}
			}
		}

		std::string sid_name = pointValue->GetString();
		if (sid_name.empty()) {
			sid_name = "No SID";
		}

		if (push_explicit) {
			explicit_sids.push_back(sid_name);
		}
		else if (push_implicit) {
			implicit_sids.push_back(sid_name);
		}
	}

	std::sort(explicit_sids.begin(), explicit_sids.end());

	explicit_sids.erase(std::unique(explicit_sids.begin(),
		explicit_sids.end()), explicit_sids.end());

	std::sort(implicit_sids.begin(), implicit_sids.end());

	implicit_sids.erase(std::unique(implicit_sids.begin(),
		implicit_sids.end()), implicit_sids.end());

	std::string out;

	if (!explicit_sids.empty()) {
		out += dest + " is valid for: ";
		for (const std::string& sid : explicit_sids) {
			out += sid;
			out += RESULT_SEP;
		}
		out.erase(out.size() - RESULT_SEP.size());
		out += ".";
	}
	if (!implicit_sids.empty()) {
		if (!explicit_sids.empty()) {
			out += " Additionally, " + dest + " may be valid for: ";
		}
		else {
			out += dest + " may be valid for: ";
		}
		for (const std::string& sid : implicit_sids) {
			out += sid;
			out += RESULT_SEP;
		}

		out.erase(out.size() - RESULT_SEP.size());
		out += ".";
	}

	if (out.empty()) {
		out = "No valid SIDs found for " + dest + ".";
	}

	return "Destination. " + out;
}

//==============================================================
// 11. UI / EuroScope interaction
//==============================================================
//Handles departure list menu and menu items
void CVFPCPlugin::OnFunctionCall(int FunctionId, const char*
	ItemString, POINT Pt, RECT Area)
{
	try {
		if (FunctionId == TAG_FUNC_CHECKFP_MENU) {
			OpenPopupList(Area, "Options", 1);
			AddPopupListElement("Show Checks (SUMMARY)", "", TAG_FUNC_CHECKFP_CHECK_SUMMARY);
			AddPopupListElement("Show Checks (FULL)", "", TAG_FUNC_CHECKFP_CHECK_FULL);
			AddPopupListElement("Toggle Override", "", TAG_FUNC_CHECKFP_OVERRIDE);
			AddPopupListElement("Toggle Checks", "", TAG_FUNC_CHECKFP_DISMISS);
		}
		else if (FunctionId == TAG_FUNC_CHECKFP_CHECK_SUMMARY) {
			checkFPDetail(ResultsView::Summary);
		}
		else if (FunctionId == TAG_FUNC_CHECKFP_CHECK_FULL) {
			checkFPDetail(ResultsView::Full);
		}
		else if (FunctionId == TAG_FUNC_CHECKFP_OVERRIDE) {
			EuroScopePlugIn::CFlightPlan flightPlan = FlightPlanSelectASEL();
			if (!flightPlan.IsValid()) {
				SendToConsole(vfpc::urgent, "No flight plan selected.");
				LOG_ERROR("No valid flight plan selected for toggling override");
				return;
			}
			const std::string callsign = flightPlan.GetCallsign();
			{
				std::lock_guard<std::mutex> g(controller_override_mtx_);
				if (controller_override_callsigns_.contains(callsign)) {
					controller_override_callsigns_.erase(callsign);
					LOG_INFO("Override disabled for flight plan: callsign='{}'",
						callsign);
				}
				else {
					controller_override_callsigns_.insert(callsign);
					LOG_INFO("Override enabled for flight plan: callsign='{}'",
						callsign);
				}
			}
		}
		else if (FunctionId == TAG_FUNC_CHECKFP_DISMISS) {
			EuroScopePlugIn::CFlightPlan flightPlan = FlightPlanSelectASEL();

			if (!flightPlan.IsValid()) {
				SendToConsole(vfpc::urgent, "No flight plan selected.");
				LOG_ERROR("No valid flight plan selected for toggling checks");
				return;
			}

			const std::string callsign = flightPlan.GetCallsign();

			{
				std::lock_guard<std::mutex> g(checks_disabled_mtx_);

				if (checks_disabled_callsigns_.contains(callsign)) {
					checks_disabled_callsigns_.erase(callsign);

					LOG_INFO("Validation checks enabled for flight plan: callsign='{}'",
						callsign);
				}
				else {
					checks_disabled_callsigns_.insert(callsign);

					LOG_INFO("Validation checks disabled for flight plan: callsign='{}'",
						callsign);
				}
			}
		}
	}
	catch (const std::exception& ex) {
		LOG_ERROR("Error: {}", ex.what());
	}
	catch (const std::string& ex) {
		LOG_ERROR("Error: {}", ex);
	}
	catch (...) {
		LOG_ERROR("An unexpected error occurred");
	}
}

bool CVFPCPlugin::ChecksEnabledForFlightPlan(
	const EuroScopePlugIn::CFlightPlan& flightPlan)
{
	const std::string callsign = flightPlan.GetCallsign();

	std::lock_guard<std::mutex> g(checks_disabled_mtx_);
	return !checks_disabled_callsigns_.contains(callsign);
}

// Gets flight plan, checks if (S/D)VFR, calls checking algorithms,
// and outputs pass/fail result to departure list item
void CVFPCPlugin::OnGetTagItem(EuroScopePlugIn::CFlightPlan flightPlan,
	EuroScopePlugIn::CRadarTarget RadarTarget,
	int ItemCode,
	int TagData,
	char sItemString[16],
	int* pColorCode,
	COLORREF* pRGB,
	double* pFontSize)
{
	// -----------------------------
	// Disconnect guard
	// -----------------------------
	if (GetConnectionType() == EuroScopePlugIn::CONNECTION_TYPE_NO) {
		// Optional: neutral display
		strcpy_s(sItemString, 16, "OFF");

		if (pRGB) *pRGB = plugin_config_.TAG_GREY;

		return;
	}

	try {
		if (ItemCode != TAG_ITEM_CHECKFP) {
			return;
		}

		strcpy_s(sItemString, 16, "PDG");

		if (pRGB) *pRGB = plugin_config_.TAG_YELLOW;

		const char* origin_cstr = flightPlan.GetFlightPlanData().GetOrigin();
		std::string origin = origin_cstr ? origin_cstr : "";
		origin = to_upper(origin);

		FlightPlanRow row = BuildFlightPlanRow_(flightPlan);

		ProcessTrackedFlightPlan_(row);

		auto snap = airport_data_.load(std::memory_order_acquire);
		const bool version_ok = validVersion.load(std::memory_order_acquire);

		if (!version_ok) {
			LOG_TRACE("OnGetTagItem: callsign='{}' returning ERR because plugin version is invalid",
				row.callsign);
			strcpy_s(sItemString, 16, "ERR");
			if (pRGB) *pRGB = plugin_config_.TAG_RED;
			return;
		}

		if (!ChecksEnabledForFlightPlan(flightPlan)) {
			strcpy_s(sItemString, 16, "OFF");
			if (pRGB) *pRGB = plugin_config_.TAG_GREY;
			return;
		}

		{
			std::lock_guard<std::mutex> g(controller_override_mtx_);
			if (controller_override_callsigns_.contains(row.callsign)) {
				strcpy_s(sItemString, 16, "OVR");
				if (pColorCode) {
					*pColorCode = EuroScopePlugIn::TAG_COLOR_RGB_DEFINED;
				}
				if (pRGB) {
					*pRGB = plugin_config_.TAG_CYAN;
				}
				return;
			}
		}

		std::string locked_airport;
		{
			std::lock_guard<std::mutex> g(active_mtx_);
			locked_airport = active_airport_;
		}

		if (!locked_airport.empty() && origin != locked_airport) {
			const std::string skip_key =
				row.callsign + "|" + origin + "|" + locked_airport;

			bool should_log = false;
			{
				std::lock_guard<std::mutex> g(skipped_airport_log_mtx_);
				should_log = skipped_airport_logged_.insert(skip_key).second;
			}

			if (should_log) {
				LOG_TRACE("Skipping validation for callsign '{}' origin '{}' because active airport is locked to '{}'",
					row.callsign, origin, locked_airport);
			}

			strcpy_s(sItemString, 16, "OFF");
			if (pRGB) *pRGB = plugin_config_.TAG_GREY;
			return;
		}

		ValidationResult result = MakePendingResult_(row.callsign, "PDG", "Validation pending.");
		{
			std::lock_guard<std::mutex> g(tracked_flightplans_mtx_);
			auto it = tracked_flightplans_.find(row.callsign);
			if (it != tracked_flightplans_.end()) {
				result = it->second.last_result;
			}
		}
		ApplyTagDisplayFromResult_(result, sItemString, pColorCode, pRGB);
	}
	catch (const std::exception& ex) {
		LOG_ERROR("OnGetTagItem exception : {}", ex.what());
		strcpy_s(sItemString, 16, "ERR");
		if (pRGB) *pRGB = plugin_config_.TAG_RED;
	}
	catch (const std::string& ex) {
		LOG_ERROR("OnGetTagItem string exception: {}", ex);
		strcpy_s(sItemString, 16, "ERR");
		if (pRGB) *pRGB = plugin_config_.TAG_RED;
	}
	catch (...) {
		LOG_ERROR("OnGetTagItem unknown exception");
		strcpy_s(sItemString, 16, "ERR");
		if (pRGB) *pRGB = plugin_config_.TAG_RED;
	}
}

void CVFPCPlugin::ShowConfigStatus_()
{
	SendToConsole(vfpc::urgent, "VFPC config status");

	std::string view_text = fmt::format("Console results view: {}",
		(console_results_view_ == ResultsView::Full) ? "FULL" : "SUMMARY");
	SendToConsole(view_text.c_str(), false);

	if (plugin_config_.config_warnings.empty()) {
		SendToConsole("Config warnings: none");
		return;
	}

	std::string warnings_text = fmt::format("Config warnings: {}",
		plugin_config_.config_warnings.size());
	SendToConsole(warnings_text.c_str(), false);

	for (const auto& warning : plugin_config_.config_warnings) {
		SendToConsole("- {}", warning);
	}
}

//Handles console commands
bool CVFPCPlugin::OnCompileCommand(const char* sCommandLine) {
	try {
		//Restart Automatic Data Loading
		if (startsWith((COMMAND_PREFIX + LOAD_COMMAND).c_str(), sCommandLine))
		{
			fileLoad = false;
			autoLoad = true;
			relCount = 0;

			{
				std::lock_guard<std::mutex> g(active_mtx_);
				airport_data_requested_ = false;
			}

			SendToConsole(vfpc::urgent, "Auto-Load Activated. Requesting airport data from API.");
			LOG_INFO("Auto-load activated; API reload requested.");

			getSids();
			return true;
		}
		//Disable API and load from Sid.json file
		else if (startsWith((COMMAND_PREFIX + FILE_COMMAND).c_str(), sCommandLine))
		{
			autoLoad = false;
			fileLoad = true;

			SendToConsole(vfpc::urgent, "Attempting to load from File: {}", DATA_FILE);
			LOG_INFO("Will now load from File: {}", DATA_FILE);

			getSids();
			return true;
		}
		// Set console output to full validation details
		else if (startsWith((COMMAND_PREFIX + "full").c_str(), sCommandLine)) {
			console_results_view_ = ResultsView::Full;
			SendToConsole("Console results view set to FULL.");
			LOG_INFO("Console results view set to FULL");
			return true;
		}
		// Set console output to summary validation details
		else if (startsWith((COMMAND_PREFIX + "summary").c_str(), sCommandLine)) {
			console_results_view_ = ResultsView::Summary;
			SendToConsole("Console results view set to SUMMARY.");
			LOG_INFO("Console results view set to SUMMARY");
			return true;
		}
		else if (startsWith((COMMAND_PREFIX + "config").c_str(), sCommandLine)) {
			ShowConfigStatus_();
			return true;
		}
		else if (startsWith((COMMAND_PREFIX + TRACE_COMMAND).c_str(), sCommandLine)) {
			auto lg = spdlog::default_logger();

			if (!lg) {
				SendToConsole(vfpc::urgent, "VFPC logger not initialised");
				return true;
			}
			lg->set_level(spdlog::level::trace);
			spdlog::set_level(spdlog::level::trace);

			SendToConsole("VFPC logging set to TRACE");
			LOG_INFO("Runtime log level changed to TRACE");
		}
		else if (startsWith((COMMAND_PREFIX + NODEBUG_COMMAND).c_str(), sCommandLine)) {
			auto lg = spdlog::default_logger();

			if (!lg) {
				SendToConsole(vfpc::urgent, "VFPC logger not initialised");
				return true;
			}
			lg->set_level(spdlog::level::info);
			spdlog::set_level(spdlog::level::info);   // also sets global level

			SendToConsole("VFPC logging set to NO DEBUG");
			LOG_INFO("Runtime log level changed to NO DEBUG");
			return true;
		}
	}
	catch (const std::exception& ex) {
		LOG_ERROR("Error: {}", ex.what());
	}
	catch (const std::string& ex) {
		LOG_ERROR("Error: {}", ex);
	}
	catch (...) {
		LOG_ERROR("Error", "An unexpected error occured");
	}
	return false;
}

void CVFPCPlugin::checkFPDetail(ResultsView view)
{
	try {
		if (!validVersion.load(std::memory_order_acquire)) {
			SendToConsole(vfpc::urgent, "VFPC is disabled.");
			return;
		}

		EuroScopePlugIn::CFlightPlan flightPlan = FlightPlanSelectASEL();
		if (!flightPlan.IsValid()) {
			SendToConsole(vfpc::urgent, "No flight plan selected.");
			return;
		}

		FlightPlanRow row = BuildFlightPlanRow_(flightPlan);
		ValidationResult result = MakePendingResult_(row.callsign, "PDG", "No cached validation result yet.");
		{
			std::lock_guard<std::mutex> g(tracked_flightplans_mtx_);
			auto it = tracked_flightplans_.find(row.callsign);
			if (it != tracked_flightplans_.end()) {
				result = it->second.last_result;
			}
		}
		OutputValidationResultToConsole_(result, view);
	}
	catch (const std::exception& ex) {
		LOG_ERROR("checkFPDetail error: {}", ex.what());
		SendToConsole(vfpc::urgent, "Error showing flight plan details.");
	}
	catch (...) {
		LOG_ERROR("checkFPDetail unknown error");
		SendToConsole(vfpc::urgent, "Unexpected error showing flight plan details.");
	}
}

//==============================================================
// 12. Main coordinator
//==============================================================
void CVFPCPlugin::OnTimer(int Counter)
{
	if (!is_initialised) return;

	// -----------------------------
	// Version gating (non-blocking)
	// -----------------------------
	if (!version_checked.load(std::memory_order_acquire)) {
		return;
	}

	if (!validVersion.load(std::memory_order_acquire)) {
		return;
	}

	try {
		const bool connected = (GetConnectionType() != EuroScopePlugIn::CONNECTION_TYPE_NO);
		// -----------------------------
		// Disconnect handling
		// -----------------------------
		if (!connected) {
			if (session_state_ != SessionState::Disconnected) {
				session_state_ = SessionState::Disconnected;
				LOG_INFO("User logged off from EuroScope...");
			}

			if (!disconnect_cleanup_done_) {
				disconnect_cleanup_done_ = true;
				if (airport_thread_.joinable()) {
					LOG_INFO("Stopping airport worker thread (disconnect)...");
					airport_thread_.request_stop();
				}

				airport_load_inflight_.store(false, std::memory_order_release);
				airport_reload_requested_.store(false, std::memory_order_release);
				pending_ready_.store(false, std::memory_order_release);
				airport_data_.store(nullptr, std::memory_order_release);

				ResetActiveAirportState();
				ResetTrackedFlightPlans_();

				last_own_callsign_.clear();
				last_own_rating_ = -1;
				last_own_identity_valid_ = false;
				active_airport_source_ = ActiveAirportSource::None;
			}
			last_update = Counter;
			return;
		}

		// -----------------------------
		// Connect transition
		// -----------------------------
		if (session_state_ == SessionState::Disconnected) {
			session_state_ = SessionState::Connected_NoCallsign;
			disconnect_cleanup_done_ = false;
			LOG_INFO("User connected to EuroScope...");
		}

		// -----------------------------
		// 1) Apply completed worker result
		// -----------------------------
		if (pending_ready_.load(std::memory_order_acquire)) {
			std::shared_ptr<AirportValidationData> snap;
			{
				std::lock_guard lk(pending_mtx_);
				if (pending_ready_.load(std::memory_order_acquire)) {
					snap = std::move(pending_snapshot_);
					pending_ready_.store(false, std::memory_order_release);
				}
			}
			if (snap) {
				airport_data_.store(std::const_pointer_cast<const AirportValidationData>(snap),
					std::memory_order_release);

				const auto* icao = FindMemberValue(snap->airport_json, "icao");

				const char* source_text = fileLoad ? "file" : "server";

				LOG_INFO("Airport snapshot published: airport='{}' source='{}'",
					(icao && icao->IsString()) ? icao->GetString() : "(unknown)",
					source_text);

				// Snapshot now exists, so a fresh request is no longer outstanding
				{
					std::lock_guard<std::mutex> g(active_mtx_);
					airport_data_requested_ = true;
				}

				airport_load_inflight_.store(false, std::memory_order_release);

				RevalidateTrackedFlightPlans_(
					fileLoad ? "file airport data loaded" : "server airport data loaded"
				);
			}
			else {
				LOG_WARN("Airport snapshot publish skipped: pending_ready_ was set but pending snapshot was null");
				airport_load_inflight_.store(false, std::memory_order_release);
			}
		}

		// -----------------------------
		// 2) Resolve active airport from own controller identity
		// -----------------------------
		{
			OwnIdentityInfo self{};
			const bool own_identity_changed = UpdateOwnIdentityIfChanged_(self);
			if (own_identity_changed) {
				ResolveActiveAirport_(self);
			}
		}

		// -----------------------------
		// 3) Request airport data once after airport is known
		// -----------------------------
		EnsureAirportDataRequested();

		// -----------------------------
		// 4) Cleanup stale flight plans (5-10secs)
		// -----------------------------
		PruneTrackedFlightPlans_();

		// -----------------------------
		// 5) Start worker if reload requested and not already inflight
		// -----------------------------
		if (airport_reload_requested_.exchange(false, std::memory_order_acq_rel)) {
			// Prevent overlap
			if (airport_load_inflight_.exchange(true, std::memory_order_acq_rel)) {
				LOG_WARN("Airport reload request ignored because a load is already in flight"); 
				last_update = Counter;
				return;
			}

			const SidSource source = (fileLoad ? SidSource::File : SidSource::DataServer);
			std::string airport;

			{
				std::lock_guard<std::mutex> g(active_mtx_);
				if (active_airport_locked_) {
					airport = active_airport_;
				}
			}

			// In both API mode and file mode, do not start worker until airport is known
			if (airport.empty()) {
				LOG_TRACE("Airport reload deferred: active airport not yet determined.");
				airport_load_inflight_.store(false, std::memory_order_release);
				last_update = Counter;
				return;
			}

			// Stop previous worker if needed
			if (airport_thread_.joinable()) {
				LOG_TRACE("Stopping previous airport worker thread...");
				airport_thread_.request_stop();
			}

			LOG_TRACE("Starting airport worker thread. source={}, airport = '{}'",
				(source == SidSource::File ? "file" : "server"), airport);

			airport_thread_ = std::jthread([this, source, airport](std::stop_token st)
				{
					try {
						auto snap = std::make_shared<AirportValidationData>();
						const bool ok = FetchSidsInto_((source == SidSource::File) ? SidSource::File : SidSource::DataServer,
							airport,
							st,
							snap->airport_json);

						if (!ok || st.stop_requested()) {
							airport_load_inflight_.store(false, std::memory_order_release);
							return;
						}

						LOG_TRACE("Airport worker: snapshot ready. cfgIsArray = {} cfgSize = {}",
							snap->airport_json.IsArray(),
							snap->airport_json.IsArray() ? snap->airport_json.Size() : 0);
						{
							std::lock_guard lk(pending_mtx_);
							pending_snapshot_ = std::move(snap);
							pending_ready_.store(true, std::memory_order_release);
						}
					}
					catch (const std::exception& ex) {
						LOG_ERROR("Airport worker exception: {}", ex.what());
						airport_load_inflight_.store(false, std::memory_order_release);
					}
					catch (...) {
						LOG_ERROR("Airport worker unknown exception.");
						airport_load_inflight_.store(false, std::memory_order_release);
					}
				});
		}

		last_update = Counter;
	}
	catch (const std::exception& ex) {
		LOG_ERROR("Exception in OnTimer: {}", ex.what());
	}
	catch (const std::string& ex) {
		LOG_ERROR("Exception in OnTimer: {}", ex);
	}
	catch (...) {
		LOG_ERROR("Unknown exception in OnTimer.");
	}
}