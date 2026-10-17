import Foundation

/// A text message from the device (web/server/docs/protocol.md).
public enum DeviceMessage: Equatable, Sendable {
    case hello(nonce: Data)
    case ready(name: String)
    case listenStart
    case listenEnd
    case speakDone
    case touch(gesture: String)
    case unknown(type: String)

    public init?(json: String) {
        guard let data = json.data(using: .utf8),
              let object = try? JSONSerialization.jsonObject(with: data) as? [String: Any],
              let type = object["type"] as? String
        else { return nil }
        switch type {
        case "hello":
            guard let nonce = (object["nonce"] as? String).flatMap({ Data(base64Encoded: $0) }) else { return nil }
            self = .hello(nonce: nonce)
        case "ready": self = .ready(name: object["name"] as? String ?? "")
        case "listen.start": self = .listenStart
        case "listen.end": self = .listenEnd
        case "speak.done": self = .speakDone
        case "touch": self = .touch(gesture: object["gesture"] as? String ?? "tap")
        default: self = .unknown(type: type)
        }
    }
}

/// The expressions the avatar has.
public enum FaceExpression: String, CaseIterable, Sendable {
    case neutral, happy, sad, angry, doubt, sleepy
}

/// A text message to the device.
public enum AppMessage: Equatable, Sendable {
    case auth(mac: Data)
    case listenStart
    case listenEnd
    case speakStart(text: String)
    case speakEnd
    case face(FaceExpression)
    case look(yaw: Double, pitch: Double)
    case state(thinking: Bool)

    public var json: String {
        let object: [String: Any] = switch self {
        case .auth(let mac): ["type": "auth", "mac": mac.base64EncodedString()]
        case .listenStart: ["type": "listen.start"]
        case .listenEnd: ["type": "listen.end"]
        case .speakStart(let text): ["type": "speak.start", "text": text]
        case .speakEnd: ["type": "speak.end"]
        case .face(let e): ["type": "face", "expression": e.rawValue]
        case .look(let yaw, let pitch): ["type": "look", "yaw": yaw, "pitch": pitch]
        case .state(let thinking): ["type": "state", "state": thinking ? "thinking" : "idle"]
        }
        let data = try! JSONSerialization.data(withJSONObject: object, options: [.sortedKeys])
        return String(decoding: data, as: UTF8.self)
    }
}

/// `GET /info`.
public struct DeviceInfo: Codable, Equatable, Sendable {
    public var name: String
    public var model: String
    public var firmware: String
    public var paired: Bool
}
