// Spoken commands on the StackChan itself, with KoeAct-JP (web/server/docs/m5-firmware.md,
// "音声の命令"): every recording also goes through it, and a command it is sure of runs here
// without the app — look, nod, expression, LED, volume, brightness.
//
// The model is two flash partitions written by tools/flash_koeact.sh: "model" (Ralomi, ESP-DL
// .espdl, run 0.4 s at a time while the user speaks) and "a2" (the Action decoder's weights).
// Without them begin() returns false and every recording goes to the app as before.
//
// The recognizer runs in its own task. The main loop hands it samples with push() and asks for
// the result with poll() after finish().
#pragma once

#include <Arduino.h>

namespace commands {

struct Result {
    /// The Action JSON the decoder wrote: "[]" or an array of calls. Empty when nothing ran.
    String actions;
    /// Whether p_act reached the threshold, so the calls should run.
    bool accepted = false;
    /// The probability that the utterance is a command (the first piece is not "[]").
    float pAct = 0;
    /// What Ralomi heard, in kana (the CTC side of the same model). Rough: no kanji, no spaces.
    String kana;
    /// From the end of the recording to the result.
    uint32_t tailMs = 0;
};

/// Loads the model. False if the partitions are missing or do not hold a model.
bool begin();
bool ready();

/// A new utterance. Drops whatever an earlier one left.
void start();
/// 16 kHz mono samples of the utterance. Call from one task only.
void push(const int16_t* samples, size_t count);
/// The utterance is over: the result comes through poll().
void finish();
/// The result of the last finished utterance, once.
bool poll(Result& out);
/// Whether an utterance is being recognized (between start() and its result).
bool busy();

}  // namespace commands
