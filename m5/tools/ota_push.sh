#!/bin/sh
# Signs firmware.bin and sends it to the StackChan over Wi-Fi (POST /ota/signed).
#
#   m5/tools/ota_push.sh stackchan-ab12.local [path/to/firmware.bin]
#
# The StackChan must run a firmware that has the public key (ota_keygen.sh, then one USB flash).
# The key is $STACKCHAN_OTA_KEY, or ~/.config/stackchan/ota_signing.pem.
set -eu
here=$(cd "$(dirname "$0")" && pwd)
host=${1:?usage: ota_push.sh <host> [firmware.bin]}
bin=${2:-$here/../.pio/build/stackchan/firmware.bin}
key=${STACKCHAN_OTA_KEY:-$HOME/.config/stackchan/ota_signing.pem}
[ -f "$bin" ] || { echo "no image: $bin (pio run)" >&2; exit 1; }
[ -f "$key" ] || { echo "no signing key: $key (ota_keygen.sh)" >&2; exit 1; }

info=$(curl -fsS --max-time 10 "http://$host/info")
case "$info" in
    *'"ota_signed":true'*) ;;
    *) echo "This firmware does not take signed updates. Flash one with the public key over USB first." >&2
       echo "$info" >&2; exit 1 ;;
esac

sig=$(openssl dgst -sha256 -sign "$key" "$bin" | xxd -p -c 1000 | tr -d '\n')
echo "Sending $(wc -c < "$bin" | tr -d ' ') bytes to $host ..."
# Expect: is off because the StackChan's web server does not answer 100-continue.
reply=$(curl -sS --max-time 180 -X POST -H 'Expect:' -H 'Content-Type: application/octet-stream' \
    --data-binary @"$bin" -w '\n%{http_code}' "http://$host/ota/signed?sig=$sig")
echo "$reply"
case "$reply" in
    *'"ok":true'*) ;;
    *) exit 1 ;;
esac

echo "Waiting for it to restart ..."
sleep 5
for _ in 1 2 3 4 5 6 7 8 9 10 11 12; do
    if now=$(curl -fsS --max-time 3 "http://$host/info" 2>/dev/null); then
        echo "$now"
        exit 0
    fi
    sleep 3
done
echo "It did not answer again; look at it." >&2
exit 1
