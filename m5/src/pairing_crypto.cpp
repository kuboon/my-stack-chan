#include "pairing_crypto.h"

#include <algorithm>
#include <cstdio>
#include <cstring>

#include <mbedtls/base64.h>
#include <mbedtls/ecdh.h>
#include <mbedtls/ecp.h>
#include <mbedtls/gcm.h>
#include <mbedtls/md.h>

namespace pairing {
namespace {

const mbedtls_md_info_t* sha256() { return mbedtls_md_info_from_type(MBEDTLS_MD_SHA256); }

/// Loads a private key as a scalar, clamped (RFC 7748 section 5).
int loadPrivate(const uint8_t priv[kKeySize], mbedtls_mpi* d) {
    uint8_t k[kKeySize];
    std::memcpy(k, priv, kKeySize);
    k[0] &= 248;
    k[31] &= 127;
    k[31] |= 64;
    int ret = mbedtls_mpi_read_binary_le(d, k, kKeySize);
    std::memset(k, 0, sizeof k);
    return ret;
}

void hmac2(const uint8_t* key, size_t keyLen, const uint8_t* a, size_t aLen, const uint8_t* b,
           size_t bLen, uint8_t out[32]) {
    mbedtls_md_context_t ctx;
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, sha256(), 1);
    mbedtls_md_hmac_starts(&ctx, key, keyLen);
    mbedtls_md_hmac_update(&ctx, a, aLen);
    if (bLen) mbedtls_md_hmac_update(&ctx, b, bLen);
    mbedtls_md_hmac_finish(&ctx, out);
    mbedtls_md_free(&ctx);
}

}  // namespace

bool publicKey(const uint8_t priv[kKeySize], uint8_t pub[kKeySize], Rng rng, void* rngCtx) {
    mbedtls_ecp_group grp;
    mbedtls_mpi d;
    mbedtls_ecp_point q;
    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);
    mbedtls_ecp_point_init(&q);
    size_t olen = 0;
    bool ok = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_CURVE25519) == 0 &&
              loadPrivate(priv, &d) == 0 && mbedtls_ecp_mul(&grp, &q, &d, &grp.G, rng, rngCtx) == 0 &&
              mbedtls_ecp_point_write_binary(&grp, &q, MBEDTLS_ECP_PF_UNCOMPRESSED, &olen, pub,
                                             kKeySize) == 0 &&
              olen == kKeySize;
    mbedtls_ecp_point_free(&q);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_group_free(&grp);
    return ok;
}

bool sharedSecret(const uint8_t priv[kKeySize], const uint8_t peer[kKeySize], uint8_t out[kKeySize],
                  Rng rng, void* rngCtx) {
    mbedtls_ecp_group grp;
    mbedtls_mpi d, z;
    mbedtls_ecp_point q;
    mbedtls_ecp_group_init(&grp);
    mbedtls_mpi_init(&d);
    mbedtls_mpi_init(&z);
    mbedtls_ecp_point_init(&q);
    bool ok = mbedtls_ecp_group_load(&grp, MBEDTLS_ECP_DP_CURVE25519) == 0 &&
              loadPrivate(priv, &d) == 0 &&
              mbedtls_ecp_point_read_binary(&grp, &q, peer, kKeySize) == 0 &&
              mbedtls_ecdh_compute_shared(&grp, &z, &q, &d, rng, rngCtx) == 0 &&
              mbedtls_mpi_write_binary_le(&z, out, kKeySize) == 0;
    if (ok) {
        uint8_t acc = 0;
        for (size_t i = 0; i < kKeySize; i++) acc |= out[i];
        ok = acc != 0;
    }
    mbedtls_ecp_point_free(&q);
    mbedtls_mpi_free(&z);
    mbedtls_mpi_free(&d);
    mbedtls_ecp_group_free(&grp);
    return ok;
}

uint32_t pairingCode(const uint8_t devicePub[kKeySize], const uint8_t appPub[kKeySize]) {
    static const char kLabel[] = "stackchan-pair-v1";
    mbedtls_md_context_t ctx;
    uint8_t digest[32];
    mbedtls_md_init(&ctx);
    mbedtls_md_setup(&ctx, sha256(), 0);
    mbedtls_md_starts(&ctx);
    mbedtls_md_update(&ctx, reinterpret_cast<const uint8_t*>(kLabel), sizeof kLabel - 1);
    mbedtls_md_update(&ctx, devicePub, kKeySize);
    mbedtls_md_update(&ctx, appPub, kKeySize);
    mbedtls_md_finish(&ctx, digest);
    mbedtls_md_free(&ctx);
    uint32_t n = (uint32_t(digest[0]) << 24) | (uint32_t(digest[1]) << 16) |
                 (uint32_t(digest[2]) << 8) | uint32_t(digest[3]);
    return n % 1000000;
}

std::string formatCode(uint32_t code) {
    char buf[8];
    std::snprintf(buf, sizeof buf, "%06u", static_cast<unsigned>(code % 1000000));
    return buf;
}

void authKey(const uint8_t shared[kKeySize], const uint8_t appPub[kKeySize],
             const uint8_t devicePub[kKeySize], uint8_t out[kKeySize]) {
    static const char kInfo[] = "stackchan-v1-auth";
    uint8_t salt[2 * kKeySize];
    std::memcpy(salt, appPub, kKeySize);
    std::memcpy(salt + kKeySize, devicePub, kKeySize);
    uint8_t prk[32];
    hmac2(salt, sizeof salt, shared, kKeySize, nullptr, 0, prk);  // HKDF-Extract
    uint8_t info[sizeof kInfo];                                   // info || 0x01
    std::memcpy(info, kInfo, sizeof kInfo - 1);
    info[sizeof kInfo - 1] = 0x01;
    hmac2(prk, sizeof prk, info, sizeof info, nullptr, 0, out);  // HKDF-Expand, one block
    std::memset(prk, 0, sizeof prk);
}

void wsMac(const uint8_t key[kKeySize], const uint8_t nonce[kNonceSize], uint8_t out[kKeySize]) {
    static const char kLabel[] = "stackchan-ws-v1";
    hmac2(key, kKeySize, reinterpret_cast<const uint8_t*>(kLabel), sizeof kLabel - 1, nonce,
          kNonceSize, out);
}

void wifiKey(const uint8_t key[kKeySize], uint8_t out[kKeySize]) {
    static const char kInfo[] = "stackchan-v1-wifi";
    static const uint8_t kNoSalt[1] = {0};
    uint8_t prk[32];
    hmac2(kNoSalt, 0, key, kKeySize, nullptr, 0, prk);  // HKDF-Extract, the empty salt
    uint8_t info[sizeof kInfo];                          // info || 0x01
    std::memcpy(info, kInfo, sizeof kInfo - 1);
    info[sizeof kInfo - 1] = 0x01;
    hmac2(prk, sizeof prk, info, sizeof info, nullptr, 0, out);  // HKDF-Expand, one block
    std::memset(prk, 0, sizeof prk);
}

bool openWifiPassword(const uint8_t wifiKey[kKeySize], const std::string& ssid,
                      const uint8_t nonce[kWifiNonceSize], const uint8_t* sealed, size_t len,
                      std::string& password) {
    if (len < kWifiTagSize) return false;
    size_t textLen = len - kWifiTagSize;
    std::string text(textLen, '\0');
    mbedtls_gcm_context gcm;
    mbedtls_gcm_init(&gcm);
    bool ok = mbedtls_gcm_setkey(&gcm, MBEDTLS_CIPHER_ID_AES, wifiKey, kKeySize * 8) == 0 &&
              mbedtls_gcm_auth_decrypt(&gcm, textLen, nonce, kWifiNonceSize,
                                       reinterpret_cast<const unsigned char*>(ssid.data()), ssid.size(),
                                       sealed + textLen, kWifiTagSize, sealed,
                                       reinterpret_cast<unsigned char*>(&text[0])) == 0;
    mbedtls_gcm_free(&gcm);
    if (ok) password = text;
    std::fill(text.begin(), text.end(), '\0');
    return ok;
}

bool equalConstantTime(const uint8_t* a, const uint8_t* b, size_t len) {
    uint8_t acc = 0;
    for (size_t i = 0; i < len; i++) acc |= a[i] ^ b[i];
    return acc == 0;
}

std::string base64Encode(const uint8_t* data, size_t len) {
    std::string out(((len + 2) / 3) * 4 + 1, '\0');
    size_t olen = 0;
    mbedtls_base64_encode(reinterpret_cast<unsigned char*>(&out[0]), out.size(), &olen, data, len);
    out.resize(olen);
    return out;
}

bool base64Decode(const std::string& text, uint8_t* out, size_t len) {
    uint8_t buf[128];
    size_t olen = 0;
    if (mbedtls_base64_decode(buf, sizeof buf, &olen,
                              reinterpret_cast<const unsigned char*>(text.data()), text.size()) != 0 ||
        olen != len)
        return false;
    std::memcpy(out, buf, len);
    return true;
}

bool base64Decode(const std::string& text, uint8_t* out, size_t cap, size_t& len) {
    len = 0;
    return mbedtls_base64_decode(out, cap, &len, reinterpret_cast<const unsigned char*>(text.data()),
                                 text.size()) == 0;
}

}  // namespace pairing
