#include "settings.h"

#include <Preferences.h>
#include <esp_random.h>

namespace settings {
namespace {

Preferences prefs;

String randomPassword() {
    static const char kChars[] = "abcdefghjkmnpqrstuvwxyzACDEFGHJKLMNPQRSTUVWXYZ23456789";
    String out;
    for (int i = 0; i < 10; i++) out += kChars[esp_random() % (sizeof kChars - 1)];
    return out;
}

}  // namespace

void begin() { prefs.begin("stackchan", false); }

String apPassword() {
    String pw = prefs.getString("ap_pass", "");
    if (pw.length() < 8) {
        pw = randomPassword();
        prefs.putString("ap_pass", pw);
    }
    return pw;
}

bool authKey(uint8_t out[32]) { return prefs.getBytes("auth_key", out, 32) == 32; }

void setAuthKey(const uint8_t key[32]) { prefs.putBytes("auth_key", key, 32); }

void clearAuthKey() { prefs.remove("auth_key"); }

}  // namespace settings
