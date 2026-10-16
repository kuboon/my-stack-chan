import Foundation

/// Turns an LLM reply arriving in pieces into sentences to speak, and pulls out the expression
/// tag (`[happy]` and the like) the instructions ask it to start with.
public struct ReplyParser: Sendable {
    private var buffer = ""
    private var sawTag = false
    public private(set) var expression: FaceExpression?

    public init() {}

    /// Adds text and returns the sentences it completed.
    public mutating func append(_ text: String) -> [String] {
        buffer += text
        stripTags()
        var sentences: [String] = []
        while let end = buffer.firstIndex(where: { "。！？!?\n".contains($0) }) {
            let sentence = String(buffer[...end]).trimmingCharacters(in: .whitespacesAndNewlines)
            buffer.removeSubrange(...end)
            if !sentence.isEmpty { sentences.append(sentence) }
        }
        return sentences
    }

    /// What is left when the reply is complete.
    public mutating func finish() -> [String] {
        stripTags()
        let rest = buffer.trimmingCharacters(in: .whitespacesAndNewlines)
        buffer = ""
        return rest.isEmpty ? [] : [rest]
    }

    /// Removes `[word]` tags, keeping the first known expression. A tag cut off at the end of the
    /// buffer stays until the rest of it arrives.
    private mutating func stripTags() {
        while let open = buffer.firstIndex(of: "[") {
            guard let close = buffer[open...].firstIndex(of: "]") else { return }
            let word = buffer[buffer.index(after: open)..<close].lowercased()
            if !sawTag, let e = FaceExpression(rawValue: word) {
                expression = e
                sawTag = true
            }
            buffer.removeSubrange(open...close)
        }
    }
}
