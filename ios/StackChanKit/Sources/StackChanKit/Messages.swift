import Foundation

/// A text message from the device (web/server/docs/protocol.md).
public enum DeviceMessage: Equatable, Sendable {
    case hello(nonce: Data)
    case ready(name: String)
    case listenStart
    case listenEnd
    case speakDone
    case touch(gesture: String)
    /// A photo the camera took for `photo.take`, as JPEG, with where the time went.
    case photo(jpeg: Data, width: Int, height: Int, timing: PhotoTiming)
    /// `photo.take` could not be answered: "busy" or "camera".
    case photoFailed(reason: String)
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
        case "photo":
            if let jpeg = (object["jpeg"] as? String).flatMap({ Data(base64Encoded: $0) }) {
                let rawMs = object["ms"] as? [String: Any] ?? [:]
                let ms = rawMs.compactMapValues { $0 as? Int }
                self = .photo(
                    jpeg: jpeg,
                    width: object["width"] as? Int ?? 0,
                    height: object["height"] as? Int ?? 0,
                    timing: PhotoTiming(
                        initMs: ms["init"] ?? 0,
                        warmupMs: ms["warmup"] ?? 0,
                        shotMs: ms["shot"] ?? 0,
                        jpegMs: ms["jpeg"] ?? 0,
                        frameMs: rawMs["frames"] as? [Int] ?? [],
                        missed: object["missed"] as? Int ?? 0
                    )
                )
            } else {
                self = .photoFailed(reason: object["error"] as? String ?? "camera")
            }
        default: self = .unknown(type: type)
        }
    }
}

/// Where a photo's time went on the device, in ms, and the frames the camera failed to deliver.
public struct PhotoTiming: Equatable, Sendable {
    public var initMs = 0
    public var warmupMs = 0
    public var shotMs = 0
    public var jpegMs = 0
    /// How long each frame took to arrive: the ones dropped while the exposure settled, then the
    /// one kept.
    public var frameMs: [Int] = []
    public var missed = 0

    public init(
        initMs: Int = 0, warmupMs: Int = 0, shotMs: Int = 0, jpegMs: Int = 0, frameMs: [Int] = [], missed: Int = 0
    ) {
        self.initMs = initMs
        self.warmupMs = warmupMs
        self.shotMs = shotMs
        self.jpegMs = jpegMs
        self.frameMs = frameMs
        self.missed = missed
    }
}

/// The sizes `photo.take` can ask for.
public enum PhotoSize: String, CaseIterable, Sendable {
    /// 640×480
    case vga
    /// 320×240
    case qvga
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
    case photoTake(size: PhotoSize)

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
        case .photoTake(let size): ["type": "photo.take", "size": size.rawValue]
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
