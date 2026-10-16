import Foundation
import Testing
@testable import StackChanKit

@Test func parsesHello() {
    let m = DeviceMessage(json: #"{"type":"hello","nonce":"AAECAwQFBgcICQoLDA0ODw=="}"#)
    #expect(m == .hello(nonce: Data(0..<16)))
}

@Test func parsesDeviceEvents() {
    #expect(DeviceMessage(json: #"{"type":"listen.end"}"#) == .listenEnd)
    #expect(DeviceMessage(json: #"{"type":"touch","gesture":"swipe_forward"}"#) == .touch(gesture: "swipe_forward"))
    #expect(DeviceMessage(json: "not json") == nil)
}

@Test func encodesAppMessages() {
    #expect(AppMessage.face(.happy).json == #"{"expression":"happy","type":"face"}"#)
    #expect(AppMessage.state(thinking: true).json == #"{"state":"thinking","type":"state"}"#)
    #expect(AppMessage.speakEnd.json == #"{"type":"speak.end"}"#)
}

@Test func splitsSentencesAndTakesTheTag() {
    var p = ReplyParser()
    #expect(p.append("[hap").isEmpty)
    #expect(p.append("py]こんにちは！今日は") == ["こんにちは！"])
    #expect(p.expression == .happy)
    #expect(p.append("いい天気だね。") == ["今日はいい天気だね。"])
    #expect(p.append("また[sad]ね") == [])
    #expect(p.finish() == ["またね"])
    #expect(p.expression == .happy)
}

@Test func pcmRoundTrips() {
    let samples: [Int16] = [0, 1, -1, .max, .min]
    #expect(PCM.samples(from: PCM.data(from: samples)) == samples)
}
