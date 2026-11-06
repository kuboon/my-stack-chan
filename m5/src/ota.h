// Firmware updates over Wi-Fi (web/server/docs/protocol.md, "ファームウェアの更新").
//
// The paired app announces the image over the authenticated WebSocket (ota.begin: size and
// SHA-256) and gets a one-time token back; it then POSTs the image to /ota?token=…. The image
// goes into the OTA slot the StackChan is not running from, and only an image whose size and
// SHA-256 match the announcement, and that the bootloader accepts, becomes the next boot.
//
// A new image boots on probation (the bootloader's rollback): it is kept once it has run for
// kOtaProbationMs, and a reset before that boots the previous one again.
#pragma once

#include <Arduino.h>

namespace ota {

/// What ota.begin answers.
struct Ticket {
    bool ok;
    String token;  // when ok
    String error;  // when not: "too_large" | "busy" | "bad_request"
};

/// A new update of `size` bytes whose SHA-256 is `sha256Hex`. Safe from any task.
Ticket prepare(size_t size, const char* sha256Hex);

/// Whether `token` is the one prepare() handed out, still unused and not expired.
bool accepts(const String& token);

/// A piece of the POSTed image, as AsyncWebServer hands it over (index: its offset, total: the
/// whole body). Returns false when the upload must stop.
bool write(const uint8_t* data, size_t len, size_t index, size_t total);

/// The upload is over. Empty on success (the next boot runs it); otherwise why not.
String finish();

/// The size the OTA slot can take.
size_t maxSize();

/// Keeps the running image once it has proved itself. Call from loop().
void update();

}  // namespace ota
