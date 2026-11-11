// Host test of src/ota_signature.cpp. The key is a throwaway made for this test; the signature is
// `openssl dgst -sha256 -sign` over the message "stackchan test firmware".
// Run: m5/test/host/run.sh
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "ota_signature.h"

namespace {

int failures = 0;

void check(bool ok, const char* what) {
    std::printf("%s %s
", ok ? "ok  " : "FAIL", what);
    if (!ok) failures++;
}

const char* kPublicKey = "3059301306072a8648ce3d020106082a8648ce3d030107034200047ae9aaa13a4ffc1029029e0a2c2c4dca73bb16a24ffaf01cab694eeb9c29de00e409e70263c5780ffa79a125023093d014699b250eb25dc9f74aec76064a2956";
const char* kSignature = "304402205360913ad0fe004524bdab642c6423ce5c48fdcdccc3f07b00f4da1471a274a70220775b894006bb49e9cba88622a30957cd5f3809bae745d8c057bfa5dae08beb19";
const char* kHash = "4cbaf1902fc910b827187f3b6ba78dfc128a9c1c0186bc73e25c6a2f38f335c0";

std::vector<uint8_t> bytes(const std::string& hex) {
    std::vector<uint8_t> out(hex.size() / 2 + 1);
    size_t n = 0;
    ota_signature::hexToBytes(hex.c_str(), out.data(), out.size(), n);
    out.resize(n);
    return out;
}

}  // namespace

int main() {
    auto pub = bytes(kPublicKey), sig = bytes(kSignature), hash = bytes(kHash);
    check(pub.size() == 91 && sig.size() > 60 && hash.size() == 32, "the vectors decode");

    check(ota_signature::verify(pub.data(), pub.size(), hash.data(), sig.data(), sig.size()),
          "the signature of the hash verifies");

    auto other = hash;
    other[0] ^= 1;
    check(!ota_signature::verify(pub.data(), pub.size(), other.data(), sig.data(), sig.size()),
          "another hash does not");

    auto bad = sig;
    bad[bad.size() - 1] ^= 1;
    check(!ota_signature::verify(pub.data(), pub.size(), hash.data(), bad.data(), bad.size()),
          "a changed signature does not");

    check(!ota_signature::verify(pub.data(), pub.size(), hash.data(), sig.data(), sig.size() - 1),
          "a short signature does not");
    check(!ota_signature::verify(pub.data(), pub.size(), hash.data(), nullptr, 0), "no signature does not");
    check(!ota_signature::verify(nullptr, 0, hash.data(), sig.data(), sig.size()), "no key does not");

    auto otherKey = pub;
    otherKey[pub.size() - 1] ^= 1;  // a different point: not even on the curve, or another key
    check(!ota_signature::verify(otherKey.data(), otherKey.size(), hash.data(), sig.data(), sig.size()),
          "another key does not");

    uint8_t out[4];
    size_t n = 0;
    check(ota_signature::hexToBytes("00ff10Ab", out, 4, n) && n == 4 && out[1] == 0xff && out[3] == 0xab,
          "hex decodes, either case");
    check(!ota_signature::hexToBytes("abc", out, 4, n), "an odd number of digits is refused");
    check(!ota_signature::hexToBytes("zz", out, 4, n), "a non-hex digit is refused");
    check(!ota_signature::hexToBytes("0011223344", out, 4, n), "more than the capacity is refused");
    check(!ota_signature::hexToBytes("", out, 4, n), "nothing is refused");

    std::printf("%s
", failures ? "FAILED" : "all passed");
    return failures ? 1 : 0;
}
