// KoeAct-JP's decoder output, as JSON (web/server/docs/protocol.md, "スタックちゃんで聞き取った命令").
//
// The decoder (lib/koeact/a2) emits ids into a fixed table of JSON pieces. Concatenated up to
// </s>, they are the Action JSON: [], or [CALL] / [CALL,CALL] that already passes the schema
// (the grammar allows nothing else). Host-tested by test/host/test_actions.cpp.
#pragma once

#include <string>

namespace action_pieces {

constexpr int kCount = 62;
constexpr int kEos = 59;  // </s>
constexpr int kBos = 60;  // <s>
constexpr int kPad = 61;  // <pad>

/// The piece of an id, or nullptr outside the table.
const char* piece(int id);

/// The JSON text of generated ids, up to </s>. Unknown ids make it empty.
std::string toJson(const int* ids, int count);

}  // namespace action_pieces
