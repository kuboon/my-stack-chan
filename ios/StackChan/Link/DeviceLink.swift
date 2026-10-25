import CryptoKit
import Foundation
import Observation
import StackChanKit

/// The connection to one StackChan in direct mode: finding it, pairing over HTTP, then the
/// authenticated WebSocket (web/server/docs/protocol.md). The StackChan is either its own AP or
/// on the same home Wi-Fi as the iPad.
@MainActor @Observable
final class DeviceLink {
    enum Status: Equatable {
        case disconnected
        case connecting
        /// Waiting for the touch on the StackChan that approves this code.
        case pairing(code: String)
        case connected(name: String)
        case failed(String)
    }

    enum LinkError: LocalizedError {
        case badResponse
        case pairedWithAnother
        case pairingRejected
        case authFailed
        case notFound

        var errorDescription: String? {
            switch self {
            case .badResponse: "スタックちゃんから思わぬ返事が来ました。"
            case .pairedWithAnother: "スタックちゃんは別のアプリとペアリング済みです。スタックちゃんの画面を 10 秒長押しして解除してください。"
            case .pairingRejected: "ペアリングが承認されませんでした。"
            case .authFailed: "認証に失敗しました。もう一度つないでください。"
            case .notFound: "スタックちゃんが見つかりません。iPad をスタックちゃんの Wi-Fi か、スタックちゃんと同じ Wi-Fi につないでください。"
            }
        }
    }

    var status: Status = .disconnected
    /// The StackChan's address as the user typed it. Empty: find it (Bonjour, then the AP's
    /// own address).
    var host = ""
    /// Where the StackChan was found, and on which Wi-Fi it is: "ap" or "sta".
    private(set) var activeHost = ""
    private(set) var deviceWifi: String?

    @ObservationIgnored var onMessage: (@MainActor (DeviceMessage) -> Void)?
    @ObservationIgnored var onAudio: (@MainActor (Data) -> Void)?

    @ObservationIgnored private let session = URLSession(configuration: .ephemeral)
    @ObservationIgnored private var socket: URLSessionWebSocketTask?
    @ObservationIgnored private var outbox: AsyncStream<URLSessionWebSocketTask.Message>.Continuation?
    @ObservationIgnored private var deviceName: String?
    @ObservationIgnored private var authKey: SymmetricKey?

    /// The StackChan's own address while it is the AP.
    static let apHost = "192.168.4.1"

    var isConnected: Bool {
        if case .connected = status { true } else { false }
    }

    func connect() async {
        disconnect()
        status = .connecting
        do {
            let info = try await locate()
            deviceWifi = info.wifi ?? "ap"
            var key = KeyStore.key(for: info.name)
            if key != nil, !info.paired {
                // The StackChan forgot the pairing; so do we.
                KeyStore.delete(for: info.name)
                key = nil
            }
            if key == nil {
                if info.paired { throw LinkError.pairedWithAnother }
                key = try await pair(device: info.name)
            }
            try await openSocket(key: SymmetricKey(data: key!), name: info.name)
        } catch {
            status = .failed(error.localizedDescription)
        }
    }

    func disconnect() {
        outbox?.finish()
        outbox = nil
        socket?.cancel(with: .goingAway, reason: nil)
        socket = nil
        authKey = nil
        status = .disconnected
    }

    /// Forgets this iPad's key. The StackChan must be unpaired on its side too.
    func forgetPairing() {
        if let deviceName { KeyStore.delete(for: deviceName) }
        disconnect()
    }

    /// Has the StackChan join a home Wi-Fi; it answers with `wifi.result`.
    func joinWifi(ssid: String, password: String) throws {
        guard let authKey else { throw LinkError.authFailed }
        let sealed = try PairingCrypto.sealWifiPassword(password, ssid: ssid, authKey: authKey)
        send(.wifiConnect(ssid: ssid, nonce: sealed.nonce, password: sealed.sealed))
    }

    func send(_ message: AppMessage) {
        outbox?.yield(.string(message.json))
    }

    func sendAudio(_ pcm: Data) {
        guard !pcm.isEmpty else { return }
        outbox?.yield(.data(pcm))
    }

    // MARK: - HTTP

    private func url(_ path: String, scheme: String = "http", host: String? = nil) -> URL {
        URL(string: "\(scheme)://\(host ?? activeHost)\(path)")!
    }

    /// The StackChan's `/info`, from the typed address, or else the first that answers among those
    /// Bonjour finds and the AP's address.
    private func locate() async throws -> DeviceInfo {
        let typed = host.trimmingCharacters(in: .whitespaces)
        let candidates = typed.isEmpty ? await DeviceFinder.find() + [Self.apHost] : [typed]
        for candidate in candidates {
            if let info = try? await fetchInfo(host: candidate) {
                activeHost = candidate
                return info
            }
        }
        throw LinkError.notFound
    }

    private func fetchInfo(host: String) async throws -> DeviceInfo {
        var request = URLRequest(url: url("/info", host: host))
        request.timeoutInterval = 3
        let (data, response) = try await session.data(for: request)
        guard (response as? HTTPURLResponse)?.statusCode == 200 else { throw LinkError.badResponse }
        return try JSONDecoder().decode(DeviceInfo.self, from: data)
    }

    private func pair(device: String) async throws -> Data {
        let privateKey = Curve25519.KeyAgreement.PrivateKey()
        var request = URLRequest(url: url("/pair/start"))
        request.httpMethod = "POST"
        request.setValue("application/json", forHTTPHeaderField: "Content-Type")
        request.httpBody = try JSONSerialization.data(withJSONObject: [
            "public_key": privateKey.publicKey.rawRepresentation.base64EncodedString(),
        ])
        let (data, response) = try await session.data(for: request)
        let code = (response as? HTTPURLResponse)?.statusCode
        if code == 409 { throw LinkError.pairedWithAnother }
        guard code == 200,
              let object = try JSONSerialization.jsonObject(with: data) as? [String: Any],
              let devicePublicKey = (object["public_key"] as? String).flatMap({ Data(base64Encoded: $0) }),
              devicePublicKey.count == 32
        else { throw LinkError.badResponse }

        status = .pairing(code: PairingCrypto.pairingCode(
            devicePublicKey: devicePublicKey,
            appPublicKey: privateKey.publicKey.rawRepresentation
        ))

        for _ in 0..<75 {
            try await Task.sleep(for: .seconds(1))
            let (data, _) = try await session.data(from: url("/pair/status"))
            let state = (try? JSONSerialization.jsonObject(with: data) as? [String: Any])?["state"] as? String
            switch state {
            case "approved":
                let key = try PairingCrypto.authKey(privateKey: privateKey, devicePublicKey: devicePublicKey)
                let raw = key.withUnsafeBytes { Data($0) }
                KeyStore.save(raw, for: device)
                return raw
            case "waiting":
                continue
            default:
                throw LinkError.pairingRejected
            }
        }
        throw LinkError.pairingRejected
    }

    // MARK: - WebSocket

    private func openSocket(key: SymmetricKey, name: String) async throws {
        let socket = session.webSocketTask(with: url("/ws", scheme: "ws"))
        self.socket = socket
        socket.resume()

        guard case .string(let helloText) = try await socket.receive(),
              case .hello(let nonce)? = DeviceMessage(json: helloText)
        else { throw LinkError.badResponse }
        try await socket.send(.string(AppMessage.auth(mac: PairingCrypto.wsMac(key: key, nonce: nonce)).json))
        guard case .string(let readyText) = try await socket.receive(),
              case .ready? = DeviceMessage(json: readyText)
        else { throw LinkError.authFailed }

        deviceName = name
        authKey = key
        status = .connected(name: name)

        // One task sends, so that messages and audio keep their order.
        let (stream, continuation) = AsyncStream<URLSessionWebSocketTask.Message>.makeStream()
        outbox = continuation
        Task {
            for await message in stream {
                try? await socket.send(message)
            }
        }
        Task { await receiveLoop(socket) }
    }

    private func receiveLoop(_ socket: URLSessionWebSocketTask) async {
        while true {
            do {
                switch try await socket.receive() {
                case .string(let text):
                    if let message = DeviceMessage(json: text) { onMessage?(message) }
                case .data(let data):
                    onAudio?(data)
                @unknown default:
                    break
                }
            } catch {
                if self.socket === socket {
                    outbox?.finish()
                    outbox = nil
                    self.socket = nil
                    status = .disconnected
                }
                return
            }
        }
    }
}
