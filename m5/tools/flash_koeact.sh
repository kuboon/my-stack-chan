#!/bin/sh
# Writes KoeAct-JP's model into the StackChan's "model" and "a2" flash partitions (spoken
# commands; see lib/koeact/README.md). Run once after flashing a firmware with partitions.csv:
#
#   m5/tools/flash_koeact.sh [serial port]
#
# The files come from a fixed revision of https://huggingface.co/ayousanz/KoeAct-JP (Apache-2.0)
# and are checked against the SHA-256 the release publishes. Needs curl and esptool (PlatformIO's
# copy is used when esptool is not on PATH).
set -eu

REVISION=55ca64478dc146a2672e6fffe8f7799a4e85f41e
BASE="https://huggingface.co/ayousanz/KoeAct-JP/resolve/$REVISION/cores3"
MODEL_SHA256=dce593c8c9293c5565d1126e2f5b96a8e8151259044fc0d3c2f79a80d62eca92
A2_SHA256=61ee72e27af12907dd535908ee941b29fdee2bc3521e421705812adfa05ba4a6

here=$(cd "$(dirname "$0")" && pwd)
cache="${KOEACT_CACHE:-$here/../.pio/koeact/$REVISION}"
mkdir -p "$cache"

# sha256sum on macOS has no --status and does not read a check list from stdin, so compare by hand.
sha256() { if command -v sha256sum >/dev/null 2>&1; then sha256sum "$1"; else shasum -a 256 "$1"; fi | cut -d' ' -f1; }
fetch() { # name sha256
  if [ ! -f "$cache/$1" ] || [ "$(sha256 "$cache/$1")" != "$2" ]; then
    echo "downloading $1"
    curl -fL --progress-bar -o "$cache/$1.part" "$BASE/$1"
    [ "$(sha256 "$cache/$1.part")" = "$2" ] || { echo "SHA-256 mismatch: $1" >&2; exit 1; }
    mv "$cache/$1.part" "$cache/$1"
  fi
}
fetch ralomi_e13_stream40.espdl "$MODEL_SHA256"
fetch a2_weights.bin "$A2_SHA256"

# Offsets from the partition table, so it stays the one place they are written.
offset() { awk -F, -v n="$1" '$1 == n { gsub(/ /, "", $4); print $4 }' "$here/../partitions.csv"; }
MODEL_AT=$(offset model)
A2_AT=$(offset a2)
[ -n "$MODEL_AT" ] && [ -n "$A2_AT" ] || { echo "partitions.csv has no model / a2 partition" >&2; exit 1; }

if command -v esptool >/dev/null 2>&1; then esptool="esptool"
elif command -v esptool.py >/dev/null 2>&1; then esptool="esptool.py"
else esptool="python3 $HOME/.platformio/packages/tool-esptoolpy/esptool.py"
fi
port_args=""
[ $# -ge 1 ] && port_args="--port $1"

# shellcheck disable=SC2086
$esptool --chip esp32s3 $port_args write_flash \
  "$MODEL_AT" "$cache/ralomi_e13_stream40.espdl" \
  "$A2_AT" "$cache/a2_weights.bin"
echo "KoeAct-JP written: model at $MODEL_AT, a2 at $A2_AT"
