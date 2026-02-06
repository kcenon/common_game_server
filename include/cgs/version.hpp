#pragma once

/// @file version.hpp
/// @brief Project version information and core namespace definition.

#define CGS_VERSION_MAJOR 0
#define CGS_VERSION_MINOR 1
#define CGS_VERSION_PATCH 0
#define CGS_VERSION_STRING "0.1.0"

namespace cgs {

/// Project version information at compile time.
struct Version {
    static constexpr int major = CGS_VERSION_MAJOR;
    static constexpr int minor = CGS_VERSION_MINOR;
    static constexpr int patch = CGS_VERSION_PATCH;
    static constexpr const char* string = CGS_VERSION_STRING;
};

} // namespace cgs
