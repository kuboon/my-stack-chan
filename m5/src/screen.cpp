#include "screen.h"

#include <Avatar.h>
#include <M5Unified.h>

namespace screen {
namespace {

using namespace m5avatar;

Avatar avatar;
bool avatarStarted = false;
bool avatarShown = false;

constexpr int kButtonTop = 180;
constexpr int kButtonHeight = 50;

void hideAvatar() {
    if (avatarShown) {
        avatar.suspend();
        avatarShown = false;
    }
}

void drawWifi(const String& ssid, const String& password, int y) {
    auto& d = M5.Display;
    String qr = "WIFI:T:WPA;S:" + ssid + ";P:" + password + ";;";
    d.qrcode(qr.c_str(), 8, y, 140, 3);
    d.setFont(&fonts::lgfxJapanGothic_16);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(160, y + 10);
    d.print("Wi-Fi");
    d.setCursor(160, y + 34);
    d.print(ssid);
    d.setCursor(160, y + 66);
    d.print("パスワード");
    d.setCursor(160, y + 90);
    d.print(password);
}

}  // namespace

void begin() {
    M5.Display.setRotation(1);
    M5.Display.fillScreen(TFT_BLACK);
}

void showSetup(const String& ssid, const String& password) {
    hideAvatar();
    auto& d = M5.Display;
    d.fillScreen(TFT_BLACK);
    drawWifi(ssid, password, 10);
    d.setFont(&fonts::lgfxJapanGothic_16);
    d.setTextColor(TFT_YELLOW, TFT_BLACK);
    d.setCursor(8, 170);
    d.print("iPad をこの Wi-Fi につなぎ、");
    d.setCursor(8, 194);
    d.print("アプリでペアリングしてください");
}

void showPairingCode(const String& code) {
    hideAvatar();
    auto& d = M5.Display;
    d.fillScreen(TFT_BLACK);
    d.setFont(&fonts::lgfxJapanGothic_16);
    d.setTextColor(TFT_WHITE, TFT_BLACK);
    d.setCursor(8, 12);
    d.print("iPad と同じ番号なら「承認」");
    d.setFont(&fonts::Font7);
    d.setTextColor(TFT_GREENYELLOW, TFT_BLACK);
    d.setTextDatum(middle_center);
    d.drawString(code, d.width() / 2, 100);
    d.setTextDatum(top_left);
    d.fillRoundRect(8, kButtonTop, 148, kButtonHeight, 8, TFT_DARKGREEN);
    d.fillRoundRect(164, kButtonTop, 148, kButtonHeight, 8, TFT_DARKGREY);
    d.setFont(&fonts::lgfxJapanGothic_24);
    d.setTextColor(TFT_WHITE);
    d.setTextDatum(middle_center);
    d.drawString("承認", 8 + 74, kButtonTop + kButtonHeight / 2);
    d.drawString("やめる", 164 + 74, kButtonTop + kButtonHeight / 2);
    d.setTextDatum(top_left);
}

void showWaiting(const String& ssid, const String& password) {
    hideAvatar();
    auto& d = M5.Display;
    d.fillScreen(TFT_BLACK);
    drawWifi(ssid, password, 10);
    d.setFont(&fonts::lgfxJapanGothic_16);
    d.setTextColor(TFT_CYAN, TFT_BLACK);
    d.setCursor(8, 170);
    d.print("iPad のアプリを待っています");
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setCursor(8, 200);
    d.print("10 秒長押しでペアリングを解除");
}

void showFace() {
    if (avatarShown) return;
    if (!avatarStarted) {
        avatar.init();
        avatar.setSpeechFont(&fonts::lgfxJapanGothic_16);
        avatarStarted = true;
    } else {
        avatar.resume();
    }
    avatarShown = true;
}

Button pairingButtonAt(int x, int y) {
    if (y < kButtonTop || y > kButtonTop + kButtonHeight) return Button::None;
    if (x >= 8 && x < 156) return Button::Approve;
    if (x >= 164 && x < 312) return Button::Cancel;
    return Button::None;
}

void setExpression(const char* name) {
    if (!avatarStarted) return;
    Expression e = Expression::Neutral;
    if (!strcmp(name, "happy")) e = Expression::Happy;
    else if (!strcmp(name, "sad")) e = Expression::Sad;
    else if (!strcmp(name, "angry")) e = Expression::Angry;
    else if (!strcmp(name, "doubt")) e = Expression::Doubt;
    else if (!strcmp(name, "sleepy")) e = Expression::Sleepy;
    avatar.setExpression(e);
}

void setMouth(float ratio) {
    if (avatarStarted) avatar.setMouthOpenRatio(ratio);
}

void setBalloon(const char* text) {
    if (avatarStarted) avatar.setSpeechText(text);
}

void setThinking(bool thinking) {
    if (!avatarStarted) return;
    avatar.setExpression(thinking ? Expression::Doubt : Expression::Neutral);
    avatar.setSpeechText(thinking ? "..." : "");
}

void setListening(bool listening) {
    if (!avatarStarted) return;
    avatar.setSpeechText(listening ? "きいてるよ" : "");
}

void setTakingPhoto(bool taking) {
    if (!avatarShown) return;
    if (taking) {
        avatar.setExpression(Expression::Happy);
        avatar.setSpeechText("とるよ〜");
        // Let the avatar draw that, then stop it: its frames share the PSRAM with the camera's DMA.
        delay(150);
        avatar.suspend();
    } else {
        avatar.resume();
        avatar.setExpression(Expression::Neutral);
        avatar.setSpeechText("");
    }
}

}  // namespace screen
