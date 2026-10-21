// StackChan (K151) firmware, direct mode: the StackChan is a Wi-Fi AP and the paired iPad app is
// its brain (web/server/docs/protocol.md).
#include <Arduino.h>
#include <M5StackChan.h>

#include "app_link.h"
#include "audio.h"
#include "camera.h"
#include "config.h"
#include "screen.h"
#include "settings.h"

namespace {

enum class View { None, Setup, Pairing, Waiting, Face };
View view = View::None;
uint32_t pressStarted = 0;
uint32_t lastIdleLook = 0;

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
    if (app_link::pairingState() == app_link::PairingState::Waiting) want = View::Pairing;
    else if (!app_link::paired()) want = View::Setup;
    else if (!app_link::connected()) want = View::Waiting;
    else want = View::Face;
    if (want == view) return;
    view = want;
    switch (view) {
        case View::Setup: screen::showSetup(app_link::ssid(), app_link::password()); break;
        case View::Pairing: screen::showPairingCode(app_link::pairingCode()); break;
        case View::Waiting: screen::showWaiting(app_link::ssid(), app_link::password()); break;
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
            break;
        }
        case CommandType::State: screen::setThinking(!strcmp(c.text, "thinking")); break;
        case CommandType::PhotoTake: takePhoto(c.text); break;
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
    if (millis() - lastIdleLook < 8000) return;
    lastIdleLook = millis();
    M5StackChan.Motion.move(random(-300, 301), random(100, 401), 150);
}

}  // namespace

void setup() {
    M5StackChan.begin();
    settings::begin();
    screen::begin();
    audio::begin();
    app_link::begin();
}

void loop() {
    M5StackChan.update();
    app_link::update();

    app_link::Command c;
    while (app_link::poll(c)) handleCommand(c);

    updateView();
    handleTouch();
    handleBodyTouch();

    audio::update(sendFrame, speechDone);
    if (audio::isSpeaking()) screen::setMouth(audio::speechLevel());
    idleMotion();
    delay(1);
}
