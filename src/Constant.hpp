#pragma once
#include "stdafx.h"

#define PLUGIN_WELCOME_MESSAGE	"Welcome to the (UK) VATSIM Flight Plan Checker"

using namespace std;

const int TAG_ITEM_CHECKFP = 1;

const int TAG_FUNC_CHECKFP_MENU = 100;
const int TAG_FUNC_CHECKFP_CHECK_FULL = 101;
const int TAG_FUNC_CHECKFP_CHECK_SUMMARY = 102;
const int TAG_FUNC_CHECKFP_OVERRIDE = 103;
const int TAG_FUNC_CHECKFP_DISMISS = 104;

const size_t API_REFRESH_TIME = 10;

const string EVEN_DIRECTION = "EVEN";
const string ODD_DIRECTION = "ODD";

const string PLUGIN_FILE = "VFPC.dll";
const string DATA_FILE = "Sid.json";
const string LOG_FILE = "VFPC.log";

const string COMMAND_PREFIX = ".vfpc ";
const string CONFIG_COMMAND = "config";
const string FILE_COMMAND = "file"; 
const string LOAD_COMMAND = "load";
const string NODEBUG_COMMAND = "nodebug"; 
const string TRACE_COMMAND = "trace";

const string DCT_ENTRY = "DCT";
const string SPDLVL_SEP = "/";
const string OUTDATED_SID = "#";

const string RESULT_SEP = ", ";
const string ROUTE_RESULT_SEP = " / ";
const string NO_RESULTS = "None";

const string WILDCARD = "*";

const int RVSM_UPPER = 41000;

inline static bool startsWith(const char *pre, const char *str)
{
	size_t lenpre = strlen(pre), lenstr = strlen(str);
	return lenstr < lenpre ? false : strncmp(pre, str, lenpre) == 0;
};