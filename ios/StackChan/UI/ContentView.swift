import SwiftUI

struct ContentView: View {
    @Bindable var link: DeviceLink
    var conversation: Conversation
    var camera: RemoteCamera
    @State private var showingCamera = false

    var body: some View {
        NavigationStack {
            VStack(spacing: 0) {
                ConnectionPanel(link: link, conversation: conversation)
                Divider()
                ConversationLog(conversation: conversation)
                Divider()
                TalkBar(link: link, conversation: conversation)
            }
            .navigationTitle("スタックちゃん")
            .toolbar {
                Button {
                    showingCamera = true
                } label: {
                    Image(systemName: "camera")
                }
                .disabled(!link.isConnected)
                Menu {
                    Button("会話を消す", action: conversation.clear)
                    Button("この iPad のペアリングを消す", role: .destructive, action: link.forgetPairing)
                } label: {
                    Image(systemName: "ellipsis.circle")
                }
            }
            .sheet(isPresented: $showingCamera) {
                PhotoView(link: link, camera: camera)
            }
        }
    }
}

private struct ConnectionPanel: View {
    @Bindable var link: DeviceLink
    var conversation: Conversation

    var body: some View {
        VStack(alignment: .leading, spacing: 12) {
            switch link.status {
            case .disconnected, .failed:
                Text("スタックちゃんの画面の QR コードをカメラで読んで Wi-Fi につないでから、「つなぐ」を押してください。")
                    .font(.callout)
                HStack {
                    TextField("アドレス", text: $link.host)
                        .textFieldStyle(.roundedBorder)
                        .keyboardType(.numbersAndPunctuation)
                        .autocorrectionDisabled()
                        .frame(maxWidth: 220)
                    Button("つなぐ") { Task { await link.connect() } }
                        .buttonStyle(.borderedProminent)
                    Button {
                        Task { await conversation.prepare() }
                    } label: {
                        if conversation.preparing { ProgressView() } else { Text("音声認識を準備") }
                    }
                    .disabled(conversation.preparing)
                }
                if case .failed(let message) = link.status {
                    Text(message).foregroundStyle(.red).font(.callout)
                }
            case .connecting:
                ProgressView("つないでいます…")
            case .pairing(let code):
                VStack(alignment: .leading, spacing: 8) {
                    Text("スタックちゃんの画面に同じ番号が出ていたら、スタックちゃんの「承認」を押してください。")
                    Text(code)
                        .font(.system(size: 64, weight: .bold, design: .monospaced))
                        .tracking(8)
                }
            case .connected(let name):
                HStack {
                    Label(name, systemImage: "checkmark.circle.fill").foregroundStyle(.green)
                    Spacer()
                    Button("切る", action: link.disconnect)
                }
            }
        }
        .padding()
        .frame(maxWidth: .infinity, alignment: .leading)
    }
}

private struct ConversationLog: View {
    var conversation: Conversation

    var body: some View {
        ScrollViewReader { proxy in
            List(conversation.lines) { line in
                switch line.role {
                case .user:
                    Text(line.text).frame(maxWidth: .infinity, alignment: .trailing)
                case .stackchan:
                    Label(line.text, systemImage: "face.smiling")
                case .note:
                    Text(line.text).font(.footnote).foregroundStyle(.secondary)
                }
            }
            .listStyle(.plain)
            .onChange(of: conversation.lines.count) {
                if let last = conversation.lines.last { proxy.scrollTo(last.id, anchor: .bottom) }
            }
        }
    }
}

private struct TalkBar: View {
    var link: DeviceLink
    var conversation: Conversation

    var body: some View {
        HStack {
            Text(phaseText).foregroundStyle(.secondary)
            Spacer()
            Button {
                conversation.toggleListening()
            } label: {
                Label(
                    conversation.phase == .listening ? "話し終わった" : "話しかける",
                    systemImage: conversation.phase == .listening ? "stop.circle.fill" : "mic.circle.fill"
                )
                .font(.title2)
            }
            .buttonStyle(.borderedProminent)
            .disabled(!link.isConnected || conversation.phase == .thinking)
        }
        .padding()
    }

    private var phaseText: String {
        switch conversation.phase {
        case .idle: "スタックちゃんの画面をタップしても話しかけられます"
        case .listening: "きいています…"
        case .thinking: "考えています…"
        case .speaking: "しゃべっています…"
        }
    }
}
