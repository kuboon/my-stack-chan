import SwiftUI

@main
struct StackChanApp: App {
    @State private var link: DeviceLink
    @State private var conversation: Conversation

    init() {
        let link = DeviceLink()
        let conversation = Conversation(link: link)
        link.onMessage = { [weak conversation] in conversation?.handle($0) }
        link.onAudio = { [weak conversation] in conversation?.handleAudio($0) }
        _link = State(initialValue: link)
        _conversation = State(initialValue: conversation)
    }

    var body: some Scene {
        WindowGroup {
            ContentView(link: link, conversation: conversation)
        }
    }
}
