#pragma once

/// @file rsa_utils.hpp
/// @brief RSA-SHA256 signing and verification using OpenSSL 3.x EVP API.
///
/// Internal header for the auth service. Uses BIO_new_mem_buf for PEM key
/// loading from strings (no file I/O) to support testability.
///
/// @see SRS-NFR-014

#include <cstdint>
#include <openssl/bio.h>
#include <openssl/err.h>
#include <openssl/evp.h>
#include <openssl/pem.h>
#include <string>
#include <string_view>
#include <vector>

namespace cgs::service::detail {

/// Sign a message with RSA-SHA256 using a PEM-encoded private key.
///
/// @param privateKeyPem PEM-encoded RSA private key string.
/// @param message       The data to sign.
/// @return Raw signature bytes, or empty vector on failure.
[[nodiscard]] inline std::vector<uint8_t> rsaSha256Sign(std::string_view privateKeyPem,
                                                        std::string_view message) {
    // Load private key from PEM string via BIO.
    auto* bio = BIO_new_mem_buf(privateKeyPem.data(), static_cast<int>(privateKeyPem.size()));
    if (!bio) {
        return {};
    }

    auto* pkey = PEM_read_bio_PrivateKey(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!pkey) {
        return {};
    }

    // Create signing context.
    auto* mdCtx = EVP_MD_CTX_new();
    if (!mdCtx) {
        EVP_PKEY_free(pkey);
        return {};
    }

    std::vector<uint8_t> signature;

    if (EVP_DigestSignInit(mdCtx, nullptr, EVP_sha256(), nullptr, pkey) != 1) {
        EVP_MD_CTX_free(mdCtx);
        EVP_PKEY_free(pkey);
        return {};
    }

    if (EVP_DigestSignUpdate(
            mdCtx, reinterpret_cast<const unsigned char*>(message.data()), message.size()) != 1) {
        EVP_MD_CTX_free(mdCtx);
        EVP_PKEY_free(pkey);
        return {};
    }

    // Determine signature length.
    std::size_t sigLen = 0;
    if (EVP_DigestSignFinal(mdCtx, nullptr, &sigLen) != 1) {
        EVP_MD_CTX_free(mdCtx);
        EVP_PKEY_free(pkey);
        return {};
    }

    signature.resize(sigLen);
    if (EVP_DigestSignFinal(mdCtx, signature.data(), &sigLen) != 1) {
        EVP_MD_CTX_free(mdCtx);
        EVP_PKEY_free(pkey);
        return {};
    }

    signature.resize(sigLen);
    EVP_MD_CTX_free(mdCtx);
    EVP_PKEY_free(pkey);
    return signature;
}

/// Verify an RSA-SHA256 signature using a PEM-encoded public key.
///
/// @param publicKeyPem PEM-encoded RSA public key string.
/// @param message      The original signed data.
/// @param signature    The signature bytes to verify.
/// @return True if the signature is valid.
[[nodiscard]] inline bool rsaSha256Verify(std::string_view publicKeyPem,
                                          std::string_view message,
                                          const std::vector<uint8_t>& signature) {
    // Load public key from PEM string via BIO.
    auto* bio = BIO_new_mem_buf(publicKeyPem.data(), static_cast<int>(publicKeyPem.size()));
    if (!bio) {
        return false;
    }

    auto* pkey = PEM_read_bio_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!pkey) {
        return false;
    }

    // Create verification context.
    auto* mdCtx = EVP_MD_CTX_new();
    if (!mdCtx) {
        EVP_PKEY_free(pkey);
        return false;
    }

    if (EVP_DigestVerifyInit(mdCtx, nullptr, EVP_sha256(), nullptr, pkey) != 1) {
        EVP_MD_CTX_free(mdCtx);
        EVP_PKEY_free(pkey);
        return false;
    }

    if (EVP_DigestVerifyUpdate(
            mdCtx, reinterpret_cast<const unsigned char*>(message.data()), message.size()) != 1) {
        EVP_MD_CTX_free(mdCtx);
        EVP_PKEY_free(pkey);
        return false;
    }

    int result = EVP_DigestVerifyFinal(mdCtx, signature.data(), signature.size());

    EVP_MD_CTX_free(mdCtx);
    EVP_PKEY_free(pkey);
    return result == 1;
}

}  // namespace cgs::service::detail
