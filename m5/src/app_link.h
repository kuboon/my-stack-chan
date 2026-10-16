// The direct-mode link to the app: the AP, the HTTP endpoints, pairing, and the authenticated
// WebSocket (web/server/docs/protocol.md).
//
// The web server runs in its own task. What it receives that the main loop must act on arrives
// as Commands through poll(); speech PCM goes straight to audio::pushSpeech().
#pragma once

#include <Arduino.h>

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
};

struct Command {
    CommandType type;
    char text[160];
    float x;
    float y;
};

enum class PairingState { None, Waiting, Approved, Rejected };

void begin();

/// The AP's SSID and password, for the screen.
const String& ssid();
const String& password();

/// The next command, if any.
bool poll(Command& out);

/// Whether an authenticated app is connected.
bool connected();

/// Sends a JSON text message to the authenticated app.
void sendText(const String& json);
/// Sends PCM to the authenticated app.
void sendAudio(const int16_t* samples, size_t count);

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
