#include "ota.h"

#include <Update.h>
#include <esp_ota_ops.h>
#include <esp_random.h>
#include <freertos/FreeRTOS.h>
#include <freertos/semphr.h>
#include <mbedtls/md.h>

#include "config.h"
#include "ota_pubkey.h"
#include "ota_signature.h"

// Arduino marks a new image valid as soon as it starts; ask it to leave that to update().
bool verifyRollbackLater() { return true; }

namespace ota {
namespace {

SemaphoreHandle_t lock = xSemaphoreCreateMutex();
String token;
uint32_t tokenAt = 0;
size_t expectedSize = 0;
uint8_t expectedSha[32];
bool tokenUsed = false;

mbedtls_md_context_t sha;
bool writing = false;
uint32_t lastWriteAt = 0;
bool failed = false;
String failure;

bool probationChecked = false;

// A signed update (POST /ota/signed): the image is checked against `signature` once it is in.
bool signedUpdate = false;
constexpr size_t kMaxSignatureLength = 80;  // a P-256 DER signature is at most 72 bytes
uint8_t signature[kMaxSignatureLength];
size_t signatureLength = 0;

/// An upload that stopped coming (the connection dropped) no longer holds the slot. With the lock.
void dropStaleUpload() {
    if (writing && millis() - lastWriteAt > 30000) {
        Update.abort();
        mbedtls_md_free(&sha);
        writing = false;
    }
}

struct Guard {
    Guard() { xSemaphoreTake(lock, portMAX_DELAY); }
    ~Guard() { xSemaphoreGive(lock); }
};

bool parseHex(const char* hex, uint8_t* out, size_t len) {
    if (!hex || strlen(hex) != len * 2) return false;
    for (size_t i = 0; i < len; i++) {
        auto nibble = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        int hi = nibble(hex[2 * i]), lo = nibble(hex[2 * i + 1]);
        if (hi < 0 || lo < 0) return false;
        out[i] = uint8_t(hi << 4 | lo);
    }
    return true;
}

void fail(const char* why) {
    if (!failed) failure = why;
    failed = true;
}

}  // namespace

size_t maxSize() {
    const esp_partition_t* next = esp_ota_get_next_update_partition(nullptr);
    return next ? next->size : 0;
}

Ticket prepare(size_t size, const char* sha256Hex) {
    Ticket t{false, "", ""};
    uint8_t hash[32];
    if (!size || !parseHex(sha256Hex, hash, sizeof hash)) {
        t.error = "bad_request";
        return t;
    }
    if (size > maxSize()) {
        t.error = "too_large";
        return t;
    }
    Guard g;
    dropStaleUpload();
    if (writing) {
        t.error = "busy";
        return t;
    }
    signedUpdate = false;
    uint8_t raw[16];
    esp_fill_random(raw, sizeof raw);
    token = "";
    for (uint8_t b : raw) {
        char hex[3];
        snprintf(hex, sizeof hex, "%02x", b);
        token += hex;
    }
    tokenAt = millis();
    tokenUsed = false;
    expectedSize = size;
    memcpy(expectedSha, hash, sizeof hash);
    t.ok = true;
    t.token = token;
    return t;
}

bool signedEnabled() { return kPublicKeyLength > 0; }

bool beginSigned(const String& signatureHex, size_t total) {
    if (!signedEnabled() || !total || total > maxSize()) return false;
    Guard g;
    dropStaleUpload();
    if (writing) return false;
    size_t length = 0;
    if (!ota_signature::hexToBytes(signatureHex.c_str(), signature, kMaxSignatureLength, length)) return false;
    signatureLength = length;
    signedUpdate = true;
    // Handed on to write() like a token's update, with the size the body really has. There is no
    // token for it: accepts() says no while a signed update is staged.
    token = "";
    tokenUsed = false;
    expectedSize = total;
    return true;
}

bool accepts(const String& candidate) {
    Guard g;
    return !signedUpdate && token.length() && !tokenUsed && candidate == token &&
           millis() - tokenAt < config::kOtaTokenTtlMs;
}

bool write(const uint8_t* data, size_t len, size_t index, size_t total) {
    if (index == 0) {
        Guard g;
        if (writing || tokenUsed || total != expectedSize) return false;
        tokenUsed = true;
        writing = true;
        failed = false;
        failure = "";
        mbedtls_md_init(&sha);
        mbedtls_md_setup(&sha, mbedtls_md_info_from_type(MBEDTLS_MD_SHA256), 0);
        mbedtls_md_starts(&sha);
        if (!Update.begin(total, U_FLASH)) fail("begin");
    }
    if (!writing || failed) return false;
    lastWriteAt = millis();
    mbedtls_md_update(&sha, data, len);
    if (Update.write(const_cast<uint8_t*>(data), len) != len) fail("write");
    return !failed;
}

String finish() {
    Guard g;
    if (!writing) return "not_started";
    writing = false;
    token = "";
    uint8_t got[32];
    mbedtls_md_finish(&sha, got);
    mbedtls_md_free(&sha);
    bool wasSigned = signedUpdate;
    signedUpdate = false;
    if (!failed && wasSigned) {
        // The image is only the next boot if it is the developer's.
        if (!ota_signature::verify(kPublicKey, kPublicKeyLength, got, signature, signatureLength))
            fail("signature");
    } else if (!failed && memcmp(got, expectedSha, sizeof got) != 0) {
        fail("sha256");
    }
    if (!failed && Update.progress() != expectedSize) fail("size");
    if (failed) {
        Update.abort();
        log_e("OTA: failed (%s)", failure.c_str());
        return failure;
    }
    // end() checks the image and makes it the next boot.
    if (!Update.end(false)) {
        log_e("OTA: the image was refused (%s)", Update.errorString());
        return "image";
    }
    log_i("OTA: %u bytes written; the next boot runs them", unsigned(expectedSize));
    return "";
}

void update() {
    if (probationChecked || millis() < config::kOtaProbationMs) return;
    probationChecked = true;
    esp_ota_img_states_t state;
    const esp_partition_t* running = esp_ota_get_running_partition();
    if (esp_ota_get_state_partition(running, &state) == ESP_OK && state == ESP_OTA_IMG_PENDING_VERIFY) {
        esp_ota_mark_app_valid_cancel_rollback();
        log_i("OTA: the new firmware is kept");
    }
}

}  // namespace ota
