#pragma once

#define STACKCHAN_FIRMWARE_VERSION "0.1.0"
#define STACKCHAN_MODEL "K151"

namespace config {

/// Audio on the wire and on the device: 16 kHz, 16-bit, mono (web/server/docs/protocol.md).
constexpr uint32_t kSampleRate = 16000;
/// Samples per microphone frame sent to the app: 50 ms.
constexpr size_t kMicFrameSamples = 800;
/// Longest a recording runs before it stops by itself.
constexpr uint32_t kMaxListenMs = 15000;
/// Seconds of speech the playback buffer holds (PSRAM).
constexpr size_t kPlaybackSeconds = 60;
/// How long the screen must be held to forget the pairing.
constexpr uint32_t kUnpairHoldMs = 10000;
/// How long a pairing waits for the touch that approves it.
constexpr uint32_t kPairingTimeoutMs = 60000;
/// How long a new WebSocket has to authenticate.
constexpr uint32_t kAuthTimeoutMs = 5000;
/// How long boot waits for the saved home Wi-Fi before it starts the AP instead.
constexpr uint32_t kHomeJoinTimeoutMs = 20000;
/// How long the app's `wifi.connect` may try.
constexpr uint32_t kWifiTryTimeoutMs = 15000;
/// How long after `wifi.result` (ok) or `wifi.forget` the StackChan restarts.
constexpr uint32_t kWifiRestartDelayMs = 3000;
/// How long the head stays where `look` put it before the idle motion moves it again.
constexpr uint32_t kLookHoldMs = 30000;
/// The longest the end of a recording waits for the spoken-command result (normally ~0.5 s).
constexpr uint32_t kCommandWaitMs = 3000;
/// Without an app, how long the face stays up after a spoken command.
constexpr uint32_t kCommandSessionMs = 10000;
/// Frames the camera drops while its exposure settles, before the one it keeps.
constexpr int kCameraWarmupFrames = 6;
/// frame2jpg()'s quality, 1 to 100.
constexpr uint8_t kCameraJpegQuality = 80;

}  // namespace config
