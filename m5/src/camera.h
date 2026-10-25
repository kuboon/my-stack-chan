// The camera (GC0308, 0.3 MP): one still photo on request, as JPEG. The sensor runs only for the
// shot, so that its DMA does not load the PSRAM the rest of the time.
#pragma once

#include <Arduino.h>

namespace camera {

struct Photo {
    uint8_t* jpeg = nullptr;  // free() it
    size_t len = 0;
    int width = 0;
    int height = 0;
    // Where the time went, in ms, and the frames the driver failed to deliver (each costs its
    // 4-second timeout).
    uint32_t initMs = 0;
    uint32_t warmupMs = 0;
    uint32_t shotMs = 0;
    uint32_t jpegMs = 0;
    int missed = 0;
    // How long each frame took to arrive: the warm-up ones, then the one kept.
    static constexpr int kMaxFrames = 8;
    uint32_t frameMs[kMaxFrames] = {};
    int frames = 0;
};

enum class Size { VGA, QVGA };

/// Takes a photo. Blocks for a second or more. Call from the loop task: the camera's control bus
/// is M5Unified's internal I2C, which the loop task also uses.
bool capture(Photo& out, Size size);

}  // namespace camera
