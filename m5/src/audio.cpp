#include "audio.h"

#include <M5Unified.h>
#include <freertos/FreeRTOS.h>
#include <cmath>

#include "config.h"

namespace audio {
namespace {

// Playback: a ring buffer in PSRAM, fed from the network task and drained here.
uint8_t* ring = nullptr;
size_t ringSize = 0;
size_t ringHead = 0;  // write position
size_t ringTail = 0;  // read position
portMUX_TYPE ringLock = portMUX_INITIALIZER_UNLOCKED;

constexpr size_t kPlayChunk = 1600;  // samples, 100 ms
constexpr int kPlayBuffers = 3;
int16_t playBuf[kPlayBuffers][kPlayChunk];
int playIndex = 0;
float level = 0;

volatile bool speaking = false;
volatile bool speechEnded = false;

// Recording: three rotating buffers, as M5Unified's microphone queues two at a time.
constexpr int kRecBuffers = 3;
int16_t recBuf[kRecBuffers][config::kMicFrameSamples];
int recIndex = 0;
int recQueued = 0;
bool listening = false;
uint32_t listenStarted = 0;

size_t ringAvailable() { return (ringHead + ringSize - ringTail) % ringSize; }

size_t ringRead(uint8_t* out, size_t len) {
    portENTER_CRITICAL(&ringLock);
    size_t n = std::min(len, ringAvailable());
    for (size_t i = 0; i < n; i++) {
        out[i] = ring[ringTail];
        ringTail = (ringTail + 1) % ringSize;
    }
    portEXIT_CRITICAL(&ringLock);
    return n;
}

void useSpeaker() {
    if (M5.Mic.isEnabled()) M5.Mic.end();
    if (!M5.Speaker.isEnabled()) M5.Speaker.begin();
}

void useMic() {
    if (M5.Speaker.isEnabled()) {
        M5.Speaker.stop();
        M5.Speaker.end();
    }
    if (!M5.Mic.isEnabled()) M5.Mic.begin();
}

float rms(const int16_t* s, size_t n) {
    double sum = 0;
    for (size_t i = 0; i < n; i++) sum += double(s[i]) * s[i];
    return n ? std::sqrt(sum / n) / 32768.0f : 0;
}

}  // namespace

void begin() {
    ringSize = config::kSampleRate * 2 * config::kPlaybackSeconds;
    ring = static_cast<uint8_t*>(ps_malloc(ringSize));
    if (!ring) {
        ringSize = config::kSampleRate * 2 * 4;  // no PSRAM: 4 seconds
        ring = static_cast<uint8_t*>(malloc(ringSize));
    }
    auto mic = M5.Mic.config();
    mic.sample_rate = config::kSampleRate;
    M5.Mic.config(mic);
    M5.Speaker.setVolume(200);
}

void startListening() {
    if (listening || speaking) return;
    useMic();
    listening = true;
    listenStarted = millis();
    recIndex = 0;
    recQueued = 0;
}

void stopListening() { listening = false; }

bool isListening() { return listening || recQueued > 0; }

void pushSpeech(const uint8_t* data, size_t len) {
    if (!ring || !len) return;
    portENTER_CRITICAL(&ringLock);
    for (size_t i = 0; i < len; i++) {
        size_t next = (ringHead + 1) % ringSize;
        if (next == ringTail) break;  // full: drop the rest
        ring[ringHead] = data[i];
        ringHead = next;
    }
    portEXIT_CRITICAL(&ringLock);
}

void beginSpeech() {
    listening = false;
    portENTER_CRITICAL(&ringLock);
    ringHead = ringTail = 0;
    portEXIT_CRITICAL(&ringLock);
    speechEnded = false;
    speaking = true;
}

void endSpeech() { speechEnded = true; }

bool isSpeaking() { return speaking; }

float speechLevel() { return level; }

void update(void (*onFrame)(const int16_t*, size_t), void (*onSpeechDone)()) {
    // Recording. record() returns once the buffer is queued; with two in flight, the one queued
    // two calls ago is complete.
    if (listening && millis() - listenStarted > config::kMaxListenMs) listening = false;
    if (listening) {
        if (M5.Mic.record(recBuf[recIndex], config::kMicFrameSamples, config::kSampleRate)) {
            recQueued++;
            if (recQueued >= 3) {
                onFrame(recBuf[(recIndex + 1) % kRecBuffers], config::kMicFrameSamples);
                recQueued--;
            }
            recIndex = (recIndex + 1) % kRecBuffers;
        }
    } else if (recQueued > 0) {
        while (M5.Mic.isRecording()) delay(1);
        // The last `recQueued` buffers, oldest first.
        for (int k = recQueued; k > 0; k--) {
            onFrame(recBuf[(recIndex - k + kRecBuffers) % kRecBuffers], config::kMicFrameSamples);
        }
        recQueued = 0;
    }

    // Playback.
    if (!speaking) return;
    useSpeaker();
    while (M5.Speaker.isPlaying(0) < 2) {
        size_t avail = ringAvailable() & ~size_t(1);  // whole samples only
        bool last = speechEnded;
        if (avail < kPlayChunk * 2 && !last) break;  // wait for a whole chunk
        if (avail < 2) break;
        int16_t* buf = playBuf[playIndex];
        size_t got = ringRead(reinterpret_cast<uint8_t*>(buf), std::min(avail, kPlayChunk * 2)) / 2;
        if (!got) break;
        level = std::min(1.0f, rms(buf, got) * 4.0f);
        M5.Speaker.playRaw(buf, got, config::kSampleRate, false, 1, 0, false);
        playIndex = (playIndex + 1) % kPlayBuffers;
    }
    if (speechEnded && ringAvailable() < 2 && M5.Speaker.isPlaying(0) == 0) {
        speaking = false;
        speechEnded = false;
        level = 0;
        onSpeechDone();
    } else if (M5.Speaker.isPlaying(0) == 0) {
        level = 0;
    }
}

}  // namespace audio
