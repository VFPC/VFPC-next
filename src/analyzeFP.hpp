//======================================================================
// analyzeFP.h
//
// Declares the main VFPC plugin class and the core data structures used
// for flight plan parsing, validation, result generation, airport data
// management, and EuroScope event handling.
//
// Design notes:
// - CVFPCPlugin is the central runtime class for the plugin.
// - Airport validation data is published as an immutable snapshot and
//   read concurrently by validation/UI code.
// - This header should contain declarations only; implementation-heavy
//   helpers belong in analyzeFP.cpp.
//======================================================================

//==============================================================
// 1. Includes / forward declarations
//==============================================================
#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>
#include <fmt/format.h>
#include <Windows.h>
#include <rapidjson/document.h>
#include "Constant.hpp"
#include "Utils.h"
#include "PluginConfig.h"
#include "Log.h"


//==============================================================
// 1. Validation data structures
//==============================================================

// Normalised view of the flight plan data needed by the validator.
// This decouples validation logic from direct EuroScope API access and
// makes validation decisions easier to reason about and cache.
struct FlightPlanRow
{
	std::string callsign;
	std::string eobt; // [day, hour, minute]
	std::string origin;
	std::string departure_rwy;
	std::string destination;
	std::string route;

	std::vector<std::string> route_points;

	std::string sid;
	std::string sid_suffix;
	std::string first_waypoint;

	std::string aircraft_type;
	std::string engine_type;

	// Future navigation capability support
	std::string remarks;   // Raw EuroScope Item 18 remarks
	std::string pbn;       // Extracted PBN/ value from remarks
	bool has_rnav1 = false;

	int rfl = 0;
	bool is_ifr = false;
};
//==============================================================
// 2. Helper tags / utility types
//==============================================================
namespace vfpc {
	struct UrgentTag {};
	inline constexpr UrgentTag urgent{};
}

namespace
{
	template <typename Out>
	void split(const std::string& s, char delim, Out result)
	{
		std::istringstream iss(s);
		std::string item;
		while (std::getline(iss, item, delim))
		{
			*result++ = item;
		}
	}
	std::vector<std::string> split(const std::string& s, char delim)
	{
		std::vector<std::string> elems;
		split(s, delim, std::back_inserter(elems));
		return elems;
	}
}

//==============================================================
// 3. Plugin Class declarations.
//==============================================================
class CVFPCPlugin : public EuroScopePlugIn::CPlugIn
{
public:

	//==============================================================
	// 4. Public data structures/result types.
	//==============================================================
	// --------------------
	// Snapshot type (immutable once published)
	// --------------------
	struct AirportValidationData {
		rapidjson::Document airport_json; // Single airport, built from API or file.
		AirportValidationData() { airport_json.SetObject(); }
	};

	enum ValidationField
	{
		VF_CALLSIGN = 0,
		VF_ORIGIN = 1,
		VF_SID = 2,
		VF_DESTINATION = 3,
		VF_EXIT_POINT = 4,
		VF_ROUTE = 5,
		VF_MIN_MAX = 6,
		VF_ODD_EVEN = 7,
		VF_SUFFIX = 8,
		VF_RESTRICTIONS = 9,
		VF_WARNINGS = 10,
		VF_BANS = 11,
		VF_SYNTAX = 12,
		VF_STATUS = 13,
		VF_FIELD_COUNT = 14
	};

	enum class ResultsView
	{
		Full,
		Summary
	};

	enum class ValidationStatus
	{
		Pending,
		Passed,
		Failed,
		NotChecked
	};
	struct ValidationResult
	{
		bool ready = false;
		bool passed = false;
		std::string itemString;

		struct Field
		{
			std::string full = "-";
			std::string summary = "-";
		};

		std::array<Field, VF_FIELD_COUNT> fields;
		ValidationStatus status = ValidationStatus::Pending;
	};

	//==============================================================
	// 5. Construction / destruction
	//==============================================================
	CVFPCPlugin();
	virtual ~CVFPCPlugin();

	//==============================================================
	// 6. API / data loading
	//==============================================================
	virtual bool webCall(
		std::string url,
		std::string& out,
		std::stop_token st);

	virtual bool APICall(
		const std::string& base_url, 
		const std::string& endpoint, 
		std::stop_token st, 
		rapidjson::Document& out);

	virtual bool fileCall(rapidjson::Document& out);

	// Triggers an async reload of airport/SID data (worker thread builds snapshot)
	virtual void getSids();

	//==============================================================
	// 7. Validation entry points
	//==============================================================
	virtual std::vector<bool> checkDestination(
		const rapidjson::Value& constraints, 
		const std::string& destination, 
		std::vector<bool>& in)const;

	virtual std::vector<bool> checkExitPoint(
		const rapidjson::Value& constraints, 
		const std::vector<std::string>& extracted_route, 
		std::vector<bool>& in)const;

	virtual std::vector<bool> checkRoute(
		const rapidjson::Value& constraints,
		const std::vector<std::string>& extracted_route,
		std::vector<bool>& in,
		std::vector<std::string>* route_advisories = nullptr) const;

	virtual std::vector<bool> checkMinMax(
		const rapidjson::Value& constraints, 
		int RFL, 
		std::vector<bool>& in)const;

	virtual std::vector<bool> checkDirection(
		const rapidjson::Value& constraints, 
		int RFL, std::vector<bool>& in)const;

	virtual std::vector<bool> checkAlerts(
		const rapidjson::Value& constraints,
		bool* warn, 
		std::vector<bool>& in, 
		int rfl)const;

	virtual void checkFPDetail(ResultsView view);
	
	//==============================================================
	// 8. Validation output formatting
	//==============================================================
	virtual std::string BansOutput(
		const rapidjson::Value& constraints, 
		std::vector<size_t> successes, 
		std::vector<std::string> extracted_route, 
		std::string dest,
		int rfl) const;

	virtual std::string WarningsOutput(
		const rapidjson::Value& constraints, 
		std::vector<size_t> successes, 
		std::vector<std::string> extracted_route, 
		std::string dest,
		int rfl)const;

	virtual std::string AlternativesOutput(
		const rapidjson::Value& sid_ele, 
		std::vector<size_t> successes = {})const;

	virtual std::vector<std::string> AlternativesSingle(
		const rapidjson::Value& sid_ele) const;

	virtual std::string RestrictionsOutput(
		const rapidjson::Value& sid_ele, 
		const FlightPlanRow& row, 
		bool check_type = true, 
		bool check_time = true, 
		bool check_ban = true, 
		std::vector<size_t> successes = {})const;

	virtual std::vector<std::vector<std::string>> RestrictionsSingle(
		const rapidjson::Value& restrictions, 
		const FlightPlanRow& row, 
		bool check_type = true, 
		bool check_time = true, 
		bool check_ban = true)const;

	virtual std::string SuffixOutput(
		const rapidjson::Value& sid_ele, 
		std::vector<size_t> successes = {})const;
	
	// Airway equivalence helpers
	static bool IsAirwayToken_(const std::string& token);
	static std::string StripUpperAirwayPrefix_(const std::string& token);
	static bool IsUpperLowerAirwayEquivalent_(const std::string& expected, const std::string& filed);
	
	static bool RouteTokenMatchesStrictOrAirwayEquivalent_(const std::string& expected, const std::string& filed, std::vector<std::string>* warnings);
	virtual std::vector<std::string> SuffixSingle(const rapidjson::Value& restrictions) const;
	virtual std::string DirectionOutput(const rapidjson::Value& constraints, std::vector<size_t> successes)const;
	virtual std::string MinMaxOutput(const rapidjson::Value& constraints, std::vector<size_t> successes)const;
	std::string LevelFailureDetail_(
		const rapidjson::Value& constraints,
		const std::vector<size_t>& candidates,
		const FlightPlanRow& row) const;
	virtual std::string RouteOutput(const rapidjson::Value& constraints, const std::vector<std::string>& extracted_route, const std::string& dest, int rfl, bool req_lvl)const;
	virtual std::string ExitPointOutput(const rapidjson::Value& airport, std::vector<std::string> extracted_route)const;
	virtual std::string DestinationOutput(const rapidjson::Value& airport, const std::string& dest)const;
	//==============================================================
	// 9. UI / console helpers
	//==============================================================
	virtual bool ChecksEnabledForFlightPlan(const EuroScopePlugIn::CFlightPlan& flightPlan);
	void SendToConsole(const char* msg, bool urgent = false);

	template <typename... Args>
	void SendToConsole(fmt::format_string<Args...> f, Args&&... args)
	{
		std::string s = fmt::format(f, std::forward<Args>(args)...);
		SendToConsole(s.c_str(), false);
	}

	template <typename... Args>
	void SendToConsole(vfpc::UrgentTag, fmt::format_string<Args...> f,
		Args&&... args)
	{
		std::string s = fmt::format(f, std::forward<Args>(args)...);
		SendToConsole(s.c_str(), true);
	}

	void ApplyTagDisplayFromResult_(const ValidationResult& result, char sItemString[16], int* pColorCode, COLORREF* pRGB) const;
	void OutputValidationResultToConsole_(const ValidationResult& result, ResultsView view);
	std::string CompactFieldName_(ValidationField field) const;
	bool FieldNeedsAttention_(const ValidationResult& result, ValidationField field) const;
	std::string TagMnemonicFromResult_(const ValidationResult& result) const;
	//==============================================================
	// 10. EuroScope event handlers
	//==============================================================
	virtual bool OnCompileCommand(const char* sCommandLine) override;
	virtual void OnFunctionCall(int FunctionId, const char* ItemString, POINT Pt, RECT Area);
	virtual void OnGetTagItem(EuroScopePlugIn::CFlightPlan FlightPlan,
		EuroScopePlugIn::CRadarTarget RadarTarget, int ItemCode,
		int TagData, char sItemString[16],
		int* pColorCode,
		COLORREF* pRGB,
		double* pFontSize);

	virtual void OnTimer(int Count) override;
	//==============================================================
	// 11. Snapshot state shared across threads
	//==============================================================
	std::atomic_bool version_checked{ false };
	std::atomic_bool validVersion{ false };
	std::atomic<std::shared_ptr<const AirportValidationData>> airport_data_{ nullptr };
	std::atomic_bool airport_reload_requested_{ false };
	std::atomic_bool airport_load_inflight_{ false };
	//==============================================================
	// 12. Public utility helpers.
	//==============================================================
	std::string destArrayContains(const rapidjson::Value& a, std::string s)const {
		for (rapidjson::SizeType i = 0; i < a.Size(); i++) {
			std::string test = a[i].GetString();
			rapidjson::SizeType x = static_cast<rapidjson::SizeType>(s.rfind(
				test, 0));
			if (s.rfind(a[i].GetString(), 0) != -1)
				return a[i].GetString();
		}
		return "";
	}

	bool arrayContainsEnding(const rapidjson::Value& a, std::string s)const {
		for (rapidjson::SizeType i = 0; i < a.Size(); i++) {
			std::string comp = a[i].GetString();
			int pos = s.size() - comp.size();
			if (pos < 0)
				continue;
			bool valid = true;
			for (rapidjson::SizeType i = 0; i < comp.size(); i++) {
				if (comp[i] != s[pos + i])
					valid = false;
			}
			if (valid)
				return true;
		}
		return false;
	}

	bool arrayContainsString(const rapidjson::Value& a, const std::string& s)const {
		for (rapidjson::SizeType i = 0; i < a.Size(); i++) {
			if (a[i].GetString() == s)
				return true;
		}
		return false;
	}

	std::string arrayToString(const rapidjson::Value& a, char delimiter) {
		std::string s;
		for (rapidjson::SizeType i = 0; i < a.Size(); i++) {
			s += a[i].GetString()[0];
			if (i != a.Size() - 1)
				s += delimiter;
		}
		return s;
	}

	bool routeContains(
		const std::vector<std::string>& filed_route,
		const rapidjson::Value& constraint_route_array,
		std::vector<std::string>* route_advisories = nullptr) const
	{
		bool found_advisory_match = false;
		std::vector<std::string> best_advisories;

		for (rapidjson::SizeType i = 0; i < constraint_route_array.Size(); ++i) {
			if (!constraint_route_array[i].IsString()) {
				continue;
			}

			std::string route = constraint_route_array[i].GetString();

			LOG_TRACE("Checking route constraint index [{}] {} against filed route {}",
				i, route, vectorToString(filed_route));

			if (route == WILDCARD) {
				return true;
			}

			std::vector<std::string> waypoints = split(route, ' ');
			for (std::string& wp : waypoints) {
				to_upper(wp);
			}

			if (waypoints.size() > filed_route.size()) {
				continue;
			}

			bool admissible = true;
			std::vector<std::string> candidate_advisories;

			for (size_t j = 0; j < waypoints.size(); ++j) {
				if (waypoints[j] == WILDCARD) {
					continue;
				}

				// Important: compare from the start of the filed route.
				// Do not use i + j here; i is the JSON route-array index.
				if (!CVFPCPlugin::RouteTokenMatchesStrictOrAirwayEquivalent_(
					waypoints[j],
					filed_route[j],
					&candidate_advisories))
				{
					LOG_TRACE("Route mismatch at token {}: expected='{}' actual='{}'",
						j, waypoints[j], filed_route[j]);

					admissible = false;
					break;
				}
			}

			if (!admissible) {
				continue;
			}

			// Best possible result: exact/clean match, no advisory needed.
			if (candidate_advisories.empty()) {
				return true;
			}

			// Keep advisory match only as fallback, in case no clean match exists.
			found_advisory_match = true;
			best_advisories.insert(
				best_advisories.end(),
				candidate_advisories.begin(),
				candidate_advisories.end());
		}

		if (found_advisory_match) {
			std::sort(best_advisories.begin(), best_advisories.end());
			best_advisories.erase(
				std::unique(best_advisories.begin(), best_advisories.end()),
				best_advisories.end());

			if (route_advisories) {
				for (const auto& warning : best_advisories) {
					LOG_TRACE("Route advisory: {}", warning);

					if (std::find(route_advisories->begin(),
						route_advisories->end(),
						warning) == route_advisories->end())
					{
						route_advisories->push_back(warning);
					}
				}
			}

			return true;
		}

		return false;
	}

	string dayIntToString(int day)const {
		switch (day) {
		case 0:
			return "Monday";
		case 1:
			return "Tuesday";
		case 2:
			return "Wednesday";
		case 3:
			return "Thursday";
		case 4:
			return "Friday";
		case 5:
			return "Saturday";
		case 6:
			return "Sunday";
		default:
			return "Out of Range";
		}
	}

	static ValidationResult initialiseResults(const string& cs) {
		ValidationResult result;
		for (auto& f : result.fields)
		{
			f.full = "-";
			f.summary = "-";
		}
		result.fields[VF_CALLSIGN].full = cs;
		result.fields[VF_CALLSIGN].summary = cs;
		result.fields[VF_STATUS].full = "Failed";
		result.fields[VF_STATUS].summary = "Failed";
		return result;
	}

	inline const char* const BoolToString(bool b) const
	{
		return b ? "true" : "false";
	}
	/***********************************************************
	* End of PUBLIC section. The rest of the class is PRIVATE
	* implementation details, and PROTECTED version state.
	***********************************************************/
private:
	//==========================================================
	// 13. Private data structures
	//==========================================================
	struct TrackedFlightPlan
	{
		std::string fingerprint;
		std::chrono::steady_clock::time_point last_seen;
		ValidationResult last_result;
		FlightPlanRow last_row;

		bool has_logged_result = false;
		std::string last_logged_result_fingerprint;
	};

	struct ParsedFlightPlanData
	{
		string eobt;
		string origin;
		string destination;
		int rfl = 0;
		string rawroute;
		vector<string> route;
		vector<string> points;
		string sid;
		string first_wp;
		string sid_suffix;
	};
	enum class SidSource { DataServer, File };
	enum class OwnIdentityKind
	{
		None,
		Controller,
		Observer,
		Unknown
	};
	enum class ActiveAirportSource
	{
		None,
		ControllerIdentity,
		Inferred
	};
	struct OwnControllerInfo
	{
		bool valid = false;
		int rating = -1;
		std::string callsign;
	};
	struct OwnIdentityInfo
	{
		bool valid = false;
		OwnIdentityKind kind = OwnIdentityKind::None;
		int rating = -1;
		std::string callsign;
		std::string airport_icao;
	};
	struct RestrictionFailureFlags
	{
		bool suffix = false;
		bool type = false;
		bool time = false;
		bool banned = false;
	};
	struct SidRestrictionResult
	{
		bool has_restrictions = false;
		bool any_applicable = false; // true if at least one SID-level restriction is in scope for this flight
		bool passed = true;          // true if at least one applicable SID-level restriction passes,
									 // or if no SID-level restrictions exist
		RestrictionFailureFlags fails;
	};
	struct ConstraintRestrictionResult
	{
		bool has_restrictions = false;
		bool passed = true; // default pass if no constraint-level restrictions exist 
		RestrictionFailureFlags fails;
	};

	//==========================================================
	// 14. Version / worker thread helpers
	//==========================================================
	void StartVersionCheckAsync();
	bool VersionCall_Worker(PluginConfig plugin_config, std::stop_token st);
	bool FetchSidsInto_(SidSource source, const std::string& activeAirportIcao, std::stop_token st, rapidjson::Document& out);
	//==========================================================
	// 15. Validation support helpers
	//==========================================================
	ValidationResult ValidateFlightPlan_(const FlightPlanRow& row) const;

	bool NormaliseAndValidateRouteSyntax_(std::vector<std::string>& route,
		const std::string& origin,
		const std::string& destination,
		const std::string& sid,
		const std::string& first_wp,
		std::string& outchk) const;

	bool TryFindSidIndex_(const rapidjson::Value& sids,
		const string& first_wp,
		size_t& pos)const;

	bool DoesSidOrRoutePointExist_(
		const rapidjson::Value& sids,
		const std::string& point) const;

	bool IsIfrFlightPlan_(const EuroScopePlugIn::CFlightPlan& flightPlan)const;

	ParsedFlightPlanData CParseFlightPlanData_(const EuroScopePlugIn::CFlightPlan& flightPlan) const;

	void SetResultField(std::vector<std::vector<std::string>>& out,
		ValidationField field,
		const std::string& normal,
		const std::string& debug);

	void SetResultBoth(std::vector<std::vector<std::string>>& out,
		ValidationField field,
		const std::string& text);
	std::vector<std::vector<std::string>> ReturnWithField(std::vector<std::vector<std::string>>& out,
		ValidationField field,
		const std::string& normal,
		const std::string& debug);
	std::vector<std::vector<std::string>> ReturnWithBoth(std::vector<std::vector<std::string>>& out,
		ValidationField field,
		const std::string& text);

	ValidationResult MakePendingResult_(const std::string& callsign,
		const std::string& normal = "Loading",
		const std::string& debug = "Airport data not loaded yet.")
		const;

	void ShowConfigStatus_();

	bool RouteContainsAnyPoint_(const rapidjson::Value& arr,
		const std::vector<std::string>& route) const;

	//==========================================================
	// 16. Constraint Restriction helpers
	//==========================================================

	bool SidLevelRestrictionRouteMatches_(
		const rapidjson::Value& restriction_route_array,
		const std::vector<std::string>& normalised_route,
		const std::string& first_waypoint) const;

	bool MatchingSidRestrictionHasTimeWindow_(
		const FlightPlanRow& row,
		const rapidjson::Value& restrictions) const;

	std::string FormatRestrictionTimeWindow_(
		const rapidjson::Value& restriction) const;

	std::string MatchingSidRestrictionTimeWindowText_(
		const FlightPlanRow& row,
		const rapidjson::Value& restrictions) const;

	std::string MatchingConstraintRestrictionTimeWindowText_(
		const FlightPlanRow& row,
		const rapidjson::Value& constraints,
		const std::vector<size_t>& successes,
		const std::vector<std::string>& normalised_route) const;

	bool RestrictionTypeSelectorMatches_(
		const rapidjson::Value& typesArr,
		const FlightPlanRow& row) const;

	bool RestrictionSelectorsMatch_(
		const FlightPlanRow& row,
		const rapidjson::Value& restriction) const;

	bool RestrictionObjectMatches_(
		const FlightPlanRow& row,
		const rapidjson::Value& restriction,
		RestrictionFailureFlags& fails) const;

	SidRestrictionResult EvaluateSidRestrictions_(
		const FlightPlanRow& row,
		const rapidjson::Value& sid_restrictions) const;

	bool SidLevelFailureCanBeOverriddenByConstraint_(
		const FlightPlanRow& row,
		const rapidjson::Value& sid_restrictions) const;

	ConstraintRestrictionResult EvaluateConstraintRestrictions_(
		const FlightPlanRow& row,
		const rapidjson::Value& restriction_array) const;

	std::vector<bool> CheckConstraintRestrictions_(
		const FlightPlanRow& row,
		const rapidjson::Value& conditions,
		RestrictionFailureFlags& aggregate_failures,
		std::vector<bool>& in) const;

	bool RouteWouldMatchConstraintRejectedByTime_(
		const FlightPlanRow& row,
		const rapidjson::Value& constraints,
		const std::vector<size_t>& candidates_before_restrictions,
		const std::vector<std::string>& normalised_route) const;

	bool ConstraintAllowsDestination_(const rapidjson::Value& cond,
		const std::string& destination) const;

	bool ConstraintAllowsLevel_(const rapidjson::Value& cond,
		int rfl) const;

	std::string BuildShortRouteExample_(const rapidjson::Value& cond) const;

	std::vector<std::string> CollectRouteExamples_(
		const rapidjson::Value& constraints,
		const std::string& destination,
		int rfl,
		const std::vector<std::string>& route_points) const;

	std::string RouteOutputShort_(const rapidjson::Value& constraints,
		const std::string& destination,
		int rfl,
		const std::vector<std::string>& route_points,
		std::size_t max_examples = 2) const;

	std::string RouteOutputFailureDetail_(const rapidjson::Value& constraints,
		const std::string& destination,
		int rfl,
		const std::vector<std::string>& route_points,
		std::size_t max_examples = 5) const;

	//==========================================================
	// 17. Active airport helpers
	//==========================================================
	void ResetActiveAirportState();

	void TryLockActiveAirportFromFlightPlan_(const FlightPlanRow& row);
	void EnsureAirportDataRequested();

	bool ExtractSingleAirportToObject_(const rapidjson::Document& in,
		const std::string& airport_icao,
		rapidjson::Document& out) const;

	OwnControllerInfo GetOwnControllerInfo_() const;

	OwnIdentityInfo GetOwnIdentityInfo_() const;

	bool TryExtractAirportFromControllerCallsign_(const std::string&
		callsign,
		std::string& out_icao) const;

	bool UpdateOwnIdentityIfChanged_(OwnIdentityInfo& out_info);

	void ResolveActiveAirport_(const OwnIdentityInfo& self);

	void SetActiveAirport_(const std::string& airport,
		ActiveAirportSource source);

	void ClearActiveAirport_();

	const char* OwnIdentityKindToString_(OwnIdentityKind kind) const;

	const char* ActiveAirportSourceToString_(ActiveAirportSource
		source) const;

	//==========================================================
	// 18. Flight plan tracking helpers
	//==========================================================
	std::string BuildFlightPlanFingerprint_(const FlightPlanRow& row) const;

	void ProcessTrackedFlightPlan_(const FlightPlanRow& row);

	void RevalidateTrackedFlightPlans_(const char* reason);

	void PruneTrackedFlightPlans_();

	void ResetTrackedFlightPlans_();

	bool CanValidateFlightPlans_() const;

	FlightPlanRow BuildFlightPlanRow_(const EuroScopePlugIn::CFlightPlan& flightPlan) const;

	//==========================================================
	// 19. Vaidation result helpers
	//==========================================================
	void SetPending_(ValidationResult& result, const std::string& text = "PDG") const;

	void SetPassed_(ValidationResult& result, const std::string& text = "OK!") const;

	void SetFailed_(ValidationResult& result, const std::string& text = "ERR") const;

	void SetNotChecked_(ValidationResult& result, const std::string& text = "VFR") const;

	void SetField_(ValidationResult& result,
		ValidationField field,
		const std::string& normal,
		const std::string& debug) const;

	void SetFieldIfUnset_(ValidationResult& result,
		ValidationField field,
		const std::string& normal,
		const std::string& debug) const;

	bool FieldIndicatesFailure_(const ValidationResult::Field& field)
		const;

	void FinalizeValidationResult_(ValidationResult& result) const;

	void SetBlockedFields_(ValidationResult& result,
		std::initializer_list<ValidationField> fields,
		const std::string& reason) const;

	std::string SummaryDetail_(const ValidationResult& 
		result, ValidationField field) const;

	bool IsUsefulSummaryText_(const std::string& text) const;

	//==========================================================
	// 20. Private state
	//==========================================================
	std::string base_url_;
	// version checking
	std::jthread version_thread_;

	// airport worker
	std::jthread airport_thread_;

	// pending snapshot produced by worker, applied on main thread
	std::mutex pending_mtx_;
	std::shared_ptr<AirportValidationData> pending_snapshot_;
	std::atomic_bool pending_ready_{ false };

	// active airports tracking (from tag items)
	mutable std::mutex active_mtx_;
	std::string active_airport_;
	bool active_airport_locked_ = false; // true, once airport has been determined.

	ActiveAirportSource active_airport_source_ = ActiveAirportSource::None;
	std::string last_own_callsign_;
	int last_own_rating_ = -1;
	bool last_own_identity_valid_ = false;
	bool airport_data_requested_ = false; // prevents repeated reload requests.
	std::unordered_map<std::string, int> origin_counts_;

	bool disconnect_cleanup_done_ = false;

	std::mutex tracked_flightplans_mtx_;
	std::unordered_map<std::string, TrackedFlightPlan>
		tracked_flightplans_;

	ResultsView console_results_view_ = ResultsView::Summary;

	// other existing state
	int last_update = -1;

//==========================================================
// 21. Protected version state.
//==========================================================
protected:

	int* thisVersion = nullptr;
	std::vector<int> curVersion;
	std::vector<int> minVersion;
};

	//==============================================================
	// 22. Free helper declarations
	//==============================================================
bool InitializeLogging(PluginConfig& pc);
