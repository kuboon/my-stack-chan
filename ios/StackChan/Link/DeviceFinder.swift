import Foundation
import Network

/// Finds StackChans on the same Wi-Fi: Bonjour `_stackchan._tcp`, whose instance names are
/// `stackchan-XXXX` and whose hosts are `stackchan-xxxx.local` (web/server/docs/protocol.md).
enum DeviceFinder {
    /// The hosts of the StackChans that show up within `timeout`.
    static func find(timeout: Duration = .seconds(2)) async -> [String] {
        let browser = NWBrowser(for: .bonjour(type: "_stackchan._tcp", domain: nil), using: .tcp)
        let results = AsyncStream<[String]> { continuation in
            browser.browseResultsChangedHandler = { results, _ in
                continuation.yield(results.compactMap { result in
                    if case .service(let name, _, _, _) = result.endpoint { name } else { nil }
                })
            }
            browser.stateUpdateHandler = { state in
                switch state {
                case .failed, .cancelled: continuation.finish()
                default: break
                }
            }
            browser.start(queue: .main)
        }
        let deadline = Task {
            try? await Task.sleep(for: timeout)
            browser.cancel()
        }
        var found: [String] = []
        for await names in results { found = names }
        deadline.cancel()
        return found.map { "\($0.lowercased()).local" }
    }
}
