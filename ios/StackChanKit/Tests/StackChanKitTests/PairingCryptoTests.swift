import Foundation
import Testing
@testable import StackChanKit

#if canImport(CryptoKit)
import CryptoKit
#else
import Crypto
#endif

/// The same vectors the firmware's host test reads (m5/test/vectors/vectors.json, copied here).
struct Vectors: Decodable {
    let app_private: String
    let app_public: String
    let device_public: String
    let pairing_code: String
    let auth_key: String
    let ws_nonce: String
    let ws_mac: String

    static func load() throws -> Vectors {
        let url = try #require(Bundle.module.url(forResource: "vectors", withExtension: "json"))
        return try JSONDecoder().decode(Vectors.self, from: Data(contentsOf: url))
    }
}

func bytes(_ base64: String) -> Data { Data(base64Encoded: base64)! }

@Test func pairingCodeMatchesFirmware() throws {
    let v = try Vectors.load()
    let code = PairingCrypto.pairingCode(devicePublicKey: bytes(v.device_public), appPublicKey: bytes(v.app_public))
    #expect(code == v.pairing_code)
}

@Test func authKeyMatchesFirmware() throws {
    let v = try Vectors.load()
    let priv = try Curve25519.KeyAgreement.PrivateKey(rawRepresentation: bytes(v.app_private))
    #expect(priv.publicKey.rawRepresentation == bytes(v.app_public))
    let key = try PairingCrypto.authKey(privateKey: priv, devicePublicKey: bytes(v.device_public))
    #expect(key.withUnsafeBytes { Data($0) } == bytes(v.auth_key))
}

@Test func wsMacMatchesFirmware() throws {
    let v = try Vectors.load()
    let key = SymmetricKey(data: bytes(v.auth_key))
    #expect(PairingCrypto.wsMac(key: key, nonce: bytes(v.ws_nonce)) == bytes(v.ws_mac))
}
