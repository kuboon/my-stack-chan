import Foundation
import Testing
@testable import StackChanKit

@Test func parsesHello() {
    let m = DeviceMessage(json: #"{"type":"hello","nonce":"AAECAwQFBgcICQoLDA0ODw=="}"#)
    #expect(m == .hello(nonce: Data(0..<16)))
}

@Test func parsesDeviceEvents() {
    #expect(DeviceMessage(json: #"{"type":"listen.end"}"#) == .listenEnd(heard: nil))
    #expect(DeviceMessage(json: #"{"type":"touch","gesture":"swipe_forward"}"#) == .touch(gesture: "swipe_forward"))
    #expect(DeviceMessage(json: "not json") == nil)
}

@Test func parsesPhotos() {
    let json = #"{"type":"photo","width":640,"height":480,"ms":{"init":1,"warmup":2,"shot":3,"jpeg":4,"frames":[6,7]},"missed":5,"jpeg":"/9j/2Q=="}"#
    #expect(DeviceMessage(json: json) == .photo(
        jpeg: Data([0xFF, 0xD8, 0xFF, 0xD9]), width: 640, height: 480,
        timing: PhotoTiming(initMs: 1, warmupMs: 2, shotMs: 3, jpegMs: 4, frameMs: [6, 7], missed: 5)
    ))
    #expect(DeviceMessage(json: #"{"type":"photo","error":"busy"}"#) == .photoFailed(reason: "busy"))
}

@Test func parsesWifiMessages() {
    #expect(DeviceMessage(json: #"{"type":"wifi.networks","networks":[{"ssid":"home","rssi":-50,"secure":true}]}"#)
        == .wifiNetworks([WifiNetwork(ssid: "home", rssi: -50, secure: true)]))
    #expect(DeviceMessage(json: #"{"type":"wifi.result","ok":true,"ssid":"home","ip":"192.168.1.9"}"#)
        == .wifiJoined(ssid: "home", ip: "192.168.1.9"))
    #expect(DeviceMessage(json: #"{"type":"wifi.result","ok":false,"error":"wrong_password"}"#)
        == .wifiFailed(error: "wrong_password"))
    #expect(AppMessage.wifiConnect(ssid: "home", nonce: Data([0]), password: Data([1])).json
        == #"{"nonce":"AA==","password":"AQ==","ssid":"home","type":"wifi.connect"}"#)
}

@Test func encodesAppMessages() {
    #expect(AppMessage.face(.happy).json == #"{"expression":"happy","type":"face"}"#)
    #expect(AppMessage.state(thinking: true).json == #"{"state":"thinking","type":"state"}"#)
    #expect(AppMessage.speakEnd.json == #"{"type":"speak.end"}"#)
    #expect(AppMessage.photoTake(size: .qvga).json == #"{"size":"qvga","type":"photo.take"}"#)
}

@Test func looksWhereItIsTold() {
    #expect(LookDirection.right.yaw > 0 && LookDirection.left.yaw < 0)
    #expect(LookDirection.front.yaw == 0)
    #expect(LookDirection.up.pitch > LookDirection.front.pitch)
    #expect(LookDirection.front.pitch > LookDirection.down.pitch)
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

@Test func parsesHeardCommand() {
    let json = #"{"type":"listen.end","actions":[{"name":"nod","arguments":{"count":2}}],"accepted":true,"p_act":0.995,"kana":"うなずいて"}"#
    guard case .listenEnd(let heard?) = DeviceMessage(json: json) else {
        Issue.record("no heard command")
        return
    }
    #expect(heard.accepted)
    #expect(heard.names == ["nod"])
    #expect(heard.kana == "うなずいて")
    #expect(heard.actions == #"[{"arguments":{"count":2},"name":"nod"}]"#)
    #expect(abs(heard.pAct - 0.995) < 1e-9)
}
