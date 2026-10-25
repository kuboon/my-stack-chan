// What the firmware keeps across restarts, in NVS.
#pragma once

#include <Arduino.h>

namespace settings {

void begin();

/// The AP's WPA2 password, made on first boot.
String apPassword();

/// The key shared with the paired app. False when not paired.
bool authKey(uint8_t out[32]);
void setAuthKey(const uint8_t key[32]);
void clearAuthKey();

/// The home Wi-Fi to join at boot. False when none is saved.
bool homeWifi(String& ssid, String& password);
void setHomeWifi(const String& ssid, const String& password);
void clearHomeWifi();

}  // namespace settings
