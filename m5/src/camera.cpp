#include "camera.h"

#include <M5Unified.h>
#include <esp_camera.h>
#include <img_converters.h>

#include "config.h"

namespace camera {
namespace {

// CoreS3's camera pins (docs.m5stack.com/en/arduino/m5cores3/camera). The sensor has its own
// clock, and its SCCB is the internal I2C bus, which Arduino's Wire1 already holds: the driver
// shares that bus instead of opening the pins again.
camera_config_t makeConfig(Size size) {
    camera_config_t c{};
    c.pin_pwdn = -1;
    c.pin_reset = -1;
    c.pin_xclk = -1;
    c.pin_sccb_sda = -1;
    c.pin_sccb_scl = -1;
    c.sccb_i2c_port = M5.In_I2C.getPort();
    c.pin_d7 = 47;
    c.pin_d6 = 48;
    c.pin_d5 = 16;
    c.pin_d4 = 15;
    c.pin_d3 = 42;
    c.pin_d2 = 41;
    c.pin_d1 = 40;
    c.pin_d0 = 39;
    c.pin_vsync = 46;
    c.pin_href = 38;
    c.pin_pclk = 45;
    c.xclk_freq_hz = 20000000;
    c.ledc_timer = LEDC_TIMER_0;
    c.ledc_channel = LEDC_CHANNEL_0;
    c.pixel_format = PIXFORMAT_RGB565;  // the GC0308 has no JPEG: frame2jpg() encodes it
    c.frame_size = size == Size::QVGA ? FRAMESIZE_QVGA : FRAMESIZE_VGA;
    c.jpeg_quality = 0;
    c.fb_count = 1;
    c.fb_location = CAMERA_FB_IN_PSRAM;
    c.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
    return c;
}

/// The next frame, and how long it took to come.
camera_fb_t* nextFrame(Photo& out) {
    uint32_t t = millis();
    camera_fb_t* fb = esp_camera_fb_get();
    if (out.frames < Photo::kMaxFrames) out.frameMs[out.frames++] = millis() - t;
    if (!fb) out.missed++;
    return fb;
}

}  // namespace

bool capture(Photo& out, Size size) {
    out = Photo{};
    camera_config_t config = makeConfig(size);
    uint32_t t = millis();
    esp_err_t err = esp_camera_init(&config);
    out.initMs = millis() - t;
    if (err != ESP_OK) {
        log_e("camera init failed: %s", esp_err_to_name(err));
        return false;
    }
    // The first frames come before the exposure and white balance have settled.
    t = millis();
    for (int i = 0; i < config::kCameraWarmupFrames; i++) {
        if (camera_fb_t* fb = nextFrame(out)) esp_camera_fb_return(fb);
    }
    out.warmupMs = millis() - t;
    bool ok = false;
    t = millis();
    camera_fb_t* fb = nextFrame(out);
    out.shotMs = millis() - t;
    if (fb) {
        out.width = fb->width;
        out.height = fb->height;
        t = millis();
        ok = frame2jpg(fb, config::kCameraJpegQuality, &out.jpeg, &out.len);
        out.jpegMs = millis() - t;
        esp_camera_fb_return(fb);
    }
    esp_camera_deinit();
    log_i("camera: init %u warmup %u shot %u jpeg %u ms, missed %d, %u bytes", out.initMs, out.warmupMs,
          out.shotMs, out.jpegMs, out.missed, out.len);
    if (!ok) {
        log_e("camera capture failed");
        free(out.jpeg);
        out = Photo{};
    }
    return ok;
}

}  // namespace camera
