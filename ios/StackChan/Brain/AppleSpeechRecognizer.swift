import AVFoundation
import Speech

/// STT with the Speech framework's SpeechAnalyzer, on device.
final class AppleSpeechRecognizer: SpeechRecognizer {
    let name = "Apple Speech (SpeechAnalyzer)"
    private let locale = Locale(identifier: "ja-JP")

    enum RecognizerError: LocalizedError {
        case unsupportedLocale
        case noAudioFormat

        var errorDescription: String? {
            switch self {
            case .unsupportedLocale: "この iPad では日本語の音声認識が使えません。"
            case .noAudioFormat: "音声認識に渡す音声の形式が決められません。"
            }
        }
    }

    private func makeTranscriber() async throws -> SpeechTranscriber {
        guard let supported = await SpeechTranscriber.supportedLocale(equivalentTo: locale) else {
            throw RecognizerError.unsupportedLocale
        }
        return SpeechTranscriber(locale: supported, preset: .transcription)
    }

    func prepare() async throws {
        let transcriber = try await makeTranscriber()
        if let request = try await AssetInventory.assetInstallationRequest(supporting: [transcriber]) {
            try await request.downloadAndInstall()
        }
    }

    func transcribe(_ audio: AsyncStream<[Int16]>) async throws -> String {
        let transcriber = try await makeTranscriber()
        let analyzer = SpeechAnalyzer(modules: [transcriber])
        guard let format = await SpeechAnalyzer.bestAvailableAudioFormat(compatibleWith: [transcriber]),
              let converter = PCMConverter(from: PCMConverter.wireFormat, to: format)
        else { throw RecognizerError.noAudioFormat }

        let collector = Task {
            var text = ""
            for try await result in transcriber.results where result.isFinal {
                text += String(result.text.characters)
            }
            return text
        }

        let (input, builder) = AsyncStream<AnalyzerInput>.makeStream()
        try await analyzer.start(inputSequence: input)
        for await samples in audio {
            if let wire = PCMConverter.buffer(from: samples), let converted = converter.convert(wire) {
                builder.yield(AnalyzerInput(buffer: converted))
            }
        }
        builder.finish()
        try await analyzer.finalizeAndFinishThroughEndOfInput()
        return try await collector.value.trimmingCharacters(in: .whitespacesAndNewlines)
    }
}
