import SwiftUI

@main
struct StackChanApp: App {
    @State private var link: DeviceLink
    @State private var conversation: Conversation
    @State private var camera: RemoteCamera

    init() {
        let link = DeviceLink()
        let conversation = Conversation(link: link)
        let camera = RemoteCamera(link: link)
        link.onMessage = { [weak conversation, weak camera] in
            conversation?.handle($0)
            camera?.handle($0)
        }
        link.onAudio = { [weak conversation] in conversation?.handleAudio($0) }
        _link = State(initialValue: link)
        _conversation = State(initialValue: conversation)
        _camera = State(initialValue: camera)
    }

    var body: some Scene {
        WindowGroup {
            ContentView(link: link, conversation: conversation, camera: camera)
        }
    }
}
