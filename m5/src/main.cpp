// StackChan (K151) firmware, direct mode: the StackChan is a Wi-Fi AP and the paired iPad app is
// its brain (web/server/docs/protocol.md).
#include <Arduino.h>
#include <ArduinoJson.h>
#include <M5StackChan.h>

#include "app_link.h"
#include "audio.h"
#include "camera.h"
#include "config.h"
#include "net.h"
#include "screen.h"
#include "settings.h"

namespace {

enum class View { None, Setup, Pairing, Waiting, HomeUnpaired, HomePaired, Face };
View view = View::None;
uint32_t pressStarted = 0;
uint32_t lastIdleLook = 0;
uint32_t idleHoldUntil = 0;  // after a `look` from the app
uint32_t restartAt = 0;      // 0: no restart due

void sendFrame(const int16_t* samples, size_t count) { app_link::sendAudio(samples, count); }

void speechDone() {
    screen::setMouth(0);
    app_link::sendText("{\"type\":\"speak.done\"}");
}

void startListening() {
    if (!app_link::connected() || audio::isSpeaking() || audio::isListening()) return;
    audio::startListening();
    screen::setListening(true);
    app_link::sendText("{\"type\":\"listen.start\"}");
}

void stopListening() {
    if (!audio::isListening()) return;
    audio::stopListening();
    // Flush the frames still in the microphone before the end marker.
    audio::update(sendFrame, speechDone);
    screen::setListening(false);
    app_link::sendText("{\"type\":\"listen.end\"}");
}

String joiningSsid;
String joiningPassword;

/// Answers `wifi.connect` once the attempt is over. Joined: saves it and restarts onto it.
void reportJoin(net::JoinResult result) {
    JsonDocument doc;
    doc["type"] = "wifi.result";
    doc["ok"] = result == net::JoinResult::Ok;
    const char* balloon = "";
    switch (result) {
        case net::JoinResult::Ok:
            doc["ssid"] = joiningSsid;
            doc["ip"] = net::joinAddress();
            settings::setHomeWifi(joiningSsid, joiningPassword);
            restartAt = millis() + config::kWifiRestartDelayMs;
            balloon = "つながったよ！さいきどうするね";
            break;
        case net::JoinResult::WrongPassword:
            doc["error"] = "wrong_password";
            balloon = "パスワードがちがうみたい";
            break;
        case net::JoinResult::NotFound:
            doc["error"] = "not_found";
            balloon = "その Wi-Fi がみつからないよ";
            break;
        default:
            doc["error"] = "timeout";
            balloon = "つながらなかったよ";
            break;
    }
    joiningPassword = "";
    // On the home Wi-Fi, a failed attempt left it: restart onto the saved one again.
    if (result != net::JoinResult::Ok && net::mode() == net::Mode::STA)
        restartAt = millis() + config::kWifiRestartDelayMs;
    String json;
    serializeJson(doc, json);
    app_link::sendText(json);
    screen::setBalloon(balloon);
}

void takePhoto(const char* size) {
    // The shot stops the loop for about a second, which recording and playback cannot afford.
    if (audio::isListening() || audio::isSpeaking()) {
        app_link::sendText("{\"type\":\"photo\",\"error\":\"busy\"}");
        return;
    }
    screen::setTakingPhoto(true);
    camera::Photo photo;
    bool taken = camera::capture(photo, !strcmp(size, "qvga") ? camera::Size::QVGA : camera::Size::VGA);
    screen::setTakingPhoto(false);
    if (!taken) {
        app_link::sendText("{\"type\":\"photo\",\"error\":\"camera\"}");
        return;
    }
    bool sent = app_link::sendPhoto(photo);
    free(photo.jpeg);
    if (!sent) app_link::sendText("{\"type\":\"photo\",\"error\":\"camera\"}");
}

/// The view the state calls for, drawn when it changes.
void updateView() {
    View want;
    bool home = net::mode() == net::Mode::STA;
    if (app_link::pairingState() == app_link::PairingState::Waiting) want = View::Pairing;
    else if (app_link::connected()) want = View::Face;
    else if (home) want = app_link::paired() ? View::HomePaired : View::HomeUnpaired;
    else want = app_link::paired() ? View::Waiting : View::Setup;
    if (want == view) return;
    view = want;
    String note = net::homeFailed() ? "Wi-Fi「" + net::homeSsid() + "」につながりませんでした" : "";
    switch (view) {
        case View::Setup: screen::showSetup(net::name(), net::apPassword(), note); break;
        case View::Pairing: screen::showPairingCode(app_link::pairingCode()); break;
        case View::Waiting: screen::showWaiting(net::name(), net::apPassword(), note); break;
        case View::HomeUnpaired:
        case View::HomePaired:
            screen::showOnHomeWifi(net::homeSsid(), net::hostname() + ".local", net::address(),
                                   view == View::HomePaired);
            break;
        case View::Face: screen::showFace(); break;
        default: break;
    }
}

void handleCommand(const app_link::Command& c) {
    using app_link::CommandType;
    switch (c.type) {
        case CommandType::ClientReady:
            M5StackChan.Motion.goHome(300);
            break;
        case CommandType::ClientGone:
            audio::stopListening();
            break;
        case CommandType::ListenStart: startListening(); break;
        case CommandType::ListenEnd: stopListening(); break;
        case CommandType::SpeakStart:
            if (audio::isListening()) stopListening();
            screen::setBalloon(c.text);
            break;
        case CommandType::SpeakEnd: break;
        case CommandType::Face: screen::setExpression(c.text); break;
        case CommandType::Look: {
            int yaw = constrain(int(c.x * 600), -600, 600);
            int pitch = constrain(int(c.y * 900), 0, 900);
            M5StackChan.Motion.move(yaw, pitch, 400);
            idleHoldUntil = millis() + config::kLookHoldMs;
            break;
        }
        case CommandType::State: screen::setThinking(!strcmp(c.text, "thinking")); break;
        case CommandType::PhotoTake: takePhoto(c.text); break;
        case CommandType::WifiScan: net::startScan(); break;
        case CommandType::WifiConnect:
            if (net::joining()) break;
            joiningSsid = c.text;
            joiningPassword = c.secret;
            net::startJoin(joiningSsid, joiningPassword);
            screen::setBalloon("つないでみるね");
            break;
        case CommandType::WifiForget:
            settings::clearHomeWifi();
            restartAt = millis() + config::kWifiRestartDelayMs;
            screen::setBalloon("Wi-Fi をわすれたよ");
            break;
    }
}

void handleTouch() {
    auto t = M5.Touch.getDetail();
    if (t.wasPressed()) pressStarted = millis();
    if (t.isPressed() && pressStarted && millis() - pressStarted > config::kUnpairHoldMs) {
        pressStarted = 0;
        audio::stopListening();
        app_link::unpair();
        return;
    }
    if (!t.wasClicked()) return;
    if (view == View::Pairing) {
        switch (screen::pairingButtonAt(t.x, t.y)) {
            case screen::Button::Approve: app_link::approvePairing(); break;
            case screen::Button::Cancel: app_link::rejectPairing(); break;
            default: break;
        }
    } else if (view == View::Face) {
        if (audio::isListening()) stopListening();
        else startListening();
    }
}

void handleBodyTouch() {
    auto& ts = M5StackChan.TouchSensor;
    const char* gesture = nullptr;
    if (ts.wasSwipedForward()) gesture = "swipe_forward";
    else if (ts.wasSwipedBackward()) gesture = "swipe_backward";
    else if (ts.wasClicked()) gesture = "tap";
    if (!gesture || !app_link::connected()) return;
    app_link::sendText(String("{\"type\":\"touch\",\"gesture\":\"") + gesture + "\"}");
}

/// Now and then, look somewhere else, as a living thing would.
void idleMotion() {
    if (view != View::Face || audio::isSpeaking() || audio::isListening()) return;
    if (millis() - lastIdleLook < 8000 || int32_t(idleHoldUntil - millis()) > 0) return;
    lastIdleLook = millis();
    M5StackChan.Motion.move(random(-300, 301), random(100, 401), 150);
}

}  // namespace

void setup() {
    M5StackChan.begin();
    settings::begin();
    screen::begin();
    audio::begin();
    net::begin(screen::showJoining);
    app_link::begin();
}

void loop() {
    M5StackChan.update();
    app_link::update();

    app_link::Command c;
    while (app_link::poll(c)) {
        handleCommand(c);
        memset(c.secret, 0, sizeof c.secret);
    }

    net::update();
    String networks;
    if (net::scanResult(networks)) app_link::sendText(networks);
    net::JoinResult joined = net::takeJoinResult();
    if (joined != net::JoinResult::None) reportJoin(joined);
    if (restartAt && int32_t(millis() - restartAt) >= 0) ESP.restart();

    updateView();
    handleTouch();
    handleBodyTouch();

    audio::update(sendFrame, speechDone);
    if (audio::isSpeaking()) screen::setMouth(audio::speechLevel());
    idleMotion();
    delay(1);
}
