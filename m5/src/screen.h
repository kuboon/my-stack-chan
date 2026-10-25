// What the display shows: setup screens drawn directly, or the avatar once an app is connected.
#pragma once

#include <Arduino.h>

namespace screen {

void begin();

/// Not paired: how to join the AP, and what to do next. `note` (may be empty) is shown above
/// that, such as why the home Wi-Fi was not joined.
void showSetup(const String& ssid, const String& password, const String& note);
/// A pairing is waiting: the code, and the approve / cancel buttons.
void showPairingCode(const String& code);
/// Paired, but no app is connected.
void showWaiting(const String& ssid, const String& password, const String& note);
/// Booting: trying the saved home Wi-Fi.
void showJoining(const String& ssid);
/// On the home Wi-Fi with no app connected: where the app finds it.
void showOnHomeWifi(const String& ssid, const String& host, const String& address, bool paired);
/// The avatar.
void showFace();

enum class Button { None, Approve, Cancel };
/// Which pairing button a touch at (x, y) hits.
Button pairingButtonAt(int x, int y);

// The avatar, while it is shown.
void setExpression(const char* name);
void setMouth(float ratio);
void setBalloon(const char* text);
void setThinking(bool thinking);
void setListening(bool listening);
/// The balloon while a photo is taken. The avatar stops moving until it is done.
void setTakingPhoto(bool taking);

}  // namespace screen
