// Wi-Fi: the home Wi-Fi when one is saved and it joins, or else the StackChan's own AP
// (web/server/docs/protocol.md, "AP" and "家の Wi-Fi"). Also finding and joining a home Wi-Fi
// for the app.
#pragma once

#include <Arduino.h>

namespace net {

enum class Mode { AP, STA };

/// Joins the saved home Wi-Fi, or else starts the AP. Blocks while it tries (up to 20 s);
/// `onJoining` is called first with the SSID it tries, for the screen.
void begin(void (*onJoining)(const String& ssid));

Mode mode();
/// `stackchan-XXXX`: the AP's SSID and the mDNS instance name.
const String& name();
/// The AP's WPA2 password.
const String& apPassword();
/// In STA: the home Wi-Fi, `stackchan-xxxx.local` and the address it got.
const String& homeSsid();
String hostname();
String address();
/// A home Wi-Fi was saved but did not join at boot (so the AP is up).
bool homeFailed();

/// Starts looking for Wi-Fi networks. The result comes from scanResult().
void startScan();
/// The `wifi.networks` message, once a scan has finished.
bool scanResult(String& json);

enum class JoinResult { None, Ok, WrongPassword, NotFound, Timeout };
/// Starts trying a home Wi-Fi, while the AP stays up. The result comes from takeJoinResult().
void startJoin(const String& ssid, const String& password);
bool joining();
/// The attempt's result, once, when it has one; None until then.
JoinResult takeJoinResult();
/// The address the attempt got, once it is Ok.
String joinAddress();

/// Call from loop().
void update();

}  // namespace net
