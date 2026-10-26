// Runs the calls of KoeAct-JP's Action schema v1 on the StackChan: look, turn, nod, shake, bow,
// set_expression, set_led, (set|adjust)_volume, (set|adjust)_brightness. Motions are queued
// and played from update(), so nothing here blocks the loop.
#pragma once

#include <Arduino.h>

namespace actions {

/// Runs a JSON array of calls (the decoder's output). Returns how many it ran.
int run(const String& json);

/// Plays the queued motion. Call from loop().
void update();

/// Whether a motion is playing (the idle look waits for it).
bool moving();

}  // namespace actions
