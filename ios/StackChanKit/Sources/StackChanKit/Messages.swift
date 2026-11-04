import Foundation

/// What the StackChan heard in a recording with its own spoken-command model (KoeAct-JP), sent
/// with `listen.end`. When `accepted`, the StackChan has already done `actions`, and the app does
/// not answer the recording.
public struct HeardCommand: Equatable, Sendable {
    /// The Action JSON: "[]" or an array of calls such as `{"name":"nod","arguments":{"count":2}}`.
    public var actions: String
    /// The names of the calls, in order.
    public var names: [String]
    public var accepted: Bool
    public var pAct: Double
    /// Ralomi's kana reading of the recording. Rough: no kanji.
    public var kana: String

    public init(actions: String, names: [String], accepted: Bool, pAct: Double, kana: String) {
        self.actions = actions
        self.names = names
        self.accepted = accepted
        self.pAct = pAct
        self.kana = kana
    }

    init?(listenEnd object: [String: Any]) {
        guard let calls = object["actions"] as? [[String: Any]] else { return nil }
        let data = (try? JSONSerialization.data(withJSONObject: calls, options: [.sortedKeys])) ?? Data("[]".utf8)
        self.init(
            actions: String(decoding: data, as: UTF8.self),
            names: calls.compactMap { $0["name"] as? String },
            accepted: object["accepted"] as? Bool ?? false,
            pAct: object["p_act"] as? Double ?? 0,
            kana: object["kana"] as? String ?? ""
        )
    }
}

/// A text message from the device (web/server/docs/protocol.md).
public enum DeviceMessage: Equatable, Sendable {
    case hello(nonce: Data)
    case ready(name: String)
    case listenStart
    /// The recording is over. `heard` is what the StackChan made of it itself (KoeAct-JP), when
    /// it has the model.
    case listenEnd(heard: HeardCommand?)
    case speakDone
    case touch(gesture: String)
    /// A photo the camera took for `photo.take`, as JPEG, with where the time went.
    case photo(jpeg: Data, width: Int, height: Int, timing: PhotoTiming)
    /// `photo.take` could not be answered: "busy" or "camera".
    case photoFailed(reason: String)
    /// The answer to `wifi.scan`, strongest first.
    case wifiNetworks([WifiNetwork])
    /// The answer to `wifi.connect`: the address on success, or the error.
    case wifiJoined(ssid: String, ip: String)
    case wifiFailed(error: String)
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
        case "listen.end": self = .listenEnd(heard: HeardCommand(listenEnd: object))
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
        case "wifi.networks":
            let list = object["networks"] as? [[String: Any]] ?? []
            self = .wifiNetworks(list.compactMap { n in
                guard let ssid = n["ssid"] as? String else { return nil }
                return WifiNetwork(ssid: ssid, rssi: n["rssi"] as? Int ?? 0, secure: n["secure"] as? Bool ?? true)
            })
        case "wifi.result":
            if object["ok"] as? Bool == true {
                self = .wifiJoined(ssid: object["ssid"] as? String ?? "", ip: object["ip"] as? String ?? "")
            } else {
                self = .wifiFailed(error: object["error"] as? String ?? "")
            }
        default: self = .unknown(type: type)
        }
    }
}

/// A Wi-Fi network the device sees.
public struct WifiNetwork: Equatable, Sendable, Identifiable {
    public var ssid: String
    public var rssi: Int
    public var secure: Bool
    public var id: String { ssid }

    public init(ssid: String, rssi: Int, secure: Bool) {
        self.ssid = ssid
        self.rssi = rssi
        self.secure = secure
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

/// Where to turn the head, in the words people use. The values are `look`'s.
public enum LookDirection: String, CaseIterable, Sendable {
    case front, right, left, up, down

    /// -1.0 to 1.0, positive to StackChan's right.
    public var yaw: Double {
        switch self {
        case .right: 0.6
        case .left: -0.6
        case .front, .up, .down: 0
        }
    }

    /// 0.0 to 1.0, higher looks up.
    public var pitch: Double {
        switch self {
        case .up: 0.9
        case .down: 0
        case .front, .right, .left: 0.3
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
    case photoTake(size: PhotoSize)
    case wifiScan
    /// `password` is sealed (PairingCrypto.sealWifiPassword).
    case wifiConnect(ssid: String, nonce: Data, password: Data)
    case wifiForget

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
        case .wifiScan: ["type": "wifi.scan"]
        case .wifiConnect(let ssid, let nonce, let password):
            ["type": "wifi.connect", "ssid": ssid, "nonce": nonce.base64EncodedString(),
             "password": password.base64EncodedString()]
        case .wifiForget: ["type": "wifi.forget"]
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
    /// "ap" or "sta". Missing from older firmware, which is always "ap".
    public var wifi: String?
}
