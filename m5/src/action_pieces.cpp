#include "action_pieces.h"

namespace action_pieces {
namespace {

// KoeAct-JP's VOCAB (code/koeact/model/pieces.py at revision 55ca644): the JSON pieces, the enum
// values and digits, then </s>, <s>, <pad>. The order is the decoder's id order.
const char* const kPieces[kCount] = {
    "[]",
    "[",
    "]",
    ",",
    "{\"name\":\"look\",\"arguments\":{\"direction\":\"",
    "{\"name\":\"turn\",\"arguments\":{\"direction\":\"",
    "{\"name\":\"nod\",\"arguments\":{\"count\":",
    "{\"name\":\"shake\",\"arguments\":{\"count\":",
    "{\"name\":\"bow\",\"arguments\":{}}",
    "{\"name\":\"set_expression\",\"arguments\":{\"expression\":\"",
    "{\"name\":\"set_led\",\"arguments\":{\"color\":\"",
    "{\"name\":\"set_volume\",\"arguments\":{\"level\":",
    "{\"name\":\"adjust_volume\",\"arguments\":{\"direction\":\"",
    "{\"name\":\"set_brightness\",\"arguments\":{\"level\":",
    "{\"name\":\"adjust_brightness\",\"arguments\":{\"direction\":\"",
    "\",\"amount\":\"",
    "\",\"degrees\":",
    "\",\"by\":",
    "\"}}",
    "}}",
    "left", "right", "up", "down", "up_left", "up_right", "down_left", "down_right", "center",
    "slight", "normal", "large",
    "happy", "sad", "surprised", "neutral", "angry", "sleepy", "doubt",
    "red", "orange", "yellow", "green", "light_blue", "blue", "purple", "pink", "white", "off",
    "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "</s>", "<s>", "<pad>",
};

}  // namespace

const char* piece(int id) { return id >= 0 && id < kCount ? kPieces[id] : nullptr; }

std::string toJson(const int* ids, int count) {
    std::string out;
    for (int i = 0; i < count; i++) {
        int id = ids[i];
        if (id == kEos) break;
        if (id == kBos || id == kPad) continue;
        const char* p = piece(id);
        if (!p) return "";
        out += p;
    }
    return out;
}

}  // namespace action_pieces
