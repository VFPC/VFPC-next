#ifndef VERSIONINFO_H
#define VERSIONINFO_H

//==============================================================
// Shared plugin identity/version information
// Used by both C++ source and resource.rc
//==============================================================

// Numeric version for VERSIONINFO
#define VFPC_VERSION_MAJOR 3
#define VFPC_VERSION_MINOR 7
#define VFPC_VERSION_PATCH 2
#define VFPC_VERSION_BUILD 0

// Comma-separated numeric form required by FILEVERSION / PRODUCTVERSION
#define VFPC_VERSION_NUM  VFPC_VERSION_MAJOR, VFPC_VERSION_MINOR, VFPC_VERSION_PATCH, VFPC_VERSION_BUILD

// String helpers
#define VFPC_STRINGIZE2(x) #x
#define VFPC_STRINGIZE(x) VFPC_STRINGIZE2(x)

// "3.6.4.0"
#define VFPC_VERSION_STR_FULL \
    VFPC_STRINGIZE(VFPC_VERSION_MAJOR) "." \
    VFPC_STRINGIZE(VFPC_VERSION_MINOR) "." \
    VFPC_STRINGIZE(VFPC_VERSION_PATCH) "." \
    VFPC_STRINGIZE(VFPC_VERSION_BUILD)

// "3.6.4"
#define VFPC_VERSION_STR \
    VFPC_STRINGIZE(VFPC_VERSION_MAJOR) "." \
    VFPC_STRINGIZE(VFPC_VERSION_MINOR) "." \
    VFPC_STRINGIZE(VFPC_VERSION_PATCH)

// Shared product metadata
#define VFPC_PLUGIN_NAME         "VFPC"
#define VFPC_PRODUCT_NAME        "VFPC"
#define VFPC_FILE_DESCRIPTION    "VFPC EuroScope Plugin"
#define VFPC_INTERNAL_NAME       "VFPC"
#define VFPC_ORIGINAL_FILENAME   "VFPC.dll"
#define VFPC_COMPANY_NAME        "Peter Richardson"
#define VFPC_COPYRIGHT_TEXT      "Copyright (C) 2026 Peter Richardson"

#endif //VERSIONINFO_H
