import Foundation
import FoundationModels
import StackChanKit

/// Replies from Apple's on-device model (Foundation Models).
final class FoundationModelsChat: ChatModel {
    let name = "Apple Foundation Models"
    private let tools: [any Tool]
    private var session: LanguageModelSession

    var isAvailable: Bool { SystemLanguageModel.default.isAvailable }

    init(body: any StackChanBody) {
        tools = [TakePhotoTool(body: body), LookTool(body: body)]
        session = LanguageModelSession(tools: tools, instructions: Persona.instructions)
    }

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
        session = LanguageModelSession(tools: tools, instructions: Persona.instructions)
    }
}

// MARK: - Tools

private struct TakePhotoTool: Tool {
    let name = "takePhoto"
    let description = "スタックちゃんの顔のカメラで、目の前の写真を 1 枚撮る。写真を撮ってと頼まれたときに使う。"
    let body: any StackChanBody

    @Generable
    struct Arguments {}

    func call(arguments: Arguments) async throws -> String {
        await body.takePhoto() ? "写真を撮りました。" : "写真を撮れませんでした。"
    }
}

private struct LookTool: Tool {
    let name = "look"
    let description = "スタックちゃんの首を動かして、言われた向きを見る。右・左・上・下・正面を向いてと頼まれたときに使う。"
    let body: any StackChanBody

    @Generable
    enum Direction: String {
        case front, right, left, up, down
    }

    @Generable
    struct Arguments {
        @Guide(description: "向く方向。front は正面、right は右、left は左、up は上、down は下")
        var direction: Direction
    }

    func call(arguments: Arguments) async throws -> String {
        let direction = LookDirection(rawValue: arguments.direction.rawValue) ?? .front
        await body.look(direction)
        return "\(direction.rawValue) を向きました。"
    }
}
