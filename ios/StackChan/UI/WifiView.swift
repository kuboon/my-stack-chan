import StackChanKit
import SwiftUI

/// The StackChan's home Wi-Fi: pick a network it sees, give the password, and it joins.
struct WifiView: View {
    var link: DeviceLink
    var setup: WifiSetup
    @Environment(\.dismiss) private var dismiss
    @State private var chosen: WifiNetwork?
    @State private var password = ""
    @State private var confirmingForget = false

    var body: some View {
        NavigationStack {
            List {
                Section {
                    Text(link.deviceWifi == "sta"
                        ? "スタックちゃんは家の Wi-Fi につながっています。"
                        : "スタックちゃんはいま自分の Wi-Fi (AP) を出しています。")
                    if !link.isConnected {
                        Text("先にスタックちゃんにつないでください。").foregroundStyle(.secondary)
                    }
                }
                status
                Section("スタックちゃんから見える Wi-Fi") {
                    if setup.state == .scanning {
                        ProgressView("探しています…")
                    }
                    ForEach(setup.networks) { network in
                        Button {
                            chosen = network
                            password = ""
                        } label: {
                            HStack {
                                Text(network.ssid)
                                Spacer()
                                if network.secure { Image(systemName: "lock.fill").foregroundStyle(.secondary) }
                                Image(systemName: "wifi", variableValue: strength(network.rssi))
                            }
                        }
                        .disabled(!link.isConnected)
                    }
                }
                if link.deviceWifi == "sta" {
                    Section {
                        Button("家の Wi-Fi を忘れさせる", role: .destructive) { confirmingForget = true }
                            .disabled(!link.isConnected)
                    } footer: {
                        Text("スタックちゃんは再起動して、自分の Wi-Fi (AP) を出します。")
                    }
                }
            }
            .navigationTitle("スタックちゃんの Wi-Fi")
            .toolbar {
                ToolbarItem(placement: .primaryAction) {
                    Button("探す", action: setup.scan).disabled(!link.isConnected || setup.state == .scanning)
                }
                ToolbarItem(placement: .cancellationAction) {
                    Button("閉じる") { dismiss() }
                }
            }
            .onAppear { if setup.networks.isEmpty { setup.scan() } }
            .alert(
                chosen.map { "「\($0.ssid)」につなぐ" } ?? "",
                isPresented: Binding(get: { chosen != nil }, set: { if !$0 { chosen = nil } })
            ) {
                if chosen?.secure == true {
                    SecureField("パスワード", text: $password)
                }
                Button("つなぐ") {
                    if let chosen { setup.join(chosen.ssid, password: password) }
                    password = ""
                }
                Button("やめる", role: .cancel) { password = "" }
            } message: {
                Text("パスワードは暗号にしてスタックちゃんに送ります。")
            }
            .confirmationDialog("家の Wi-Fi を忘れさせますか？", isPresented: $confirmingForget) {
                Button("忘れさせる", role: .destructive, action: setup.forget)
            }
        }
    }

    @ViewBuilder private var status: some View {
        switch setup.state {
        case .idle, .scanning:
            EmptyView()
        case .joining(let ssid):
            Section {
                ProgressView("「\(ssid)」につないでいます…")
                Text("つなぐ間に、スタックちゃんとの接続が切れることがあります。そのときは結果をスタックちゃんの画面で見てください。")
                    .font(.footnote).foregroundStyle(.secondary)
            }
        case .joined(let ssid, let ip):
            Section {
                Label("「\(ssid)」につながりました (\(ip))", systemImage: "checkmark.circle.fill")
                    .foregroundStyle(.green)
                Text("スタックちゃんは再起動します。iPad も「\(ssid)」につないでから、アプリで「つなぐ」を押してください。")
            }
        case .failed(let message):
            Section { Text(message).foregroundStyle(.red) }
        case .forgotten:
            Section {
                Text("スタックちゃんは再起動して、自分の Wi-Fi を出します。画面の QR コードで iPad をつないでください。")
            }
        }
    }

    private func strength(_ rssi: Int) -> Double {
        min(1, max(0, Double(rssi + 90) / 40))
    }
}
