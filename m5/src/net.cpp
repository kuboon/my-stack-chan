#include "net.h"

#include <ESPmDNS.h>
#include <WiFi.h>

#include <algorithm>
#include <vector>

#include "config.h"
#include "settings.h"

namespace net {
namespace {

Mode currentMode = Mode::AP;
String deviceName;
String apPass;
String home;
bool failedAtBoot = false;

bool scanning = false;

bool tryingJoin = false;
JoinResult join = JoinResult::None;
uint32_t joinStarted = 0;
// Why the station last dropped, from the Wi-Fi task (0: no drop yet).
volatile uint8_t lastReason = 0;

String macSuffix() {
    uint8_t mac[6];
    WiFi.softAPmacAddress(mac);
    char buf[5];
    snprintf(buf, sizeof buf, "%02X%02X", mac[4], mac[5]);
    return buf;
}

void onDisconnected(arduino_event_id_t, arduino_event_info_t info) {
    lastReason = info.wifi_sta_disconnected.reason;
}

bool isWrongPassword(uint8_t reason) {
    return reason == WIFI_REASON_AUTH_FAIL || reason == WIFI_REASON_4WAY_HANDSHAKE_TIMEOUT ||
           reason == WIFI_REASON_HANDSHAKE_TIMEOUT || reason == WIFI_REASON_MIC_FAILURE;
}

bool isNotFound(uint8_t reason) {
    return reason == WIFI_REASON_NO_AP_FOUND || reason == WIFI_REASON_NO_AP_FOUND_W_COMPATIBLE_SECURITY ||
           reason == WIFI_REASON_NO_AP_FOUND_IN_AUTHMODE_THRESHOLD ||
           reason == WIFI_REASON_NO_AP_FOUND_IN_RSSI_THRESHOLD;
}

void startAp() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(deviceName.c_str(), apPass.c_str());
    currentMode = Mode::AP;
}

bool joinHome(const String& ssid, const String& password) {
    WiFi.mode(WIFI_STA);
    WiFi.setHostname(hostname().c_str());
    lastReason = 0;
    WiFi.begin(ssid.c_str(), password.c_str());
    uint32_t started = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - started < config::kHomeJoinTimeoutMs) {
        if (isWrongPassword(lastReason)) break;
        delay(100);
    }
    if (WiFi.status() != WL_CONNECTED) {
        WiFi.disconnect(true);
        return false;
    }
    WiFi.setAutoReconnect(true);
    MDNS.begin(hostname().c_str());
    MDNS.setInstanceName(deviceName.c_str());
    MDNS.addService("stackchan", "tcp", 80);
    return true;
}

}  // namespace

void begin(void (*onJoining)(const String& ssid)) {
    WiFi.persistent(false);  // the home Wi-Fi lives in our settings, not the driver's
    WiFi.onEvent(onDisconnected, ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    WiFi.mode(WIFI_AP);      // the MAC is read from the AP interface
    deviceName = "stackchan-" + macSuffix();
    apPass = settings::apPassword();

    String password;
    if (settings::homeWifi(home, password)) {
        onJoining(home);
        if (joinHome(home, password)) {
            currentMode = Mode::STA;
            WiFi.setSleep(false);
            return;
        }
        failedAtBoot = true;
    }
    startAp();
    WiFi.setSleep(false);
}

Mode mode() { return currentMode; }
const String& name() { return deviceName; }
const String& apPassword() { return apPass; }
const String& homeSsid() { return home; }

String hostname() {
    String h = deviceName;
    h.toLowerCase();
    return h;
}

String address() {
    return currentMode == Mode::STA ? WiFi.localIP().toString() : WiFi.softAPIP().toString();
}

bool homeFailed() { return failedAtBoot; }

void startScan() {
    if (scanning) return;
    if (currentMode == Mode::AP) WiFi.mode(WIFI_AP_STA);  // scanning needs the station
    scanning = WiFi.scanNetworks(true) == WIFI_SCAN_RUNNING;
}

bool scanResult(String& json) {
    if (!scanning) return false;
    int n = WiFi.scanComplete();
    if (n == WIFI_SCAN_RUNNING) return false;
    scanning = false;

    struct Network {
        String ssid;
        int rssi;
        bool secure;
    };
    std::vector<Network> found;
    for (int i = 0; i < n; i++) {
        String ssid = WiFi.SSID(i);
        if (ssid.isEmpty()) continue;
        auto same = std::find_if(found.begin(), found.end(), [&](const Network& x) { return x.ssid == ssid; });
        if (same != found.end()) {
            same->rssi = std::max(same->rssi, int(WiFi.RSSI(i)));
            continue;
        }
        found.push_back({ssid, WiFi.RSSI(i), WiFi.encryptionType(i) != WIFI_AUTH_OPEN});
    }
    WiFi.scanDelete();
    std::sort(found.begin(), found.end(), [](const Network& a, const Network& b) { return a.rssi > b.rssi; });
    if (found.size() > 20) found.resize(20);

    json = "{\"type\":\"wifi.networks\",\"networks\":[";
    for (size_t i = 0; i < found.size(); i++) {
        String ssid = found[i].ssid;
        ssid.replace("\\", "\\\\");
        ssid.replace("\"", "\\\"");
        json += String(i ? "," : "") + "{\"ssid\":\"" + ssid + "\",\"rssi\":" + found[i].rssi +
                ",\"secure\":" + (found[i].secure ? "true" : "false") + "}";
    }
    json += "]}";
    return true;
}

void startJoin(const String& ssid, const String& password) {
    if (currentMode == Mode::AP) WiFi.mode(WIFI_AP_STA);
    WiFi.setAutoReconnect(false);
    lastReason = 0;
    WiFi.disconnect();
    WiFi.begin(ssid.c_str(), password.c_str());
    tryingJoin = true;
    join = JoinResult::None;
    joinStarted = millis();
}

bool joining() { return tryingJoin; }

JoinResult takeJoinResult() {
    JoinResult r = join;
    join = JoinResult::None;
    return r;
}

String joinAddress() { return WiFi.localIP().toString(); }

void update() {
    if (!tryingJoin) return;
    JoinResult result = JoinResult::None;
    uint8_t reason = lastReason;
    if (WiFi.status() == WL_CONNECTED) result = JoinResult::Ok;
    else if (isWrongPassword(reason)) result = JoinResult::WrongPassword;
    else if (isNotFound(reason)) result = JoinResult::NotFound;
    else if (millis() - joinStarted > config::kWifiTryTimeoutMs) result = JoinResult::Timeout;
    if (result == JoinResult::None) return;
    // A failed attempt stops here, so that the AP goes back to its own channel.
    if (result != JoinResult::Ok) WiFi.disconnect();
    tryingJoin = false;
    join = result;
}

}  // namespace net
