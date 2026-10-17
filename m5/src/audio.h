// Microphone capture and streamed playback. The CoreS3 cannot use both at once, so the module
// switches between them.
#pragma once

#include <Arduino.h>

namespace audio {

void begin();

/// Starts recording. Frames go to `onFrame` from update(), in the caller's task.
void startListening();
void stopListening();
bool isListening();

/// Appends speech PCM (16 kHz, 16-bit little-endian, mono). Safe from any task.
void pushSpeech(const uint8_t* data, size_t len);
/// Starts a speech: drops anything left over and switches to the speaker.
void beginSpeech();
/// No more PCM will come for this speech.
void endSpeech();
bool isSpeaking();

/// The level of what is playing now, 0.0 to 1.0, for the mouth.
float speechLevel();

/// Runs recording and playback. Call from loop(). `onFrame` receives each recorded frame,
/// `onSpeechDone` fires once when a speech has finished playing.
void update(void (*onFrame)(const int16_t* samples, size_t count), void (*onSpeechDone)());

}  // namespace audio
