import AVFoundation

/// TTS with AVSpeechSynthesizer, rendered to buffers rather than played on the iPad.
final class AppleSpeechSynthesizer: SpeechSynthesizer {
    let name = "AVSpeechSynthesizer"
    private let synthesizer = AVSpeechSynthesizer()

    /// Holds the converter between callbacks, which arrive on the synthesizer's queue.
    private final class State: @unchecked Sendable {
        var converter: PCMConverter?
        var finished = false
    }

    func synthesize(_ text: String) -> AsyncThrowingStream<[Int16], Error> {
        AsyncThrowingStream { continuation in
            let utterance = AVSpeechUtterance(string: text)
            utterance.voice = AVSpeechSynthesisVoice(language: "ja-JP")
            utterance.rate = AVSpeechUtteranceDefaultSpeechRate
            utterance.pitchMultiplier = 1.3
            let state = State()
            synthesizer.write(utterance) { buffer in
                guard !state.finished else { return }
                guard let pcm = buffer as? AVAudioPCMBuffer, pcm.frameLength > 0 else {
                    state.finished = true
                    continuation.finish()
                    return
                }
                if state.converter == nil {
                    state.converter = PCMConverter(from: pcm.format, to: PCMConverter.wireFormat)
                }
                if let converted = state.converter?.convert(pcm) {
                    continuation.yield(PCMConverter.samples(from: converted))
                }
            }
        }
    }
}
