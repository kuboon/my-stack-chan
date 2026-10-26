// Host test of src/action_pieces.cpp: decoder ids -> Action JSON, against KoeAct-JP's examples.
#include <cstdio>
#include <string>

#include "action_pieces.h"

namespace {

int failures = 0;

void expect(const std::string& got, const std::string& want, const char* what) {
    bool ok = got == want;
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) {
        std::printf("     got  %s\n     want %s\n", got.c_str(), want.c_str());
        failures++;
    }
}

}  // namespace

int main() {
    using action_pieces::toJson;
    // The model card's example: 「顔を右に45度向けて」
    const int look[] = {1, 4, 21, 16, 53, 54, 19, 2, 59};
    expect(toJson(look, 9), R"([{"name":"look","arguments":{"direction":"right","degrees":45}}])",
           "look right 45 degrees");
    const int none[] = {0, 59};
    expect(toJson(none, 2), "[]", "no command");
    // Two calls: bow, then set_led blue (<s> is skipped, nothing after </s> counts).
    const int two[] = {60, 1, 8, 3, 10, 44, 18, 2, 59, 0};
    expect(toJson(two, 10), R"([{"name":"bow","arguments":{}},{"name":"set_led","arguments":{"color":"blue"}}])",
           "bow and set_led");
    const int nod[] = {1, 6, 52, 19, 2, 59};
    expect(toJson(nod, 6), R"([{"name":"nod","arguments":{"count":3}}])", "nod 3 times");
    const int adjust[] = {1, 12, 22, 15, 29, 18, 2, 59};
    expect(toJson(adjust, 8), R"([{"name":"adjust_volume","arguments":{"direction":"up","amount":"slight"}}])",
           "adjust_volume up slight");
    const int bad[] = {1, 99};
    expect(toJson(bad, 2), "", "unknown id");
    std::printf("%s\n", failures ? "FAILED" : "all passed");
    return failures ? 1 : 0;
}
