import StackChanKit
import UIKit

/// StackChanBody on the StackChan in direct mode: the camera and the head over the link.
@MainActor
final class DeviceBody: StackChanBody {
    /// A photo the model took, for the conversation to show.
    var onPhoto: (@MainActor (UIImage) -> Void)?
    /// What the model asked the body to do, for the conversation's notes.
    var onNote: (@MainActor (String) -> Void)?

    private let link: DeviceLink
    private let camera: RemoteCamera

    init(link: DeviceLink, camera: RemoteCamera) {
        self.link = link
        self.camera = camera
    }

    func takePhoto() async -> Bool {
        onNote?("道具: 写真を撮る")
        guard let photo = await camera.capture() else {
            onNote?("写真を撮れませんでした: \(camera.error ?? "つながっていません")")
            return false
        }
        onPhoto?(photo)
        return true
    }

    func look(_ direction: LookDirection) {
        onNote?("道具: \(direction.rawValue) を向く")
        link.send(.look(yaw: direction.yaw, pitch: direction.pitch))
    }
}
