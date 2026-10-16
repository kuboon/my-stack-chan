// Host test of src/pairing_crypto.cpp against test/vectors/vectors.json.
// Run: m5/test/host/run.sh
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

#include "pairing_crypto.h"

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

/// The string value of a top-level key in the flat vectors file.
std::string field(const std::string& json, const std::string& key) {
    size_t at = json.find("\"" + key + "\"");
    if (at == std::string::npos) return "";
    size_t start = json.find('"', json.find(':', at) + 1) + 1;
    return json.substr(start, json.find('"', start) - start);
}

int rng(void*, unsigned char* out, size_t len) {
    for (size_t i = 0; i < len; i++) out[i] = static_cast<unsigned char>(std::rand());
    return 0;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "usage: %s vectors.json\n", argv[0]);
        return 2;
    }
    std::ifstream f(argv[1]);
    std::stringstream ss;
    ss << f.rdbuf();
    const std::string v = ss.str();

    using namespace pairing;
    uint8_t appPriv[32], appPub[32], devPriv[32], devPub[32], shared[32], key[32], nonce[16], mac[32];
    check(base64Decode(field(v, "app_private"), appPriv, 32), "decode app_private");
    check(base64Decode(field(v, "app_public"), appPub, 32), "decode app_public");
    check(base64Decode(field(v, "device_private"), devPriv, 32), "decode device_private");
    check(base64Decode(field(v, "device_public"), devPub, 32), "decode device_public");
    check(base64Decode(field(v, "auth_key"), key, 32), "decode auth_key");
    check(base64Decode(field(v, "ws_nonce"), nonce, 16), "decode ws_nonce");
    check(base64Decode(field(v, "ws_mac"), mac, 32), "decode ws_mac");
    check(base64Decode(field(v, "shared_secret"), shared, 32), "decode shared_secret");

    uint8_t out[32];
    check(publicKey(devPriv, out, rng, nullptr) && std::memcmp(out, devPub, 32) == 0, "device public key");
    check(publicKey(appPriv, out, rng, nullptr) && std::memcmp(out, appPub, 32) == 0, "app public key");
    check(sharedSecret(devPriv, appPub, out, rng, nullptr) && std::memcmp(out, shared, 32) == 0,
          "shared secret (device side)");
    check(sharedSecret(appPriv, devPub, out, rng, nullptr) && std::memcmp(out, shared, 32) == 0,
          "shared secret (app side)");
    check(formatCode(pairingCode(devPub, appPub)) == field(v, "pairing_code"), "pairing code");
    authKey(shared, appPub, devPub, out);
    check(std::memcmp(out, key, 32) == 0, "auth key");
    wsMac(key, nonce, out);
    check(equalConstantTime(out, mac, 32), "ws mac");
    check(base64Encode(mac, 32) == field(v, "ws_mac"), "base64 encode");
    uint8_t zero[32] = {0};
    check(!sharedSecret(devPriv, zero, out, rng, nullptr), "rejects the zero point");
    check(!base64Decode("AAEC", out, 32), "rejects a short key");

    std::printf("%s\n", failures ? "FAILED" : "all passed");
    return failures ? 1 : 0;
}
