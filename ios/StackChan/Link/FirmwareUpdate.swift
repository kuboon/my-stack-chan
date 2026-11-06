import CryptoKit
import Foundation
import Observation
import StackChanKit

/// Updating the StackChan's firmware over Wi-Fi: `ota.begin` → `ota.ready` → POST /ota →
/// `ota.result` (web/server/docs/protocol.md, "ファームウェアの更新").
@MainActor @Observable
final class FirmwareUpdate {
    enum State: Equatable {
        case idle
        case preparing
        case uploading(bytes: Int)
        /// Written; the StackChan restarts onto it.
        case restarting
        /// Back after the restart, with what `/info` says now.
        case done(firmware: String, built: String)
        case failed(String)
    }

    private(set) var state: State = .idle

    @ObservationIgnored private let link: DeviceLink
    @ObservationIgnored private var ready: CheckedContinuation<String?, Never>?
    @ObservationIgnored private var result: CheckedContinuation<String?, Never>?

    init(link: DeviceLink) {
        self.link = link
    }

    var busy: Bool {
        switch state {
        case .preparing, .uploading, .restarting: true
        default: false
        }
    }

    /// Sends the image in `file` (a PlatformIO `firmware.bin`).
    func install(from file: URL) async {
        let scoped = file.startAccessingSecurityScopedResource()
        defer { if scoped { file.stopAccessingSecurityScopedResource() } }
        guard let image = try? Data(contentsOf: file) else {
            state = .failed("ファイルを読めませんでした。")
            return
        }
        await install(image)
    }

    func install(_ image: Data) async {
        guard link.isConnected else {
            state = .failed("先にスタックちゃんにつないでください。")
            return
        }
        // An ESP32 app image starts with 0xE9; anything else is not a firmware.bin.
        guard image.first == 0xE9 else {
            state = .failed("ESP32 のファームウェア (firmware.bin) ではないようです。")
            return
        }
        if let max = link.deviceInfo?.otaMaxSize {
            guard image.count <= max else {
                state = .failed("ファームウェアが大きすぎます (\(image.count) バイト、上限 \(max))。")
                return
            }
        } else if link.deviceInfo != nil {
            state = .failed("このスタックちゃんのファームウェアは Wi-Fi での更新に対応していません。一度 USB で書き込んでください。")
            return
        }

        state = .preparing
        let sha256 = SHA256.hash(data: image).map { String(format: "%02x", $0) }.joined()
        let token: String? = await withCheckedContinuation { continuation in
            ready = continuation
            link.send(.otaBegin(size: image.count, sha256: sha256))
            Task {
                try? await Task.sleep(for: .seconds(10))
                self.resumeReady(nil)
            }
        }
        guard let token else {
            if case .preparing = state { state = .failed("スタックちゃんが更新を受け付けませんでした。") }
            return
        }

        state = .uploading(bytes: image.count)
        let outcome: String? = await withCheckedContinuation { continuation in
            result = continuation
            Task {
                do {
                    // The HTTP answer settles it; ota.result over the WebSocket says the same.
                    let status = try await link.uploadFirmware(image, token: token)
                    self.resumeResult(status == 200 ? nil : "送った内容を受け付けませんでした (HTTP \(status))。")
                } catch {
                    self.resumeResult("送れませんでした: \(error.localizedDescription)")
                }
            }
        }
        if let outcome {
            state = .failed(outcome)
            return
        }

        state = .restarting
        await waitForRestart()
    }

    func handle(_ message: DeviceMessage) {
        switch message {
        case .otaReady(let token):
            resumeReady(token)
        case .otaResult(let ok, let error):
            if ok {
                resumeResult(nil)
            } else {
                let why = Self.describe(error)
                if ready != nil {
                    state = .failed(why)
                    resumeReady(nil)
                } else {
                    resumeResult(why)
                }
            }
        default:
            break
        }
    }

    // MARK: -

    private func resumeReady(_ token: String?) {
        ready?.resume(returning: token)
        ready = nil
    }

    /// nil: written; otherwise why not.
    private func resumeResult(_ error: String?) {
        result?.resume(returning: error)
        result = nil
    }

    /// The StackChan restarts a moment after `ota.result`; reconnect and read `/info` again.
    private func waitForRestart() async {
        try? await Task.sleep(for: .seconds(8))
        for _ in 0..<12 {
            await link.connect()
            if link.isConnected, let info = link.deviceInfo {
                state = .done(firmware: info.firmware, built: info.built ?? "")
                return
            }
            try? await Task.sleep(for: .seconds(5))
        }
        state = .failed("再起動したスタックちゃんにつながりません。スタックちゃんの画面を確かめてください。")
    }

    private static func describe(_ error: String?) -> String {
        switch error {
        case "too_large": "ファームウェアが大きすぎます。"
        case "busy": "スタックちゃんは別の更新を受け取っています。"
        case "sha256", "size": "届いたファームウェアが壊れていました。もう一度試してください。"
        case "image": "スタックちゃんがファームウェアを受け付けませんでした (ESP32-S3 用ではない?)。"
        default: "更新に失敗しました (\(error ?? "不明"))。"
        }
    }
}
