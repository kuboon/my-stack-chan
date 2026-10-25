import Foundation

/// Finds where the speaker stopped, from the loudness of the recording: speech, then enough
/// silence. The noise floor is learned from the quiet parts, so a fan or the servos do not count
/// as speech.
public struct EndOfSpeechDetector: Sendable {
    public enum Event: Equatable, Sendable {
        /// Speech came, then `silenceToEnd` of quiet.
        case ended
        /// Nothing was said for `noSpeechTimeout`.
        case noSpeech
    }

    public var sampleRate = 16000
    /// Quiet after speech that ends it, in seconds.
    public var silenceToEnd = 0.8
    /// Speech needed before the end is looked for, in seconds: a tap's click is not speech.
    public var minSpeech = 0.25
    /// How long to wait for any speech at all, in seconds.
    public var noSpeechTimeout = 6.0
    /// How far above the noise floor speech is, as a ratio of RMS (3.2 is about 10 dB).
    public var speechRatio = 3.2
    /// Below this RMS nothing is speech, however quiet the room.
    public var minSpeechLevel = 300.0

    private static let frameSeconds = 0.02
    private var pending: [Int16] = []
    private var noiseFloor: Double?
    private var speechFrames = 0
    private var silentFrames = 0
    private var frames = 0
    private var done = false

    public init() {}

    /// Feeds the next samples. Returns an event once, when it happens.
    public mutating func append(_ samples: [Int16]) -> Event? {
        guard !done else { return nil }
        pending += samples
        let frameLength = Int(Double(sampleRate) * Self.frameSeconds)
        while pending.count >= frameLength {
            let level = Self.rms(pending[0..<frameLength])
            pending.removeFirst(frameLength)
            if let event = step(level) {
                done = true
                return event
            }
        }
        return nil
    }

    private mutating func step(_ level: Double) -> Event? {
        frames += 1
        let floor = noiseFloor ?? level
        let isSpeech = level > max(floor * speechRatio, minSpeechLevel)
        if isSpeech {
            speechFrames += 1
            silentFrames = 0
        } else {
            silentFrames += 1
            // Follow the room slowly, and only while it is quiet.
            noiseFloor = noiseFloor.map { $0 * 0.95 + level * 0.05 } ?? level
        }
        if seconds(speechFrames) >= minSpeech {
            if seconds(silentFrames) >= silenceToEnd { return .ended }
        } else if seconds(frames) >= noSpeechTimeout {
            return .noSpeech
        }
        return nil
    }

    private func seconds(_ frames: Int) -> Double {
        Double(frames) * Self.frameSeconds
    }

    private static func rms(_ samples: ArraySlice<Int16>) -> Double {
        guard !samples.isEmpty else { return 0 }
        let sum = samples.reduce(0.0) { $0 + Double($1) * Double($1) }
        return (sum / Double(samples.count)).squareRoot()
    }
}
