#pragma once

/// @file crypto_utils.hpp
/// @brief Internal cryptographic primitives: SHA-256, HMAC-SHA256, Base64URL.
///
/// Portable C++ implementation with no external dependencies.
/// Used internally by PasswordHasher and TokenProvider.

#include <array>
#include <cstdint>
#include <cstring>
#include <random>
#include <string>
#include <string_view>
#include <vector>

namespace cgs::service::detail {

// =============================================================================
// SHA-256 (FIPS 180-4)
// =============================================================================

/// Compute SHA-256 digest of the input data.
/// Returns 32-byte raw digest.
[[nodiscard]] inline std::array<uint8_t, 32> sha256(const uint8_t* data, std::size_t length) {
    // Initial hash values (first 32 bits of fractional parts of square roots
    // of the first 8 primes).
    uint32_t h0 = 0x6a09e667, h1 = 0xbb67ae85, h2 = 0x3c6ef372, h3 = 0xa54ff53a, h4 = 0x510e527f,
             h5 = 0x9b05688c, h6 = 0x1f83d9ab, h7 = 0x5be0cd19;

    // Round constants (first 32 bits of fractional parts of cube roots
    // of the first 64 primes).
    static constexpr uint32_t k[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4,
        0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe,
        0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
        0x4a7484aa, 0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
        0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85, 0x2e1b2138, 0x4d2c6dfc,
        0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b,
        0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116,
        0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7,
        0xc67178f2};

    auto rotr = [](uint32_t x, unsigned n) -> uint32_t { return (x >> n) | (x << (32 - n)); };

    // Pre-processing: pad the message.
    const uint64_t bitLen = static_cast<uint64_t>(length) * 8;
    std::vector<uint8_t> msg(data, data + length);
    msg.push_back(0x80);
    while ((msg.size() % 64) != 56) {
        msg.push_back(0x00);
    }
    for (int i = 7; i >= 0; --i) {
        msg.push_back(static_cast<uint8_t>((bitLen >> (i * 8)) & 0xFF));
    }

    // Process each 512-bit (64-byte) block.
    for (std::size_t offset = 0; offset < msg.size(); offset += 64) {
        uint32_t w[64]{};
        for (int i = 0; i < 16; ++i) {
            auto base = offset + static_cast<std::size_t>(i) * 4;
            w[i] = (static_cast<uint32_t>(msg[base]) << 24) |
                   (static_cast<uint32_t>(msg[base + 1]) << 16) |
                   (static_cast<uint32_t>(msg[base + 2]) << 8) |
                   static_cast<uint32_t>(msg[base + 3]);
        }
        for (int i = 16; i < 64; ++i) {
            uint32_t s0 = rotr(w[i - 15], 7) ^ rotr(w[i - 15], 18) ^ (w[i - 15] >> 3);
            uint32_t s1 = rotr(w[i - 2], 17) ^ rotr(w[i - 2], 19) ^ (w[i - 2] >> 10);
            w[i] = w[i - 16] + s0 + w[i - 7] + s1;
        }

        uint32_t a = h0, b = h1, c = h2, d = h3;
        uint32_t e = h4, f = h5, g = h6, hh = h7;

        for (int i = 0; i < 64; ++i) {
            uint32_t S1 = rotr(e, 6) ^ rotr(e, 11) ^ rotr(e, 25);
            uint32_t ch = (e & f) ^ (~e & g);
            uint32_t temp1 = hh + S1 + ch + k[i] + w[i];
            uint32_t S0 = rotr(a, 2) ^ rotr(a, 13) ^ rotr(a, 22);
            uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
            uint32_t temp2 = S0 + maj;

            hh = g;
            g = f;
            f = e;
            e = d + temp1;
            d = c;
            c = b;
            b = a;
            a = temp1 + temp2;
        }

        h0 += a;
        h1 += b;
        h2 += c;
        h3 += d;
        h4 += e;
        h5 += f;
        h6 += g;
        h7 += hh;
    }

    std::array<uint8_t, 32> digest{};
    auto store = [&](int idx, uint32_t val) {
        digest[static_cast<std::size_t>(idx * 4)] = static_cast<uint8_t>((val >> 24) & 0xFF);
        digest[static_cast<std::size_t>(idx * 4 + 1)] = static_cast<uint8_t>((val >> 16) & 0xFF);
        digest[static_cast<std::size_t>(idx * 4 + 2)] = static_cast<uint8_t>((val >> 8) & 0xFF);
        digest[static_cast<std::size_t>(idx * 4 + 3)] = static_cast<uint8_t>(val & 0xFF);
    };
    store(0, h0);
    store(1, h1);
    store(2, h2);
    store(3, h3);
    store(4, h4);
    store(5, h5);
    store(6, h6);
    store(7, h7);
    return digest;
}

/// SHA-256 overload for string_view input.
[[nodiscard]] inline std::array<uint8_t, 32> sha256(std::string_view input) {
    return sha256(reinterpret_cast<const uint8_t*>(input.data()), input.size());
}

// =============================================================================
// HMAC-SHA256 (RFC 2104)
// =============================================================================

/// Compute HMAC-SHA256(key, message).
/// Returns 32-byte raw MAC.
[[nodiscard]] inline std::array<uint8_t, 32> hmacSha256(std::string_view key,
                                                        std::string_view message) {
    constexpr std::size_t blockSize = 64;

    // If key is longer than block size, hash it first.
    std::array<uint8_t, 32> keyHash{};
    const uint8_t* keyPtr = nullptr;
    std::size_t keyLen = 0;

    if (key.size() > blockSize) {
        keyHash = sha256(key);
        keyPtr = keyHash.data();
        keyLen = keyHash.size();
    } else {
        keyPtr = reinterpret_cast<const uint8_t*>(key.data());
        keyLen = key.size();
    }

    // Zero-pad the key to block size.
    std::array<uint8_t, blockSize> paddedKey{};
    std::memcpy(paddedKey.data(), keyPtr, keyLen);

    // Inner and outer padded keys.
    std::array<uint8_t, blockSize> ipad{};
    std::array<uint8_t, blockSize> opad{};
    for (std::size_t i = 0; i < blockSize; ++i) {
        ipad[i] = paddedKey[i] ^ 0x36;
        opad[i] = paddedKey[i] ^ 0x5C;
    }

    // Inner hash: SHA-256(ipad || message)
    std::vector<uint8_t> innerMsg(ipad.begin(), ipad.end());
    innerMsg.insert(innerMsg.end(),
                    reinterpret_cast<const uint8_t*>(message.data()),
                    reinterpret_cast<const uint8_t*>(message.data()) + message.size());
    auto innerHash = sha256(innerMsg.data(), innerMsg.size());

    // Outer hash: SHA-256(opad || innerHash)
    std::vector<uint8_t> outerMsg(opad.begin(), opad.end());
    outerMsg.insert(outerMsg.end(), innerHash.begin(), innerHash.end());
    return sha256(outerMsg.data(), outerMsg.size());
}

// =============================================================================
// Base64URL encoding/decoding (RFC 4648 §5)
// =============================================================================

/// Encode bytes to base64url (no padding).
[[nodiscard]] inline std::string base64urlEncode(const uint8_t* data, std::size_t length) {
    static constexpr char table[] =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";

    std::string result;
    result.reserve((length * 4 + 2) / 3);

    for (std::size_t i = 0; i < length; i += 3) {
        uint32_t n = static_cast<uint32_t>(data[i]) << 16;
        if (i + 1 < length) {
            n |= static_cast<uint32_t>(data[i + 1]) << 8;
        }
        if (i + 2 < length) {
            n |= static_cast<uint32_t>(data[i + 2]);
        }

        result.push_back(table[(n >> 18) & 0x3F]);
        result.push_back(table[(n >> 12) & 0x3F]);
        if (i + 1 < length) {
            result.push_back(table[(n >> 6) & 0x3F]);
        }
        if (i + 2 < length) {
            result.push_back(table[n & 0x3F]);
        }
    }
    return result;
}

/// Encode a string to base64url.
[[nodiscard]] inline std::string base64urlEncode(std::string_view input) {
    return base64urlEncode(reinterpret_cast<const uint8_t*>(input.data()), input.size());
}

/// Decode base64url to bytes. Returns empty vector on invalid input.
[[nodiscard]] inline std::vector<uint8_t> base64urlDecode(std::string_view input) {
    auto decodeChar = [](char c) -> int {
        if (c >= 'A' && c <= 'Z') {
            return c - 'A';
        }
        if (c >= 'a' && c <= 'z') {
            return c - 'a' + 26;
        }
        if (c >= '0' && c <= '9') {
            return c - '0' + 52;
        }
        if (c == '-') {
            return 62;
        }
        if (c == '_') {
            return 63;
        }
        return -1;
    };

    std::vector<uint8_t> result;
    result.reserve((input.size() * 3) / 4);

    uint32_t buf = 0;
    int bits = 0;
    for (char c : input) {
        if (c == '=') {
            break;
        }
        int val = decodeChar(c);
        if (val < 0) {
            return {};
        }
        buf = (buf << 6) | static_cast<uint32_t>(val);
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            result.push_back(static_cast<uint8_t>((buf >> bits) & 0xFF));
        }
    }
    return result;
}

/// Decode base64url to string. Returns empty string on invalid input.
[[nodiscard]] inline std::string base64urlDecodeString(std::string_view input) {
    auto bytes = base64urlDecode(input);
    return {bytes.begin(), bytes.end()};
}

// =============================================================================
// Hex encoding
// =============================================================================

/// Encode bytes to lowercase hex string.
[[nodiscard]] inline std::string toHex(const uint8_t* data, std::size_t length) {
    static constexpr char hexChars[] = "0123456789abcdef";
    std::string result;
    result.reserve(length * 2);
    for (std::size_t i = 0; i < length; ++i) {
        result.push_back(hexChars[(data[i] >> 4) & 0x0F]);
        result.push_back(hexChars[data[i] & 0x0F]);
    }
    return result;
}

/// Encode a 32-byte array to hex string.
[[nodiscard]] inline std::string toHex(const std::array<uint8_t, 32>& data) {
    return toHex(data.data(), data.size());
}

// =============================================================================
// Secure random generation
// =============================================================================

/// Generate N bytes of cryptographic-quality random data (hex-encoded).
[[nodiscard]] inline std::string secureRandomHex(std::size_t numBytes) {
    std::random_device rd;
    std::vector<uint8_t> buf(numBytes);
    for (auto& b : buf) {
        b = static_cast<uint8_t>(rd());
    }
    return toHex(buf.data(), buf.size());
}

// =============================================================================
// Constant-time comparison
// =============================================================================

/// Compare two strings in constant time to prevent timing attacks.
[[nodiscard]] inline bool constantTimeEqual(std::string_view a, std::string_view b) {
    if (a.size() != b.size()) {
        return false;
    }
    volatile uint8_t diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        diff =
            static_cast<uint8_t>(diff | (static_cast<uint8_t>(a[i]) ^ static_cast<uint8_t>(b[i])));
    }
    return diff == 0;
}

}  // namespace cgs::service::detail
