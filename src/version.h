#pragma once
//
// src/version.h
//
// Single authoritative version definition for the O2EM-NG application.
// Everything that needs the application version (About page, Settings,
// future updater code) must include this header instead of duplicating
// the string.
//
// When releasing a new version, update the three O2EM_VERSION_* number
// macros and O2EM_RELEASE_NAME below, and keep
// tools/release-manifest.json ("version" and "release") in sync.
//
// The macros are plain #defines so the resource compiler can also use
// them from O2EM-NG.rc for the VERSIONINFO block.
//

#define O2EM_VERSION_MAJOR 5
#define O2EM_VERSION_MINOR 0
#define O2EM_VERSION_PATCH 1
#define O2EM_VERSION_SUFFIX ""
#define O2EM_RELEASE_NAME "Beta 5"

#define O2EM_STRINGIFY_IMPL(x) #x
#define O2EM_STRINGIFY(x) O2EM_STRINGIFY_IMPL(x)

// Plain version string without the leading "v", e.g. "5.0.1".
// Matches the "version" field in tools/release-manifest.json.
#define O2EM_VERSION_STRING \
    O2EM_STRINGIFY(O2EM_VERSION_MAJOR) "." \
    O2EM_STRINGIFY(O2EM_VERSION_MINOR) "." \
    O2EM_STRINGIFY(O2EM_VERSION_PATCH) \
    O2EM_VERSION_SUFFIX

namespace O2emVersion
{
// Numeric components, usable for update comparison and VERSIONINFO.
inline constexpr int kAppVersionMajor = O2EM_VERSION_MAJOR;
inline constexpr int kAppVersionMinor = O2EM_VERSION_MINOR;
inline constexpr int kAppVersionPatch = O2EM_VERSION_PATCH;
inline constexpr const char* kAppVersionSuffix = O2EM_VERSION_SUFFIX;
inline constexpr const char* kReleaseName = O2EM_RELEASE_NAME;

// Full display string, e.g. "v5.0.1".
inline constexpr const char* kAppVersion =
    "v" O2EM_STRINGIFY(O2EM_VERSION_MAJOR) "."
    O2EM_STRINGIFY(O2EM_VERSION_MINOR) "."
    O2EM_STRINGIFY(O2EM_VERSION_PATCH)
    O2EM_VERSION_SUFFIX;
} // namespace O2emVersion
