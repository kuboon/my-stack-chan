#include "actions.h"

#include <ArduinoJson.h>
#include <M5StackChan.h>

#include "screen.h"

namespace actions {
namespace {

// Servo units are 0.1 degree. Yaw is 0 straight ahead (+ left); pitch 0 is level, up to 900.
constexpr int kRestPitch = 150;
constexpr int kYawLimit = 1200;
constexpr int kPitchLimit = 900;

struct Step {
    int yaw;
    int pitch;
    int speed;    // StackChan-BSP's 0..1000
    uint32_t ms;  // until the next step
};

constexpr int kMaxSteps = 32;
Step steps[kMaxSteps];
int count = 0;
int next = 0;
uint32_t nextAt = 0;

int amountDegrees(const char* amount) {
    if (!strcmp(amount, "slight")) return 15;
    if (!strcmp(amount, "large")) return 60;
    return 30;
}

/// The (yaw, pitch) offset of a direction by `degrees`.
void offset(const char* direction, int degrees, int& yaw, int& pitch) {
    const int d = degrees * 10;
    yaw = pitch = 0;
    if (strstr(direction, "left")) yaw = d;
    if (strstr(direction, "right")) yaw = -d;
    if (strstr(direction, "up")) pitch = d;
    if (strstr(direction, "down")) pitch = -d;
}

void queue(int yaw, int pitch, int speed, uint32_t ms) {
    if (count >= kMaxSteps) return;
    steps[count++] = {constrain(yaw, -kYawLimit, kYawLimit), constrain(pitch, 0, kPitchLimit), speed, ms};
}

int degreesOf(JsonObjectConst args) {
    if (args["degrees"].is<int>()) return args["degrees"].as<int>();
    return amountDegrees(args["amount"] | "normal");
}

uint8_t level255(int percent) { return uint8_t(constrain(percent, 0, 100) * 255 / 100); }

int adjusted(int current255, JsonObjectConst args) {
    int percent = current255 * 100 / 255;
    int by = args["by"].is<int>() ? args["by"].as<int>()
                                  : (!strcmp(args["amount"] | "normal", "slight") ? 10
                                     : !strcmp(args["amount"] | "normal", "large") ? 40
                                                                                  : 20);
    percent += !strcmp(args["direction"] | "up", "down") ? -by : by;
    return constrain(percent, 0, 100);
}

void setLed(const char* color) {
    struct Rgb {
        const char* name;
        uint8_t r, g, b;
    };
    static const Rgb kColors[] = {
        {"red", 255, 0, 0},     {"orange", 255, 96, 0},     {"yellow", 255, 200, 0},
        {"green", 0, 200, 0},   {"light_blue", 0, 160, 255}, {"blue", 0, 0, 255},
        {"purple", 140, 0, 255}, {"pink", 255, 80, 160},     {"white", 255, 255, 255},
        {"off", 0, 0, 0},
    };
    for (const Rgb& c : kColors) {
        if (!strcmp(color, c.name)) {
            M5StackChan.showRgbColor(c.r, c.g, c.b);
            return;
        }
    }
}

bool runCall(JsonObjectConst call) {
    const char* name = call["name"] | "";
    JsonObjectConst args = call["arguments"];
    auto& motion = M5StackChan.Motion;
    const int yaw0 = motion.getCurrentYawAngle();
    const int pitch0 = motion.getCurrentPitchAngle();

    if (!strcmp(name, "look")) {
        // From straight ahead: "look right 45 degrees" is a direction, not a turn by 45.
        const char* dir = args["direction"] | "center";
        int dy = 0, dp = 0;
        if (strcmp(dir, "center")) offset(dir, degreesOf(args), dy, dp);
        queue(dy, kRestPitch + dp, 500, 800);
    } else if (!strcmp(name, "turn")) {
        int dy = 0, dp = 0;
        offset(args["direction"] | "left", degreesOf(args), dy, dp);
        queue(yaw0 + dy, pitch0 + dp, 400, 1000);
    } else if (!strcmp(name, "nod")) {
        for (int i = 0; i < constrain(int(args["count"] | 1), 1, 5); i++) {
            queue(yaw0, 350, 800, 250);
            queue(yaw0, 0, 800, 300);
        }
        queue(yaw0, kRestPitch, 600, 400);
    } else if (!strcmp(name, "shake")) {
        for (int i = 0; i < constrain(int(args["count"] | 1), 1, 5); i++) {
            queue(yaw0 + 250, pitch0, 800, 250);
            queue(yaw0 - 250, pitch0, 800, 300);
        }
        queue(yaw0, pitch0, 600, 400);
    } else if (!strcmp(name, "bow")) {
        queue(0, 400, 400, 500);
        queue(0, 0, 300, 1200);
        queue(0, kRestPitch, 300, 800);
    } else if (!strcmp(name, "set_expression")) {
        const char* e = args["expression"] | "neutral";
        // The avatar has no "surprised"; doubt (raised brows) is the nearest.
        screen::setExpression(!strcmp(e, "surprised") ? "doubt" : e);
    } else if (!strcmp(name, "set_led")) {
        setLed(args["color"] | "off");
    } else if (!strcmp(name, "set_volume")) {
        M5.Speaker.setVolume(level255(args["level"] | 50));
    } else if (!strcmp(name, "adjust_volume")) {
        M5.Speaker.setVolume(level255(adjusted(M5.Speaker.getVolume(), args)));
    } else if (!strcmp(name, "set_brightness")) {
        M5.Display.setBrightness(level255(args["level"] | 50));
    } else if (!strcmp(name, "adjust_brightness")) {
        M5.Display.setBrightness(level255(adjusted(M5.Display.getBrightness(), args)));
    } else {
        return false;
    }
    return true;
}

}  // namespace

int run(const String& json) {
    JsonDocument doc;
    if (deserializeJson(doc, json) || !doc.is<JsonArrayConst>()) return 0;
    count = next = 0;
    int ran = 0;
    for (JsonObjectConst call : doc.as<JsonArrayConst>()) ran += runCall(call) ? 1 : 0;
    nextAt = millis();
    return ran;
}

void update() {
    if (next >= count || int32_t(millis() - nextAt) < 0) return;
    const Step& s = steps[next++];
    M5StackChan.Motion.move(s.yaw, s.pitch, s.speed);
    nextAt = millis() + s.ms;
}

bool moving() { return next < count || int32_t(millis() - nextAt) < 0; }

}  // namespace actions
