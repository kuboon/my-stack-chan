import Observation
import StackChanKit
import UIKit

/// The StackChan's camera, seen from the app: `photo.take`, answered by `photo`
/// (web/server/docs/protocol.md, "写真").
@MainActor @Observable
final class RemoteCamera {
    private(set) var photo: UIImage?
    private(set) var taking = false
    private(set) var error: String?
    /// How the last photo went: the time on the device, and the round trip seen from here.
    private(set) var timing: String?
    var size: PhotoSize = .vga

    @ObservationIgnored private let link: DeviceLink
    @ObservationIgnored private var timeout: Task<Void, Never>?
    @ObservationIgnored private var started = ContinuousClock.now

    init(link: DeviceLink) {
        self.link = link
    }

    func take() {
        guard link.isConnected, !taking else { return }
        taking = true
        error = nil
        started = .now
        link.send(.photoTake(size: size))
        timeout = Task { [weak self] in
            // Each frame the camera misses costs the device 4 seconds.
            try? await Task.sleep(for: .seconds(30))
            guard !Task.isCancelled, let self, self.taking else { return }
            self.finish(error: "スタックちゃんから写真が届きませんでした。")
        }
    }

    func handle(_ message: DeviceMessage) {
        switch message {
        case .photo(let jpeg, _, _, let t):
            guard let image = UIImage(data: jpeg) else {
                finish(error: "写真を読めませんでした。")
                return
            }
            photo = image
            let total = (ContinuousClock.now - started).components
            let totalMs = total.seconds * 1000 + total.attoseconds / 1_000_000_000_000_000
            timing = "全体 \(totalMs) ms (起動 \(t.initMs)・待ち \(t.warmupMs)・撮影 \(t.shotMs)・JPEG \(t.jpegMs))"
                + (t.missed > 0 ? "、届かなかったフレーム \(t.missed)" : "")
                + "\nフレームごと: \(t.frameMs.map(String.init).joined(separator: ", ")) ms"
                + "、\(jpeg.count / 1024) KB"
            finish(error: nil)
        case .photoFailed(let reason):
            finish(error: reason == "busy"
                ? "スタックちゃんが聞いているかしゃべっているので、撮れませんでした。"
                : "スタックちゃんのカメラで撮れませんでした。")
        default:
            break
        }
    }

    private func finish(error: String?) {
        timeout?.cancel()
        timeout = nil
        taking = false
        self.error = error
    }
}
