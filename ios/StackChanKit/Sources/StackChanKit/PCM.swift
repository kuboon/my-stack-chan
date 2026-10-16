import Foundation

/// 16 kHz / 16-bit / mono little-endian PCM, the format on the wire.
public enum PCM {
    public static let sampleRate = 16_000

    public static func samples(from data: Data) -> [Int16] {
        let count = data.count / 2
        var out = [Int16](repeating: 0, count: count)
        data.withUnsafeBytes { raw in
            for i in 0..<count {
                out[i] = Int16(littleEndian: raw.loadUnaligned(fromByteOffset: i * 2, as: Int16.self))
            }
        }
        return out
    }

    public static func data(from samples: [Int16]) -> Data {
        var data = Data(capacity: samples.count * 2)
        for s in samples {
            withUnsafeBytes(of: s.littleEndian) { data.append(contentsOf: $0) }
        }
        return data
    }

    /// Float samples in -1...1 to Int16, clipping.
    public static func int16(from floats: UnsafeBufferPointer<Float>) -> [Int16] {
        floats.map { Int16(max(-1, min(1, $0)) * Float(Int16.max)) }
    }
}
