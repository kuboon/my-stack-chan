#!/bin/sh
# Builds and runs the host tests (needs a C++17 compiler and libmbedtls-dev).
set -eu
here=$(cd "$(dirname "$0")" && pwd)
out=$(mktemp -d)
c++ -std=c++17 -Wall -Wextra -I"$here/../../src" "$here/test_pairing.cpp" "$here/../../src/pairing_crypto.cpp" \
  -lmbedcrypto -o "$out/test_pairing"
"$out/test_pairing" "$here/../vectors/vectors.json"
c++ -std=c++17 -Wall -Wextra -I"$here/../../src" "$here/test_actions.cpp" "$here/../../src/action_pieces.cpp" \
  -o "$out/test_actions"
"$out/test_actions"
