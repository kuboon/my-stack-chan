// Pairing and authentication crypto for the direct (iPad) mode.
//
// Specified in web/server/docs/protocol.md. Built on mbedtls only, so that the same file
// compiles in the firmware (ESP-IDF's mbedtls 3.x) and in the host test (m5/test/host, the
// system's mbedtls 2.28 or 3.x) against m5/test/vectors/vectors.json.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace pairing {

constexpr size_t kKeySize = 32;
constexpr size_t kNonceSize = 16;

/// A random-bytes source with mbedtls's f_rng signature.
using Rng = int (*)(void* ctx, unsigned char* out, size_t len);

/// The X25519 public key of a 32-byte private key (clamped as RFC 7748 does).
bool publicKey(const uint8_t priv[kKeySize], uint8_t pub[kKeySize], Rng rng, void* rngCtx);

/// X25519(priv, peer). False for an invalid peer key or an all-zero result.
bool sharedSecret(const uint8_t priv[kKeySize], const uint8_t peer[kKeySize], uint8_t out[kKeySize],
                  Rng rng, void* rngCtx);

/// The 6-digit code both screens show: SHA-256("stackchan-pair-v1" || device || app), first 4
/// bytes big-endian, modulo 1,000,000.
uint32_t pairingCode(const uint8_t devicePub[kKeySize], const uint8_t appPub[kKeySize]);

/// The code as the screens show it, zero-padded to 6 digits.
std::string formatCode(uint32_t code);

/// HKDF-SHA256(shared, salt = app || device, info = "stackchan-v1-auth"), 32 bytes.
void authKey(const uint8_t shared[kKeySize], const uint8_t appPub[kKeySize],
             const uint8_t devicePub[kKeySize], uint8_t out[kKeySize]);

/// HMAC-SHA256(key, "stackchan-ws-v1" || nonce): what the app sends to authenticate a WebSocket.
void wsMac(const uint8_t key[kKeySize], const uint8_t nonce[kNonceSize], uint8_t out[kKeySize]);

/// Compares two buffers in time independent of where they differ.
bool equalConstantTime(const uint8_t* a, const uint8_t* b, size_t len);

std::string base64Encode(const uint8_t* data, size_t len);

/// Decodes standard Base64 into exactly `len` bytes. False if it does not decode to that length.
bool base64Decode(const std::string& text, uint8_t* out, size_t len);

}  // namespace pairing
