#ifndef VERSIONINFO_H
#define VERSIONINFO_H

//==============================================================
// Shared plugin identity/version information
// Used by both C++ source and resource.rc
//==============================================================

// Numeric version for VERSIONINFO
#define VFPC_VERSION_MAJOR 4
#define VFPC_VERSION_MINOR 0
#define VFPC_VERSION_PATCH 0
#define VFPC_VERSION_BUILD 3

// Optional prerelease label
#define VFPC_VERSION_PRERELEASE "beta2"

// Comma-separated numeric form required by FILEVERSION / PRODUCTVERSION
#define VFPC_VERSION_NUM  VFPC_VERSION_MAJOR, VFPC_VERSION_MINOR, VFPC_VERSION_PATCH, VFPC_VERSION_BUILD

// String helpers
#define VFPC_STRINGIZE2(x) #x
#define VFPC_STRINGIZE(x) VFPC_STRINGIZE2(x)

// "3.9.0.1"
#define VFPC_VERSION_STR_FULL \
    VFPC_STRINGIZE(VFPC_VERSION_MAJOR) "." \
    VFPC_STRINGIZE(VFPC_VERSION_MINOR) "." \
    VFPC_STRINGIZE(VFPC_VERSION_PATCH) "." \
    VFPC_STRINGIZE(VFPC_VERSION_BUILD)

// "3.9.0"
#define VFPC_VERSION_STR \
    VFPC_STRINGIZE(VFPC_VERSION_MAJOR) "." \
    VFPC_STRINGIZE(VFPC_VERSION_MINOR) "." \
    VFPC_STRINGIZE(VFPC_VERSION_PATCH)

// User-facing / release strings
#define VFPC_DISPLAY_VERSION \
    "v" VFPC_VERSION_STR "-" VFPC_VERSION_PRERELEASE \
    " (build " VFPC_STRINGIZE(VFPC_VERSION_BUILD) ")"
#define VFPC_RC_VERSION_STR  VFPC_VERSION_STR "-" VFPC_VERSION_PRERELEASE

// Shared product metadata
#define VFPC_PLUGIN_NAME         "VFPC"
#define VFPC_PRODUCT_NAME        "VFPC"
#define VFPC_FILE_DESCRIPTION    "VFPC EuroScope Plugin"
#define VFPC_INTERNAL_NAME       "VFPC"
#define VFPC_ORIGINAL_FILENAME   "VFPC.dll"
#define VFPC_COMPANY_NAME        "Peter Richardson, Lenny Colton, Jan Fries, Hendrik Peter, Sven Czarnian"
#define VFPC_COPYRIGHT_TEXT      "GPL v3"

#endif // VERSIONINFO_H
