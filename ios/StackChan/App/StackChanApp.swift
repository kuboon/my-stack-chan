import SwiftUI

@main
struct StackChanApp: App {
    @State private var link: DeviceLink
    @State private var conversation: Conversation
    @State private var camera: RemoteCamera
    @State private var wifi: WifiSetup
    @State private var firmware: FirmwareUpdate

    init() {
        let link = DeviceLink()
        let camera = RemoteCamera(link: link)
        let conversation = Conversation(link: link, body: DeviceBody(link: link, camera: camera))
        let wifi = WifiSetup(link: link)
        let firmware = FirmwareUpdate(link: link)
        link.onMessage = { [weak conversation, weak camera, weak wifi, weak firmware] in
            conversation?.handle($0)
            camera?.handle($0)
            wifi?.handle($0)
            firmware?.handle($0)
        }
        link.onAudio = { [weak conversation] in conversation?.handleAudio($0) }
        _link = State(initialValue: link)
        _conversation = State(initialValue: conversation)
        _camera = State(initialValue: camera)
        _wifi = State(initialValue: wifi)
        _firmware = State(initialValue: firmware)
    }

    var body: some Scene {
        WindowGroup {
            ContentView(link: link, conversation: conversation, camera: camera, wifi: wifi, firmware: firmware)
        }
    }
}
