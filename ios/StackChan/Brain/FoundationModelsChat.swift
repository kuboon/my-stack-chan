import Foundation
import FoundationModels

/// Replies from Apple's on-device model (Foundation Models).
final class FoundationModelsChat: ChatModel {
    let name = "Apple Foundation Models"
    private var session = LanguageModelSession(instructions: Persona.instructions)

    var isAvailable: Bool { SystemLanguageModel.default.isAvailable }

    func reply(to text: String) -> AsyncThrowingStream<String, Error> {
        let session = self.session
        return AsyncThrowingStream { continuation in
            let task = Task {
                do {
                    // Each snapshot holds the whole reply so far; pass on what is new.
                    var sent = ""
                    for try await snapshot in session.streamResponse(to: text) {
                        let content = snapshot.content
                        if content.hasPrefix(sent) {
                            continuation.yield(String(content.dropFirst(sent.count)))
                        }
                        sent = content
                    }
                    continuation.finish()
                } catch {
                    // The context window is small (about 4K tokens): start over when it fills.
                    if let generationError = error as? LanguageModelSession.GenerationError,
                       case .exceededContextWindowSize = generationError {
                        self.reset()
                    }
                    continuation.finish(throwing: error)
                }
            }
            continuation.onTermination = { _ in task.cancel() }
        }
    }

    func reset() {
        session = LanguageModelSession(instructions: Persona.instructions)
    }
}
