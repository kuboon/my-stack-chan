import Foundation
import Observation
import StackChanKit

/// Direct mode's brain: hears a recording from the StackChan, thinks of a reply, and sends it back
/// as speech (web/server/docs/protocol.md, "会話の流れ").
@MainActor @Observable
final class Conversation {
    enum Phase: Equatable {
        case idle, listening, thinking, speaking
    }

    struct Line: Identifiable {
        enum Role { case user, stackchan, note }
        let id = UUID()
        let role: Role
        let text: String
    }

    private(set) var phase: Phase = .idle
    private(set) var lines: [Line] = []
    private(set) var preparing = false

    // The backends, each behind a protocol so that another can replace it (speech.cpp and the like).
    @ObservationIgnored var recognizer: any SpeechRecognizer = AppleSpeechRecognizer()
    @ObservationIgnored var chat: any ChatModel = FoundationModelsChat()
    @ObservationIgnored var synthesizer: any SpeechSynthesizer = AppleSpeechSynthesizer()

    @ObservationIgnored private let link: DeviceLink
    @ObservationIgnored private var audio: AsyncStream<[Int16]>.Continuation?
    @ObservationIgnored private var transcription: Task<String, Error>?
    @ObservationIgnored private var speechStarted = false

    init(link: DeviceLink) {
        self.link = link
    }

    /// Downloads the speech model while there is still internet (before joining the StackChan's AP).
    func prepare() async {
        preparing = true
        defer { preparing = false }
        do {
            try await recognizer.prepare()
            note("音声認識の準備ができました。")
        } catch {
            note("音声認識の準備に失敗しました: \(error.localizedDescription)")
        }
        if !chat.isAvailable {
            note("この iPad では \(chat.name) が使えません (Apple Intelligence を有効にしてください)。")
        }
    }

    // MARK: - From the StackChan

    func handle(_ message: DeviceMessage) {
        switch message {
        case .listenStart: startListening()
        case .listenEnd: finishListening()
        case .speakDone:
            if phase == .speaking {
                phase = .idle
                link.send(.state(thinking: false))
            }
        case .touch(let gesture):
            if phase == .idle, gesture == "tap" { link.send(.face(.happy)) }
        default:
            break
        }
    }

    func handleAudio(_ data: Data) {
        audio?.yield(PCM.samples(from: data))
    }

    // MARK: - From the screen

    /// The app's push-to-talk: the StackChan records, and answers with listen.start / listen.end.
    func toggleListening() {
        link.send(phase == .listening ? .listenEnd : .listenStart)
    }

    func clear() {
        lines.removeAll()
        chat.reset()
    }

    // MARK: -

    private func startListening() {
        guard phase == .idle || phase == .speaking else { return }
        audio?.finish()
        let (stream, continuation) = AsyncStream<[Int16]>.makeStream()
        audio = continuation
        let recognizer = self.recognizer
        transcription = Task { try await recognizer.transcribe(stream) }
        phase = .listening
    }

    private func finishListening() {
        guard phase == .listening, let transcription else { return }
        audio?.finish()
        audio = nil
        self.transcription = nil
        phase = .thinking
        link.send(.state(thinking: true))
        Task {
            do {
                let text = try await transcription.value
                await respond(to: text)
            } catch {
                note("聞き取れませんでした: \(error.localizedDescription)")
                finishWithoutSpeech()
            }
        }
    }

    private func respond(to text: String) async {
        guard !text.isEmpty else {
            finishWithoutSpeech()
            return
        }
        lines.append(Line(role: .user, text: text))

        var parser = ReplyParser()
        var spoken: [String] = []
        speechStarted = false
        do {
            for try await piece in chat.reply(to: text) {
                for sentence in parser.append(piece) {
                    await speak(sentence, expression: parser.expression)
                    spoken.append(sentence)
                }
            }
            for sentence in parser.finish() {
                await speak(sentence, expression: parser.expression)
                spoken.append(sentence)
            }
        } catch {
            note("返事を作れませんでした: \(error.localizedDescription)")
            let sorry = "ごめんね、うまく考えられなかった。"
            await speak(sorry, expression: .sad)
            spoken.append(sorry)
        }

        if !spoken.isEmpty { lines.append(Line(role: .stackchan, text: spoken.joined())) }
        if speechStarted {
            link.send(.speakEnd)
            phase = .speaking  // until speak.done
        } else {
            finishWithoutSpeech()
        }
    }

    /// Sends one sentence as speech. The first one opens the speech: expression, then speak.start.
    private func speak(_ sentence: String, expression: FaceExpression?) async {
        if !speechStarted {
            if let expression { link.send(.face(expression)) }
            link.send(.speakStart(text: sentence))
            speechStarted = true
        }
        do {
            for try await samples in synthesizer.synthesize(sentence) {
                link.sendAudio(PCM.data(from: samples))
            }
        } catch {
            note("読み上げに失敗しました: \(error.localizedDescription)")
        }
    }

    private func finishWithoutSpeech() {
        phase = .idle
        link.send(.state(thinking: false))
    }

    private func note(_ text: String) {
        lines.append(Line(role: .note, text: text))
    }
}
