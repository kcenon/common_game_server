/// @file password_hasher.cpp
/// @brief PasswordHasher implementation using SHA-256 + random salt.
///
/// Production deployments should replace this with bcrypt (cost >= 12)
/// or argon2id via a foundation CryptoAdapter.
///
/// @see SRS-SVC-001.6

#include "cgs/service/password_hasher.hpp"

#include "crypto_utils.hpp"

namespace cgs::service {

HashedPassword PasswordHasher::hash(std::string_view password) const {
    auto salt = generateSalt();
    auto digest = hashWithSalt(password, salt);
    return {std::move(digest), std::move(salt)};
}

std::string PasswordHasher::hashWithSalt(std::string_view password, std::string_view salt) const {
    // SHA-256(salt + password)
    std::string combined;
    combined.reserve(salt.size() + password.size());
    combined.append(salt);
    combined.append(password);
    auto digest = detail::sha256(combined);
    return detail::toHex(digest);
}

bool PasswordHasher::verify(std::string_view password,
                            std::string_view storedHash,
                            std::string_view salt) const {
    auto computed = hashWithSalt(password, salt);
    return detail::constantTimeEqual(computed, storedHash);
}

std::string PasswordHasher::generateSalt() {
    return detail::secureRandomHex(16);  // 16 bytes → 32 hex chars
}

}  // namespace cgs::service
