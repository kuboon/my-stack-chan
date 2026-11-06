import SwiftUI
import UniformTypeIdentifiers

/// Updating the StackChan's firmware over Wi-Fi with a `firmware.bin` from Files.
struct FirmwareView: View {
    var link: DeviceLink
    var update: FirmwareUpdate
    @Environment(\.dismiss) private var dismiss
    @State private var picking = false

    var body: some View {
        NavigationStack {
            List {
                Section("いまのファームウェア") {
                    if let info = link.deviceInfo {
                        LabeledContent("バージョン", value: info.firmware)
                        if let built = info.built { LabeledContent("ビルド", value: built) }
                        if let max = info.otaMaxSize {
                            LabeledContent("書き込める大きさ", value: ByteCountFormatter.string(fromByteCount: Int64(max), countStyle: .file))
                        } else {
                            Text("このファームウェアは Wi-Fi での更新に対応していません。一度 USB で書き込んでください。")
                                .foregroundStyle(.secondary)
                        }
                    } else {
                        Text("先にスタックちゃんにつないでください。").foregroundStyle(.secondary)
                    }
                }
                Section {
                    Button("firmware.bin を選んで書き込む…") { picking = true }
                        .disabled(!link.isConnected || update.busy || link.deviceInfo?.otaMaxSize == nil)
                    status
                } footer: {
                    Text("GitHub の Actions (m5) の成果物 firmware、または手元の `pio run` が作る .pio/build/stackchan/firmware.bin。書き込んだあとスタックちゃんは再起動し、新しい版が 30 秒動けばそれに決まる。それより前に止まったら前の版に戻る。")
                }
            }
            .navigationTitle("ファームウェアの更新")
            .toolbar {
                Button("閉じる") { dismiss() }.disabled(update.busy)
            }
            .fileImporter(isPresented: $picking, allowedContentTypes: [.data]) { result in
                if case .success(let file) = result {
                    Task { await update.install(from: file) }
                }
            }
        }
        .interactiveDismissDisabled(update.busy)
    }

    @ViewBuilder private var status: some View {
        switch update.state {
        case .idle:
            EmptyView()
        case .preparing:
            ProgressView("準備しています…")
        case .uploading(let bytes):
            ProgressView("送っています… (\(ByteCountFormatter.string(fromByteCount: Int64(bytes), countStyle: .file)))")
        case .restarting:
            ProgressView("スタックちゃんが再起動しています…")
        case .done(let firmware, let built):
            Label("更新しました: \(firmware) (\(built))", systemImage: "checkmark.circle.fill")
                .foregroundStyle(.green)
        case .failed(let message):
            Text(message).foregroundStyle(.red)
        }
    }
}
