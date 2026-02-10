/// @file version_constraint.cpp
/// @brief Version constraint parsing and matching implementation.
///
/// @see SDS-MOD-018

#include "cgs/plugin/version_constraint.hpp"

#include <charconv>

#include "cgs/foundation/error_code.hpp"

using cgs::foundation::ErrorCode;
using cgs::foundation::GameError;
using cgs::foundation::GameResult;

namespace cgs::plugin {

// ── Version parsing ─────────────────────────────────────────────────────

GameResult<Version> ParseVersion(std::string_view str) {
    Version ver{};

    // Trim whitespace.
    while (!str.empty() && str.front() == ' ') {
        str.remove_prefix(1);
    }
    while (!str.empty() && str.back() == ' ') {
        str.remove_suffix(1);
    }

    if (str.empty()) {
        return GameResult<Version>::err(
            GameError(ErrorCode::InvalidArgument, "Empty version string"));
    }

    // Parse major.
    auto dotPos = str.find('.');
    auto majorStr = str.substr(0, dotPos);
    auto [pMaj, ecMaj] =
        std::from_chars(majorStr.data(), majorStr.data() + majorStr.size(), ver.major);
    if (ecMaj != std::errc()) {
        return GameResult<Version>::err(
            GameError(ErrorCode::InvalidArgument,
                      "Invalid major version: " + std::string(str)));
    }

    if (dotPos == std::string_view::npos) {
        return GameResult<Version>::ok(ver);
    }

    // Parse minor.
    str.remove_prefix(dotPos + 1);
    dotPos = str.find('.');
    auto minorStr = str.substr(0, dotPos);
    auto [pMin, ecMin] =
        std::from_chars(minorStr.data(), minorStr.data() + minorStr.size(), ver.minor);
    if (ecMin != std::errc()) {
        return GameResult<Version>::err(
            GameError(ErrorCode::InvalidArgument,
                      "Invalid minor version: " + std::string(str)));
    }

    if (dotPos == std::string_view::npos) {
        return GameResult<Version>::ok(ver);
    }

    // Parse patch.
    str.remove_prefix(dotPos + 1);
    auto [pPat, ecPat] =
        std::from_chars(str.data(), str.data() + str.size(), ver.patch);
    if (ecPat != std::errc()) {
        return GameResult<Version>::err(
            GameError(ErrorCode::InvalidArgument,
                      "Invalid patch version: " + std::string(str)));
    }

    return GameResult<Version>::ok(ver);
}

// ── VersionConstraint ───────────────────────────────────────────────────

bool VersionConstraint::IsSatisfiedBy(const Version& v) const noexcept {
    switch (op) {
        case ConstraintOp::GreaterEqual:
            return v >= version;
        case ConstraintOp::GreaterThan:
            return v > version;
        case ConstraintOp::LessEqual:
            return v <= version;
        case ConstraintOp::LessThan:
            return v < version;
        case ConstraintOp::Equal:
            return v == version;
        case ConstraintOp::CompatibleRelease:
            // ~=1.5.0 means >=1.5.0 and <2.0.0 (same major).
            return v >= version && v.major == version.major;
    }
    return false;
}

GameResult<VersionConstraint> VersionConstraint::Parse(std::string_view spec) {
    // Trim whitespace.
    while (!spec.empty() && spec.front() == ' ') {
        spec.remove_prefix(1);
    }

    if (spec.empty()) {
        return GameResult<VersionConstraint>::err(
            GameError(ErrorCode::InvalidArgument, "Empty constraint string"));
    }

    VersionConstraint constraint;

    // Detect operator.
    if (spec.starts_with("~=")) {
        constraint.op = ConstraintOp::CompatibleRelease;
        spec.remove_prefix(2);
    } else if (spec.starts_with(">=")) {
        constraint.op = ConstraintOp::GreaterEqual;
        spec.remove_prefix(2);
    } else if (spec.starts_with(">")) {
        constraint.op = ConstraintOp::GreaterThan;
        spec.remove_prefix(1);
    } else if (spec.starts_with("<=")) {
        constraint.op = ConstraintOp::LessEqual;
        spec.remove_prefix(2);
    } else if (spec.starts_with("<")) {
        constraint.op = ConstraintOp::LessThan;
        spec.remove_prefix(1);
    } else if (spec.starts_with("==")) {
        constraint.op = ConstraintOp::Equal;
        spec.remove_prefix(2);
    } else {
        // Default: treat bare version as ==.
        constraint.op = ConstraintOp::Equal;
    }

    auto verResult = ParseVersion(spec);
    if (verResult.hasError()) {
        return GameResult<VersionConstraint>::err(verResult.error());
    }
    constraint.version = verResult.value();

    return GameResult<VersionConstraint>::ok(constraint);
}

std::string VersionConstraint::ToString() const {
    std::string result;
    switch (op) {
        case ConstraintOp::GreaterEqual:      result = ">="; break;
        case ConstraintOp::GreaterThan:       result = ">";  break;
        case ConstraintOp::LessEqual:         result = "<="; break;
        case ConstraintOp::LessThan:          result = "<";  break;
        case ConstraintOp::Equal:             result = "=="; break;
        case ConstraintOp::CompatibleRelease: result = "~="; break;
    }
    result += std::to_string(version.major) + "." +
              std::to_string(version.minor) + "." +
              std::to_string(version.patch);
    return result;
}

// ── DependencySpec ──────────────────────────────────────────────────────

bool DependencySpec::IsSatisfiedBy(const Version& v) const noexcept {
    for (const auto& c : constraints) {
        if (!c.IsSatisfiedBy(v)) {
            return false;
        }
    }
    return true;
}

GameResult<DependencySpec> DependencySpec::Parse(std::string_view dep) {
    // Trim whitespace.
    while (!dep.empty() && dep.front() == ' ') {
        dep.remove_prefix(1);
    }
    while (!dep.empty() && dep.back() == ' ') {
        dep.remove_suffix(1);
    }

    if (dep.empty()) {
        return GameResult<DependencySpec>::err(
            GameError(ErrorCode::InvalidArgument, "Empty dependency string"));
    }

    DependencySpec spec;

    // Find the start of version constraints (first operator character).
    auto constraintStart = dep.find_first_of("><=~");

    if (constraintStart == std::string_view::npos) {
        // No version constraint — just a plugin name.
        spec.name = std::string(dep);
        return GameResult<DependencySpec>::ok(std::move(spec));
    }

    spec.name = std::string(dep.substr(0, constraintStart));

    // Trim trailing whitespace from name.
    while (!spec.name.empty() && spec.name.back() == ' ') {
        spec.name.pop_back();
    }

    if (spec.name.empty()) {
        return GameResult<DependencySpec>::err(
            GameError(ErrorCode::InvalidArgument,
                      "Missing plugin name in dependency: " + std::string(dep)));
    }

    // Parse comma-separated constraints.
    auto remaining = dep.substr(constraintStart);
    while (!remaining.empty()) {
        // Find next comma.
        auto commaPos = remaining.find(',');
        auto part = remaining.substr(0, commaPos);

        auto constraintResult = VersionConstraint::Parse(part);
        if (constraintResult.hasError()) {
            return GameResult<DependencySpec>::err(constraintResult.error());
        }
        spec.constraints.push_back(constraintResult.value());

        if (commaPos == std::string_view::npos) {
            break;
        }
        remaining.remove_prefix(commaPos + 1);
    }

    return GameResult<DependencySpec>::ok(std::move(spec));
}

std::string DependencySpec::ConstraintsToString() const {
    if (constraints.empty()) {
        return "(any version)";
    }
    std::string result;
    for (std::size_t i = 0; i < constraints.size(); ++i) {
        if (i > 0) {
            result += ", ";
        }
        result += constraints[i].ToString();
    }
    return result;
}

} // namespace cgs::plugin
