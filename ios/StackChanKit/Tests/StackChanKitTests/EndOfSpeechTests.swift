import Foundation
import Testing
@testable import StackChanKit

/// `seconds` of a tone at `amplitude` over a little noise, 16 kHz.
private func sound(_ seconds: Double, amplitude: Double) -> [Int16] {
    var noise = SystemRandomNumberGenerator()
    return (0..<Int(seconds * 16000)).map { i in
        let tone = amplitude * sin(Double(i) * 2 * .pi * 220 / 16000)
        return Int16(tone + Double(Int.random(in: -40...40, using: &noise)))
    }
}

/// Feeds `samples` in 50 ms pieces, as the StackChan sends them; the first event and when it came.
private func run(_ detector: inout EndOfSpeechDetector, _ samples: [Int16]) -> (EndOfSpeechDetector.Event, Double)? {
    for start in stride(from: 0, to: samples.count, by: 800) {
        let piece = Array(samples[start..<min(start + 800, samples.count)])
        if let event = detector.append(piece) { return (event, Double(start + piece.count) / 16000) }
    }
    return nil
}

@Test func endsAfterSpeechAndSilence() throws {
    var d = EndOfSpeechDetector()
    let audio = sound(0.3, amplitude: 0) + sound(1.0, amplitude: 4000) + sound(2.0, amplitude: 0)
    let (event, at) = try #require(run(&d, audio))
    #expect(event == .ended)
    #expect(at > 2.0 && at < 2.3)  // 1.3 s of audio before the silence, then 0.8 s of it
}

@Test func aPauseInsideASentenceDoesNotEndIt() {
    var d = EndOfSpeechDetector()
    let audio = sound(0.3, amplitude: 0) + sound(0.8, amplitude: 4000) + sound(0.4, amplitude: 0)
        + sound(0.8, amplitude: 4000)
    #expect(run(&d, audio) == nil)
}

@Test func aClickIsNotSpeech() throws {
    var d = EndOfSpeechDetector()
    let audio = sound(0.3, amplitude: 0) + sound(0.06, amplitude: 8000) + sound(7.0, amplitude: 0)
    let (event, at) = try #require(run(&d, audio))
    #expect(event == .noSpeech)
    #expect(at >= 6.0 && at < 6.2)
}

@Test func steadyNoiseIsNotSpeech() throws {
    var d = EndOfSpeechDetector()
    let (event, _) = try #require(run(&d, sound(7.0, amplitude: 600)))
    #expect(event == .noSpeech)
}

@Test func reportsOnlyOnce() {
    var d = EndOfSpeechDetector()
    _ = run(&d, sound(0.3, amplitude: 0) + sound(1.0, amplitude: 4000) + sound(1.0, amplitude: 0))
    #expect(d.append(sound(1.0, amplitude: 0)) == nil)
}
