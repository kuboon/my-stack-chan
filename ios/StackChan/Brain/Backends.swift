import Foundation

/// Speech to text. Gets the PCM of one recording as it arrives and returns what was said.
protocol SpeechRecognizer: AnyObject {
    var name: String { get }
    /// Downloads whatever the recognizer needs. Call while the iPad still has internet.
    func prepare() async throws
    /// 16 kHz mono samples, as the StackChan sends them; the stream ends with the recording.
    func transcribe(_ audio: AsyncStream<[Int16]>) async throws -> String
}

/// The language model that writes the replies.
protocol ChatModel: AnyObject {
    var name: String { get }
    var isAvailable: Bool { get }
    /// The reply to what the user said, in pieces as they are written.
    func reply(to text: String) -> AsyncThrowingStream<String, Error>
    /// Forgets the conversation.
    func reset()
}

/// Text to speech. Returns 16 kHz mono samples, in pieces as they are made.
protocol SpeechSynthesizer: AnyObject {
    var name: String { get }
    func synthesize(_ text: String) -> AsyncThrowingStream<[Int16], Error>
}

/// Who StackChan is. Shared by every ChatModel, and to be shared with the Worker's.
enum Persona {
    static let instructions = """
    あなたは「スタックちゃん」という、机の上にいる小さなロボットです。
    やさしく元気な性格で、話し相手と仲良しです。
    返事は話し言葉で、1〜3 文の短い日本語にしてください。絵文字や記号、箇条書きは使いません。
    返事の最初に、そのときの気持ちを次のどれか 1 つのタグで書いてください:
    [neutral] [happy] [sad] [angry] [doubt] [sleepy]
    例: [happy]こんにちは！今日もいっしょにがんばろうね。
    """
}
