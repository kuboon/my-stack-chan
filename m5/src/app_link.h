// The direct-mode link to the app: the HTTP endpoints, pairing, and the authenticated WebSocket
// (web/server/docs/protocol.md), on whichever Wi-Fi net:: brought up.
//
// The web server runs in its own task. What it receives that the main loop must act on arrives
// as Commands through poll(); speech PCM goes straight to audio::pushSpeech().
#pragma once

#include <Arduino.h>

#include "camera.h"

namespace app_link {

enum class CommandType {
    ClientReady,   // an app authenticated
    ClientGone,    // the authenticated app disconnected
    ListenStart,
    ListenEnd,
    SpeakStart,    // text: what to show in the balloon (may be empty)
    SpeakEnd,
    Face,          // text: the expression
    Look,          // x: yaw -1..1, y: pitch 0..1
    State,         // text: "idle" | "thinking"
    PhotoTake,     // text: "vga" | "qvga"
    WifiScan,
    WifiConnect,   // text: the SSID, secret: the password (opened already)
    WifiForget,
};

struct Command {
    CommandType type;
    char text[160];
    char secret[72];
    float x;
    float y;
};

enum class PairingState { None, Waiting, Approved, Rejected };

/// Starts the server. Call after net::begin().
void begin();

/// The next command, if any.
bool poll(Command& out);

/// Whether an authenticated app is connected.
bool connected();

/// Sends a JSON text message to the authenticated app.
void sendText(const String& json);
/// Sends PCM to the authenticated app.
void sendAudio(const int16_t* samples, size_t count);
/// Sends a photo to the authenticated app, as a `photo` message.
bool sendPhoto(const camera::Photo& photo);

/// Pairing as the screen needs it: the state, and the code to show while it waits.
PairingState pairingState();
String pairingCode();
void approvePairing();
void rejectPairing();
bool paired();
/// Forgets the pairing and disconnects the app.
void unpair();

/// Expires a pairing nobody answered and a socket that never authenticated. Call from loop().
void update();

}  // namespace app_link
