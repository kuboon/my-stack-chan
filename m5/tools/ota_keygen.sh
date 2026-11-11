#!/bin/sh
# Makes the key that signs OTA images (once), and writes its public half into src/ota_pubkey.h.
#
#   m5/tools/ota_keygen.sh
#
# The private key stays on this machine: $STACKCHAN_OTA_KEY, or ~/.config/stackchan/ota_signing.pem.
# Never commit it. The firmware built after this accepts images signed with it (ota_push.sh), and
# no others. Run it again with the key in place to rewrite ota_pubkey.h from it. A new key
# means a new firmware has to go in over USB once (or by the app's OTA) before it takes effect.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
key=${STACKCHAN_OTA_KEY:-$HOME/.config/stackchan/ota_signing.pem}

if [ -e "$key" ]; then
    echo "The key is there already, and stays: $key"
else
    mkdir -p "$(dirname "$key")"
    (umask 077 && openssl ecparam -name prime256v1 -genkey -noout -out "$key")
    echo "Made the signing key: $key (back it up; do not commit it)"
fi

bytes=$(openssl ec -in "$key" -pubout -outform DER 2>/dev/null | xxd -p -c 1000 | sed 's/../0x&, /g')
count=$(printf '%s' "$bytes" | tr ',' '\n' | grep -c 0x)
cat > "$here/../src/ota_pubkey.h" <<HEADER
// The public key that checks the signature of an OTA image (POST /ota/signed).
// Written by m5/tools/ota_keygen.sh; empty (signed updates off) until it has run.
// This is a public key: it can be committed. The private key must not be.
#pragma once

#include <cstddef>
#include <cstdint>

namespace ota {

constexpr uint8_t kPublicKey[] = {$bytes};
constexpr size_t kPublicKeyLength = $count;

}  // namespace ota
HEADER
echo "Wrote src/ota_pubkey.h ($count bytes). Build and flash it over USB once:"
echo "  cd m5 && pio run -t upload"
