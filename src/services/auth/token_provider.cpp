/// @file token_provider.cpp
/// @brief TokenProvider implementation with HS256 and RS256 JWT signing.
///
/// Token format (RFC 7519):
///   base64url(header) . base64url(payload) . base64url(signature)
///
/// HS256 Header: {"alg":"HS256","typ":"JWT"}
/// RS256 Header: {"alg":"RS256","typ":"JWT"}
/// Payload:      {"sub":"...","usr":"...","roles":[...],"jti":"...","iat":N,"exp":N}
///
/// @see SRS-SVC-001.3
/// @see SRS-SVC-001.4
/// @see SRS-NFR-014

#include "cgs/service/token_provider.hpp"

#include "cgs/foundation/error_code.hpp"
#include "cgs/foundation/game_error.hpp"
#include "cgs/service/token_blacklist.hpp"

#include "crypto_utils.hpp"
#include "rsa_utils.hpp"

#include <charconv>
#include <sstream>
#include <string>

namespace cgs::service {

using cgs::foundation::ErrorCode;
using cgs::foundation::GameError;

// ---------------------------------------------------------------------------
// Minimal JSON helpers (no external dependency)
// ---------------------------------------------------------------------------
namespace {

/// Escape a string for JSON output.
std::string jsonEscape(std::string_view s) {
    std::string out;
    out.reserve(s.size() + 2);
    out.push_back('"');
    for (char c : s) {
        switch (c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out.push_back(c);
                break;
        }
    }
    out.push_back('"');
    return out;
}

/// Convert time_point to seconds since epoch.
int64_t toEpoch(std::chrono::system_clock::time_point tp) {
    return std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
}

/// Convert seconds-since-epoch to time_point.
std::chrono::system_clock::time_point fromEpoch(int64_t epoch) {
    return std::chrono::system_clock::time_point(std::chrono::seconds(epoch));
}

/// Split a string by delimiter, returning up to maxParts parts.
std::vector<std::string> split(std::string_view s, char delim, std::size_t maxParts = 0) {
    std::vector<std::string> parts;
    std::size_t start = 0;
    while (start < s.size()) {
        if (maxParts > 0 && parts.size() + 1 >= maxParts) {
            parts.emplace_back(s.substr(start));
            return parts;
        }
        auto pos = s.find(delim, start);
        if (pos == std::string_view::npos) {
            parts.emplace_back(s.substr(start));
            break;
        }
        parts.emplace_back(s.substr(start, pos - start));
        start = pos + 1;
    }
    return parts;
}

/// Extract a JSON string value by key (simple parser for flat objects).
std::string extractJsonString(std::string_view json, std::string_view key) {
    std::string needle = "\"" + std::string(key) + "\":\"";
    auto pos = json.find(needle);
    if (pos == std::string_view::npos) {
        return {};
    }
    pos += needle.size();
    auto end = json.find('"', pos);
    if (end == std::string_view::npos) {
        return {};
    }
    return std::string(json.substr(pos, end - pos));
}

/// Extract a JSON integer value by key.
int64_t extractJsonInt(std::string_view json, std::string_view key) {
    std::string needle = "\"" + std::string(key) + "\":";
    auto pos = json.find(needle);
    if (pos == std::string_view::npos) {
        return 0;
    }
    pos += needle.size();
    // Skip whitespace
    while (pos < json.size() && (json[pos] == ' ' || json[pos] == '\t')) {
        ++pos;
    }
    int64_t result = 0;
    auto [ptr, ec] = std::from_chars(json.data() + pos, json.data() + json.size(), result);
    if (ec != std::errc{}) {
        return 0;
    }
    return result;
}

/// Extract a JSON string array by key (simple parser).
std::vector<std::string> extractJsonStringArray(std::string_view json, std::string_view key) {
    std::vector<std::string> result;
    std::string needle = "\"" + std::string(key) + "\":[";
    auto pos = json.find(needle);
    if (pos == std::string_view::npos) {
        return result;
    }
    pos += needle.size();
    auto end = json.find(']', pos);
    if (end == std::string_view::npos) {
        return result;
    }
    auto arrayContent = json.substr(pos, end - pos);

    std::size_t cur = 0;
    while (cur < arrayContent.size()) {
        auto qStart = arrayContent.find('"', cur);
        if (qStart == std::string_view::npos) {
            break;
        }
        auto qEnd = arrayContent.find('"', qStart + 1);
        if (qEnd == std::string_view::npos) {
            break;
        }
        result.emplace_back(arrayContent.substr(qStart + 1, qEnd - qStart - 1));
        cur = qEnd + 1;
    }
    return result;
}

}  // anonymous namespace

// ---------------------------------------------------------------------------
// TokenProvider
// ---------------------------------------------------------------------------

TokenProvider::TokenProvider(const AuthConfig& config)
    : signingKey_(config.signingKey),
      rsaPrivateKeyPem_(config.rsaPrivateKeyPem),
      rsaPublicKeyPem_(config.rsaPublicKeyPem),
      algorithm_(config.jwtAlgorithm) {}

std::string TokenProvider::generateAccessToken(const TokenClaims& claims,
                                               std::chrono::seconds expiry) const {
    // Select header based on algorithm.
    const bool useRs256 = (algorithm_ == JwtAlgorithm::RS256);
    const std::string header =
        useRs256 ? R"({"alg":"RS256","typ":"JWT"})" : R"({"alg":"HS256","typ":"JWT"})";
    auto encodedHeader = detail::base64urlEncode(header);

    // Compute timestamps.
    auto now = std::chrono::system_clock::now();
    auto iat = toEpoch(claims.issuedAt.time_since_epoch().count() > 0 ? claims.issuedAt : now);
    auto exp = toEpoch(now + expiry);

    // Generate a unique JWT ID for blacklist referencing.
    auto jti = detail::secureRandomHex(16);  // 16 bytes -> 32 hex chars

    // Build payload JSON.
    std::ostringstream payload;
    payload << "{\"sub\":" << jsonEscape(claims.subject)
            << ",\"usr\":" << jsonEscape(claims.username) << ",\"roles\":[";
    for (std::size_t i = 0; i < claims.roles.size(); ++i) {
        if (i > 0) {
            payload << ",";
        }
        payload << jsonEscape(claims.roles[i]);
    }
    payload << "],\"jti\":" << jsonEscape(jti) << ",\"iat\":" << iat << ",\"exp\":" << exp << "}";

    auto encodedPayload = detail::base64urlEncode(payload.str());

    // Sign: algorithm-specific.
    std::string signingInput = encodedHeader + "." + encodedPayload;

    if (useRs256) {
        auto sig = detail::rsaSha256Sign(rsaPrivateKeyPem_, signingInput);
        auto encodedSignature = detail::base64urlEncode(sig.data(), sig.size());
        return signingInput + "." + encodedSignature;
    }

    // HS256 (default).
    auto mac = detail::hmacSha256(signingKey_, signingInput);
    auto encodedSignature = detail::base64urlEncode(mac.data(), mac.size());
    return signingInput + "." + encodedSignature;
}

cgs::foundation::GameResult<TokenClaims> TokenProvider::validateAccessToken(
    std::string_view token) const {
    // Split into 3 parts: header.payload.signature
    auto parts = split(token, '.', 3);
    if (parts.size() != 3) {
        return cgs::foundation::GameResult<TokenClaims>::err(
            GameError(ErrorCode::InvalidToken, "malformed JWT: expected 3 parts"));
    }

    // Decode header to determine algorithm.
    auto headerJson = detail::base64urlDecodeString(parts[0]);
    auto alg = extractJsonString(headerJson, "alg");

    // Verify signature based on claimed algorithm.
    std::string signingInput = parts[0] + "." + parts[1];

    if (alg == "RS256") {
        // RS256 verification requires public key.
        if (rsaPublicKeyPem_.empty()) {
            return cgs::foundation::GameResult<TokenClaims>::err(GameError(
                ErrorCode::InvalidToken, "RS256 token received but no public key configured"));
        }
        auto sigBytes = detail::base64urlDecode(parts[2]);
        if (!detail::rsaSha256Verify(rsaPublicKeyPem_, signingInput, sigBytes)) {
            return cgs::foundation::GameResult<TokenClaims>::err(
                GameError(ErrorCode::InvalidToken, "invalid RS256 signature"));
        }
    } else if (alg == "HS256") {
        // HS256 verification.
        auto expectedMac = detail::hmacSha256(signingKey_, signingInput);
        auto expectedSig = detail::base64urlEncode(expectedMac.data(), expectedMac.size());
        if (!detail::constantTimeEqual(expectedSig, parts[2])) {
            return cgs::foundation::GameResult<TokenClaims>::err(
                GameError(ErrorCode::InvalidToken, "invalid HS256 signature"));
        }
    } else {
        return cgs::foundation::GameResult<TokenClaims>::err(
            GameError(ErrorCode::InvalidToken, "unsupported JWT algorithm: " + alg));
    }

    // Decode payload.
    auto payloadJson = detail::base64urlDecodeString(parts[1]);
    if (payloadJson.empty()) {
        return cgs::foundation::GameResult<TokenClaims>::err(
            GameError(ErrorCode::InvalidToken, "failed to decode payload"));
    }

    // Parse claims.
    TokenClaims claims;
    claims.subject = extractJsonString(payloadJson, "sub");
    claims.username = extractJsonString(payloadJson, "usr");
    claims.roles = extractJsonStringArray(payloadJson, "roles");
    claims.jti = extractJsonString(payloadJson, "jti");
    claims.issuedAt = fromEpoch(extractJsonInt(payloadJson, "iat"));
    claims.expiresAt = fromEpoch(extractJsonInt(payloadJson, "exp"));

    // Check expiry.
    auto now = std::chrono::system_clock::now();
    if (now > claims.expiresAt) {
        return cgs::foundation::GameResult<TokenClaims>::err(
            GameError(ErrorCode::TokenExpired, "access token has expired"));
    }

    // Check blacklist.
    if (blacklist_ && !claims.jti.empty() && blacklist_->isRevoked(claims.jti)) {
        return cgs::foundation::GameResult<TokenClaims>::err(
            GameError(ErrorCode::TokenRevoked, "access token has been revoked"));
    }

    return cgs::foundation::GameResult<TokenClaims>::ok(std::move(claims));
}

void TokenProvider::setBlacklist(TokenBlacklist* blacklist) {
    blacklist_ = blacklist;
}

std::string TokenProvider::generateRefreshToken() {
    return detail::secureRandomHex(32);  // 32 bytes → 64 hex chars
}

}  // namespace cgs::service
