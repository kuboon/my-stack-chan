#include "ota_signature.h"

#include <cstring>

#include <mbedtls/md.h>
#include <mbedtls/pk.h>

namespace ota_signature {

bool hexToBytes(const char* hex, uint8_t* out, size_t capacity, size_t& length) {
    length = 0;
    if (!hex) return false;
    size_t n = std::strlen(hex);
    if (n == 0 || n % 2 != 0 || n / 2 > capacity) return false;
    auto nibble = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    for (size_t i = 0; i < n / 2; i++) {
        int hi = nibble(hex[2 * i]), lo = nibble(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = static_cast<uint8_t>(hi << 4 | lo);
    }
    length = n / 2;
    return true;
}

bool verify(const uint8_t* publicKeyDer, size_t keyLength, const uint8_t hash[32],
            const uint8_t* signature, size_t signatureLength) {
    if (!publicKeyDer || !keyLength || !signature || !signatureLength) return false;
    mbedtls_pk_context pk;
    mbedtls_pk_init(&pk);
    bool ok = mbedtls_pk_parse_public_key(&pk, publicKeyDer, keyLength) == 0 &&
              mbedtls_pk_verify(&pk, MBEDTLS_MD_SHA256, hash, 32, signature, signatureLength) == 0;
    mbedtls_pk_free(&pk);
    return ok;
}

}  // namespace ota_signature
