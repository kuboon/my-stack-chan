import Foundation

#if canImport(CryptoKit)
import CryptoKit
#else
import Crypto
#endif

/// Pairing and WebSocket authentication, as web/server/docs/protocol.md specifies them.
/// The firmware's m5/src/pairing_crypto.cpp is the other half; both are tested against
/// m5/test/vectors/vectors.json.
public enum PairingCrypto {
    /// The 6-digit code both screens show.
    public static func pairingCode(devicePublicKey: Data, appPublicKey: Data) -> String {
        var input = Data("stackchan-pair-v1".utf8)
        input.append(devicePublicKey)
        input.append(appPublicKey)
        let digest = Array(SHA256.hash(data: input))
        let n = UInt32(digest[0]) << 24 | UInt32(digest[1]) << 16 | UInt32(digest[2]) << 8 | UInt32(digest[3])
        return String(format: "%06u", n % 1_000_000)
    }

    /// The key shared with the device: HKDF-SHA256 over the X25519 secret.
    public static func authKey(
        privateKey: Curve25519.KeyAgreement.PrivateKey,
        devicePublicKey: Data
    ) throws -> SymmetricKey {
        let peer = try Curve25519.KeyAgreement.PublicKey(rawRepresentation: devicePublicKey)
        let shared = try privateKey.sharedSecretFromKeyAgreement(with: peer)
        var salt = privateKey.publicKey.rawRepresentation
        salt.append(devicePublicKey)
        return shared.hkdfDerivedSymmetricKey(
            using: SHA256.self,
            salt: salt,
            sharedInfo: Data("stackchan-v1-auth".utf8),
            outputByteCount: 32
        )
    }

    /// The key that seals the Wi-Fi password of `wifi.connect`: HKDF-SHA256 over the shared key.
    public static func wifiKey(authKey: SymmetricKey) -> SymmetricKey {
        HKDF<SHA256>.deriveKey(
            inputKeyMaterial: authKey,
            salt: Data(),
            info: Data("stackchan-v1-wifi".utf8),
            outputByteCount: 32
        )
    }

    /// `wifi.connect`'s password: AES-256-GCM with the SSID as AAD, the tag after the ciphertext.
    public static func sealWifiPassword(
        _ password: String,
        ssid: String,
        authKey: SymmetricKey,
        nonce: AES.GCM.Nonce = AES.GCM.Nonce()
    ) throws -> (nonce: Data, sealed: Data) {
        let box = try AES.GCM.seal(
            Data(password.utf8),
            using: wifiKey(authKey: authKey),
            nonce: nonce,
            authenticating: Data(ssid.utf8)
        )
        return (Data(nonce), box.ciphertext + box.tag)
    }

    /// What the app answers the device's `hello` with.
    public static func wsMac(key: SymmetricKey, nonce: Data) -> Data {
        var input = Data("stackchan-ws-v1".utf8)
        input.append(nonce)
        return Data(HMAC<SHA256>.authenticationCode(for: input, using: key))
    }
}
