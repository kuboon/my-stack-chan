import Testing
@testable import StackChanKit

@Suite struct SpokenIntentTests {
    @Test(arguments: ["写真撮って", "写真を撮って", "写真を撮ってください", "しゃしんとって", "ねえ、写真 撮って！", "自撮りして"])
    func photo(_ text: String) {
        #expect(SpokenIntent.parse(text) == .takePhoto)
    }

    @Test(arguments: [
        ("右向いて", LookDirection.right), ("右を見て", .right), ("左むいて", .left),
        ("上を向いて", .up), ("下見て", .down), ("正面を向いて", .front), ("前を見て", .front),
    ])
    func look(_ text: String, _ direction: LookDirection) {
        #expect(SpokenIntent.parse(text) == .look(direction))
    }

    @Test(arguments: ["こんにちは", "写真を見て", "今日は何をしたの", "左右を見て", "右って何", "上手だね"])
    func other(_ text: String) {
        #expect(SpokenIntent.parse(text) == nil)
    }
}
