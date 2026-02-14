#pragma once

/// @file version_constraint.hpp
/// @brief Version constraint parsing and matching for plugin dependencies.
///
/// Supports version constraint operators: >=, >, <=, <, ==, ~= (compatible release).
/// Dependency strings combine a plugin name with optional constraints:
///   "PluginName"          — any version
///   "PluginName>=1.0.0"   — version 1.0.0 or newer
///   "PluginName~=1.5"     — compatible release (>=1.5.0, <2.0.0)
///
/// @see SDS-MOD-018 (Plugin Dependency Resolution)

#include "cgs/foundation/game_result.hpp"
#include "cgs/plugin/plugin_types.hpp"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace cgs::plugin {

/// Comparison operator for a version constraint.
enum class ConstraintOp : uint8_t {
    GreaterEqual,      ///< >=
    GreaterThan,       ///< >
    LessEqual,         ///< <=
    LessThan,          ///< <
    Equal,             ///< ==
    CompatibleRelease  ///< ~= (same major, minor >= specified)
};

/// A single version constraint: operator + version.
///
/// Example: `>=1.2.0` means "version must be at least 1.2.0".
struct VersionConstraint {
    ConstraintOp op = ConstraintOp::GreaterEqual;
    Version version;

    /// Check whether the given version satisfies this constraint.
    [[nodiscard]] bool IsSatisfiedBy(const Version& v) const noexcept;

    /// Parse a constraint string like ">=1.2.0", "<2.0.0", "~=1.5".
    ///
    /// @return Parsed constraint, or error if the format is invalid.
    [[nodiscard]] static cgs::foundation::GameResult<VersionConstraint> Parse(
        std::string_view spec);

    /// Format the constraint as a human-readable string.
    [[nodiscard]] std::string ToString() const;
};

/// A parsed dependency specification: plugin name + optional version constraints.
///
/// Parses dependency strings from PluginInfo::dependencies:
///   "NetworkPlugin"           → name="NetworkPlugin", no constraints
///   "NetworkPlugin>=1.0.0"    → name="NetworkPlugin", constraints=[>=1.0.0]
///   "CoreLib>=1.0,<2.0"       → name="CoreLib", constraints=[>=1.0.0, <2.0.0]
struct DependencySpec {
    std::string name;
    std::vector<VersionConstraint> constraints;

    /// Check whether all constraints are satisfied by the given version.
    [[nodiscard]] bool IsSatisfiedBy(const Version& v) const noexcept;

    /// Parse a dependency string.
    ///
    /// @return Parsed spec, or error if the format is invalid.
    [[nodiscard]] static cgs::foundation::GameResult<DependencySpec> Parse(std::string_view dep);

    /// Format all constraints as a human-readable string.
    [[nodiscard]] std::string ConstraintsToString() const;
};

/// Parse a version string "major.minor.patch" (minor and patch are optional).
///
/// Examples: "1" → {1,0,0}, "1.2" → {1,2,0}, "1.2.3" → {1,2,3}
[[nodiscard]] cgs::foundation::GameResult<Version> ParseVersion(std::string_view str);

}  // namespace cgs::plugin
