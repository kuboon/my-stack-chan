import Foundation

/// What the StackChan is asked to do with its body, read from the words alone.
///
/// The on-device model is not reliable at calling tools: it sometimes writes the tool's name as
/// its reply ("takePhoto") instead. The plain requests are few and fixed, so they are recognised
/// here and done without the model.
public enum SpokenIntent: Equatable, Sendable {
    case takePhoto
    case look(LookDirection)

    public static func parse(_ text: String) -> SpokenIntent? {
        let t = text.replacingOccurrences(of: " ", with: "").replacingOccurrences(of: "　", with: "")

        let photoWords = ["写真", "しゃしん", "シャシン", "自撮", "パシャ"]
        let shootWords = ["撮", "とって", "とれ", "とる", "パシャ"]
        if photoWords.contains(where: t.contains), shootWords.contains(where: t.contains) {
            return .takePhoto
        }

        let turnWords = ["向", "むいて", "みて", "見"]
        guard turnWords.contains(where: t.contains) else { return nil }
        let directions: [(LookDirection, [String])] = [
            (.right, ["右", "みぎ"]),
            (.left, ["左", "ひだり"]),
            (.up, ["上", "うえ"]),
            (.down, ["下", "した"]),
            (.front, ["正面", "しょうめん", "前", "まえ"]),
        ]
        let found = directions.filter { _, words in words.contains(where: t.contains) }
        // "左右" and the like are not a direction to turn to.
        return found.count == 1 ? .look(found[0].0) : nil
    }
}
