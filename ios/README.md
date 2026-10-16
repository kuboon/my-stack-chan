# ios — iPad アプリ

対象は A17 Pro の iPad (iPadOS 26 以降)。設計は
[`web/server/docs/ios-app.md`](../web/server/docs/ios-app.md)、スタックちゃんとのやり取りは
[`web/server/docs/protocol.md`](../web/server/docs/protocol.md)。

## いまできること (直結モード)

- スタックちゃんの AP 上で `192.168.4.1` につなぎ、ペアリング (確認コードを表示)
- 共有鍵を Keychain に保存し、WebSocket を認証
- 会話: スタックちゃんのマイクの音 → STT → LLM → TTS → スタックちゃんで再生
  - STT: Speech (`SpeechAnalyzer`)、LLM: Foundation Models、TTS:
    `AVSpeechSynthesizer`
  - それぞれ `Brain/Backends.swift` の protocol の実装なので、差し替えられる
- 返事の先頭の `[happy]` などのタグでスタックちゃんの表情を変える

## 使い方

1. iPad がインターネットにつながっているうちに、アプリで「音声認識を準備」を押す
   (日本語の音声認識モデルをダウンロードする)。Apple Intelligence
   も有効にしておく。
2. スタックちゃんの画面の QR コードを iPad のカメラで読み、スタックちゃんの
   Wi-Fi につなぐ。
3. アプリで「つなぐ」。初回は両方の画面に出る番号を見比べ、スタックちゃんの「承認」を押す。
4. スタックちゃんの画面をタップ (またはアプリの「話しかける」)
   して話し、もう一度タップ。

## ビルド

```sh
brew install xcodegen
cd ios
xcodegen generate         # StackChan.xcodeproj を作る (git には入れない)
open StackChan.xcodeproj  # 実機を選んで実行。Signing の Team を自分のものにする
```

`StackChanKit/` は端末の要らない部分 (ペアリングの暗号、メッセージ、返事の分割)
の Swift パッケージ。`swift test --package-path StackChanKit` で試せる。
