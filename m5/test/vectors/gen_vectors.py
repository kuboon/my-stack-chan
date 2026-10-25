"""Generates the pairing test vectors (vectors.json) shared by the firmware and app tests.

Keys are the X25519 test vectors of RFC 7748 section 6.1. Run: python3 gen_vectors.py > vectors.json
"""

import base64
import hashlib
import hmac
import json

from cryptography.hazmat.primitives.asymmetric.x25519 import X25519PrivateKey, X25519PublicKey
from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.ciphers.aead import AESGCM

APP_PRIV = bytes.fromhex("77076d0a7318a57d3c16c17251b26645df4c2f87ebc0992ab177fba51db92c2a")
DEVICE_PRIV = bytes.fromhex("5dab087e624a8a4b79e17f8b83800ee66f3bb1292618b6fd1c2f8b27ff88e0eb")
NONCE = bytes(range(16))
WIFI_SSID = "おうちの Wi-Fi"
WIFI_PASSWORD = "p@ss word 123"
WIFI_NONCE = bytes(range(12))


def raw_pub(priv: X25519PrivateKey) -> bytes:
    return priv.public_key().public_bytes(serialization.Encoding.Raw, serialization.PublicFormat.Raw)


def hkdf_sha256(ikm: bytes, salt: bytes, info: bytes) -> bytes:
    prk = hmac.new(salt, ikm, hashlib.sha256).digest()
    return hmac.new(prk, info + b"\x01", hashlib.sha256).digest()


app = X25519PrivateKey.from_private_bytes(APP_PRIV)
device = X25519PrivateKey.from_private_bytes(DEVICE_PRIV)
app_pub, device_pub = raw_pub(app), raw_pub(device)
shared = app.exchange(X25519PublicKey.from_public_bytes(device_pub))
assert shared == device.exchange(X25519PublicKey.from_public_bytes(app_pub))

digest = hashlib.sha256(b"stackchan-pair-v1" + device_pub + app_pub).digest()
code = f"{int.from_bytes(digest[:4], 'big') % 1_000_000:06d}"
key = hkdf_sha256(shared, app_pub + device_pub, b"stackchan-v1-auth")
mac = hmac.new(key, b"stackchan-ws-v1" + NONCE, hashlib.sha256).digest()
wifi_key = hkdf_sha256(key, b"", b"stackchan-v1-wifi")
wifi_sealed = AESGCM(wifi_key).encrypt(WIFI_NONCE, WIFI_PASSWORD.encode(), WIFI_SSID.encode())

b64 = lambda b: base64.b64encode(b).decode()
print(json.dumps({
    "app_private": b64(APP_PRIV),
    "app_public": b64(app_pub),
    "device_private": b64(DEVICE_PRIV),
    "device_public": b64(device_pub),
    "shared_secret": b64(shared),
    "pairing_code": code,
    "auth_key": b64(key),
    "ws_nonce": b64(NONCE),
    "ws_mac": b64(mac),
    "wifi_ssid": WIFI_SSID,
    "wifi_password": WIFI_PASSWORD,
    "wifi_key": b64(wifi_key),
    "wifi_nonce": b64(WIFI_NONCE),
    "wifi_sealed": b64(wifi_sealed),
}, indent=2, ensure_ascii=False))
