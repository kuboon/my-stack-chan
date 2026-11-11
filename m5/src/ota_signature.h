// The signature of a firmware image (web/server/docs/protocol.md, "署名つきの更新"): ECDSA
// (P-256) over the image's SHA-256, in DER, as `openssl dgst -sha256 -sign key.pem` writes it.
// Apart from ota.cpp so that the host can test it (test/host/test_ota_signature.cpp).
#pragma once

#include <cstddef>
#include <cstdint>

namespace ota_signature {

/// `hex` (an even number of hex digits, at most 2 * capacity) as bytes. False when it is not.
bool hexToBytes(const char* hex, uint8_t* out, size_t capacity, size_t& length);

/// Whether `signature` is `publicKeyDer`'s (a DER SubjectPublicKeyInfo) signature of `hash`.
bool verify(const uint8_t* publicKeyDer, size_t keyLength, const uint8_t hash[32],
            const uint8_t* signature, size_t signatureLength);

}  // namespace ota_signature
