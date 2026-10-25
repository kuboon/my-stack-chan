import Observation
import StackChanKit

/// Setting the StackChan's home Wi-Fi from the app: `wifi.scan`, `wifi.connect` and `wifi.forget`
/// (web/server/docs/protocol.md, "Wi-Fi の設定").
@MainActor @Observable
final class WifiSetup {
    enum State: Equatable {
        case idle
        case scanning
        case joining(ssid: String)
        /// Joined; the StackChan restarts onto it.
        case joined(ssid: String, ip: String)
        case failed(String)
        /// Asked to forget; the StackChan restarts as its own AP.
        case forgotten
    }

    private(set) var networks: [WifiNetwork] = []
    private(set) var state: State = .idle

    @ObservationIgnored private let link: DeviceLink

    init(link: DeviceLink) {
        self.link = link
    }

    func scan() {
        guard link.isConnected else { return }
        state = .scanning
        link.send(.wifiScan)
    }

    func join(_ ssid: String, password: String) {
        do {
            try link.joinWifi(ssid: ssid, password: password)
            state = .joining(ssid: ssid)
        } catch {
            state = .failed(error.localizedDescription)
        }
    }

    func forget() {
        link.send(.wifiForget)
        state = .forgotten
    }

    func handle(_ message: DeviceMessage) {
        switch message {
        case .wifiNetworks(let list):
            networks = list
            if state == .scanning { state = .idle }
        case .wifiJoined(let ssid, let ip):
            state = .joined(ssid: ssid, ip: ip)
        case .wifiFailed(let error):
            let reason = switch error {
            case "wrong_password": "パスワードがちがうようです。"
            case "not_found": "その Wi-Fi が見つかりませんでした。"
            case "timeout": "時間内につながりませんでした。"
            default: "スタックちゃんがパスワードを受け取れませんでした。"
            }
            state = .failed(reason)
        default:
            break
        }
    }
}
