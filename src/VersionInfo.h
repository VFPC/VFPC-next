#ifndef VERSIONINFO_H
#define VERSIONINFO_H

//==============================================================
// Shared plugin identity/version information
// Used by both C++ source and Resource.rc
//==============================================================

//--------------------------------------------------------------
// VERSION - EDIT THESE VALUES FOR EACH RELEASE/BUILD
//--------------------------------------------------------------

#define VFPC_VERSION_MAJOR 4
#define VFPC_VERSION_MINOR 0
#define VFPC_VERSION_PATCH 0
#define VFPC_VERSION_BUILD 0

// Set to 1 for beta/RC builds, 0 for a normal release.
#define VFPC_IS_PRERELEASE 0

// Leave empty for a normal release.
// Examples: "beta1", "beta2", "rc1"
#define VFPC_VERSION_PRERELEASE ""

//--------------------------------------------------------------
// DO NOT NORMALLY EDIT BELOW THIS LINE
//--------------------------------------------------------------

// Numeric version required by Windows VERSIONINFO
// Example: 4,0,0,0
#define VFPC_VERSION_NUM \
    VFPC_VERSION_MAJOR, VFPC_VERSION_MINOR, \
    VFPC_VERSION_PATCH, VFPC_VERSION_BUILD

// String helpers
#define VFPC_STRINGIZE2(x) #x
#define VFPC_STRINGIZE(x) VFPC_STRINGIZE2(x)

// Base semantic version
// Example: "4.0.0"
#define VFPC_VERSION_STR \
    VFPC_STRINGIZE(VFPC_VERSION_MAJOR) "." \
    VFPC_STRINGIZE(VFPC_VERSION_MINOR) "." \
    VFPC_STRINGIZE(VFPC_VERSION_PATCH)

// Full numeric version
// Example: "4.0.0.0"
#define VFPC_VERSION_STR_FULL \
    VFPC_STRINGIZE(VFPC_VERSION_MAJOR) "." \
    VFPC_STRINGIZE(VFPC_VERSION_MINOR) "." \
    VFPC_STRINGIZE(VFPC_VERSION_PATCH) "." \
    VFPC_STRINGIZE(VFPC_VERSION_BUILD)

//--------------------------------------------------------------
// Release strings
//--------------------------------------------------------------

#if VFPC_IS_PRERELEASE

    // Example: v4.0.0-beta1 (build 3)
#define VFPC_DISPLAY_VERSION \
        "v" VFPC_VERSION_STR "-" VFPC_VERSION_PRERELEASE \
        " (build " VFPC_STRINGIZE(VFPC_VERSION_BUILD) ")"

    // Example: 4.0.0-beta1
#define VFPC_RC_VERSION_STR \
        VFPC_VERSION_STR "-" VFPC_VERSION_PRERELEASE

#else

    // Example: v4.0.0 (build 0)
#define VFPC_DISPLAY_VERSION \
        "v" VFPC_VERSION_STR \
        " (build " VFPC_STRINGIZE(VFPC_VERSION_BUILD) ")"

    // Example: 4.0.0
#define VFPC_RC_VERSION_STR VFPC_VERSION_STR

#endif

//--------------------------------------------------------------
// Build configuration
//--------------------------------------------------------------

#ifdef _DEBUG
#define VFPC_BUILD_CONFIGURATION "Debug"
#define VFPC_FILE_DESCRIPTION    "VFPC EuroScope Plugin (DEBUG BUILD)"
#else
#define VFPC_BUILD_CONFIGURATION "Release"
#define VFPC_FILE_DESCRIPTION    "VFPC EuroScope Plugin"
#endif

//--------------------------------------------------------------
// Shared product metadata
//--------------------------------------------------------------

#define VFPC_PLUGIN_NAME         "VFPC"
#define VFPC_PRODUCT_NAME        "VFPC"
#define VFPC_INTERNAL_NAME       "VFPC"
#define VFPC_ORIGINAL_FILENAME   "VFPC.dll"
#define VFPC_COMPANY_NAME        "Peter Richardson, Lenny Colton, Jan Fries, Hendrik Peter, Sven Czarnian"
#define VFPC_COPYRIGHT_TEXT      "GPL v3"

#endif // VERSIONINFO_H
