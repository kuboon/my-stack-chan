import Foundation
import Security

/// The keys shared with paired StackChans, in the Keychain, one per device name.
enum KeyStore {
    private static let service = "one.kbn.stackchan.pairing"

    static func key(for device: String) -> Data? {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: device,
            kSecReturnData as String: true,
            kSecMatchLimit as String: kSecMatchLimitOne,
        ]
        var item: CFTypeRef?
        guard SecItemCopyMatching(query as CFDictionary, &item) == errSecSuccess else { return nil }
        return item as? Data
    }

    static func save(_ key: Data, for device: String) {
        delete(for: device)
        let item: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: device,
            kSecAttrAccessible as String: kSecAttrAccessibleAfterFirstUnlockThisDeviceOnly,
            kSecValueData as String: key,
        ]
        SecItemAdd(item as CFDictionary, nil)
    }

    static func delete(for device: String) {
        let query: [String: Any] = [
            kSecClass as String: kSecClassGenericPassword,
            kSecAttrService as String: service,
            kSecAttrAccount as String: device,
        ]
        SecItemDelete(query as CFDictionary)
    }
}
