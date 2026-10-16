---
title: iPad アプリ — 音声、LLM、Ruby → Wasm、設定
order: 3
date: "2026-10-15"
summary: iPad 側の設計案。直結モードの頭 (STT / ローカル LLM / TTS)、ペアリングと設定、Ruby を Wasm にしてスタックちゃんへ送る仕組み。
---

`ios/` に置く iPad アプリの設計案。SwiftUI で作り、iPadOS 26 以降を対象にする。
対象の実機は A17 Pro の iPad (iPad mini、メモリ 8 GB、Apple Intelligence 対応)。

iPad の役割は 2 つ。

- **直結モードの頭**: スタックちゃんが AP のとき、STT / LLM / TTS
  をすべてオンデバイスでやる。
- **設定と書き込みの道具**: ペアリング、Wi-Fi・Cloudflare Worker・Home Assistant
  の設定、Wasm と画像のアップロード。

スタックちゃんが Wi-Fi につながっている (単独モード) ときは、会話は M5 と
Cloudflare Worker だけで完結し、iPad は要らない。

## 画面

- **スタックちゃん**: 接続状態、会話ログ (直結モード)、押して話すボタン
- **接続**: スタックちゃんの AP につなぐ、ペアリング (確認コードの表示)
- **設定**: スタックちゃんに書き込む設定
  - Wi-Fi (スキャン結果から SSID を選びパスワードを入力)
  - Cloudflare Worker の URL とデバイス用の鍵
  - Home Assistant の URL、長期アクセストークン、LLM に触らせるエンティティ
- **挙動 (Ruby)**: Ruby エディタ、ビルド、アップロード、ロールバック
- **画像**: 写真やファイルから選んでアップロード

## ペアリング

M5 の API を使えるのは最初にハンドシェイクしたアプリだけ
([M5 ファームウェアの認証](./m5-firmware#api-の認証))。

1. 未ペアリングの M5 に X25519 の公開鍵を送り、M5 の公開鍵を受け取る (CryptoKit
   の `Curve25519.KeyAgreement`)。
2. 両者の公開鍵から 6 桁の確認コードを作って表示する。M5 の画面と同じなら、
   ユーザーが M5 をタッチして承認する。
3. HKDF で鍵を作り、Keychain に保存する。以降のリクエストはこの鍵で HMAC
   署名し、秘密を含むボディは AES-GCM で暗号化する。

鍵は M5 ごとに 1 つ。iPad を替えるときは M5 側でペアリングを消してから
やり直す。

## 直結モードの音声と LLM

すべてオンデバイスで完結させる (AP 接続中の iPad にはインターネットが無い)。
**まずは Apple 標準の機能で作り、あとで差し替えられるようにしておく。**

| 機能 | 最初に使うもの                                                 | 差し替え候補                                                       |
| ---- | -------------------------------------------------------------- | ------------------------------------------------------------------ |
| VAD  | (M5 のタッチで「押して話す」)                                  | speech.cpp の Silero VAD                                           |
| STT  | Speech フレームワーク (`SpeechAnalyzer` / `SpeechTranscriber`) | speech.cpp の parakeet-tdt_ctc-0.6b-ja                             |
| LLM  | Foundation Models フレームワーク (Apple のオンデバイス LLM)    | MLX Swift / llama.cpp の小さなモデル                               |
| TTS  | `AVSpeechSynthesizer`                                          | speech.cpp の Irodori-TTS (参照録音からスタックちゃんの声を作れる) |

### 差し替えられる形にする

STT / LLM / TTS をそれぞれ protocol にし、会話の流れ (M5 との WebSocket、
押して話す、口パク用の音声送信) はどの実装でも同じコードで動かす。

```swift
protocol SpeechRecognizer {
  /// M5 から届いた 16 kHz PCM を流し込み、確定したテキストを返す
  func transcribe(_ audio: AsyncStream<[Int16]>) async throws -> String
}

protocol ChatModel {
  func reply(to history: [Message]) async throws -> AsyncThrowingStream<String, Error>
}

protocol SpeechSynthesizer {
  /// 文を受け取り、できた順に 16 kHz PCM を返す (M5 へそのまま流す)
  func synthesize(_ text: String) -> AsyncThrowingStream<[Int16], Error>
}

// 最初: AppleSpeechRecognizer / FoundationModelsChat / AVSpeechSynthesizerTTS
// あとで: SpeechCppRecognizer / SpeechCppSynthesizer (libspeech を xcframework にして C API を呼ぶ)
```

- 設定画面で実装を選べるようにし、比べながら差し替える。
- LLM の応答はストリーミングで受け、文の区切りごとに TTS に渡して、
  しゃべり出しまでの時間を短くする。
- LLM には「スタックちゃんらしさ」のシステムプロンプトと、表情タグ (`[happy]`
  など) の出し方を指示する。Worker 側と同じプロンプトを使い、どちらが頭でも
  同じキャラクターにする。Foundation Models は文脈が短い (4K トークン程度)
  ので、会話の履歴は直近だけを渡す。
- STT は M5 から来た PCM を流し込む (マイクは M5 のものを使う)。
- TTS の音声は PCM にして M5 へ送り、M5 のスピーカーで鳴らす。
  `AVSpeechSynthesizer` は `write(_:toBufferCallback:)` でバッファを受け取り、
  16 kHz に変換して送る。
- 単独モードでネットが落ちたときも、同じ Wi-Fi にいれば iPad が頭を代われる。

### speech.cpp に差し替えるとき

[speech.cpp](https://github.com/nyosegawa/speech.cpp) は ggml (Metal) 上の C/C++
ライブラリで、C API を Swift から呼べる。公式は iOS 向けの
ビルドを出していないので、CMake で iOS (arm64 + Metal) 向けにビルドして
xcframework にする必要がある。

- メモリ: STT (parakeet-ja Q8_0、約 0.8 GB) + TTS (Irodori-TTS MF Q5_K、約 1.1
  GB) で約 2 GB。8 GB の A17 Pro なら載るが、Increased Memory Limit の
  entitlement を付ける。
- モデルは 1〜2 GB あるので、アプリに同梱せず初回にダウンロードする。
- 速度は Mac (M5) の数字しか公表されていないので、実機で測ってから決める。

## Ruby → Wasm

M5 に Ruby 処理系は載せず、iPad で Wasm にしてから送る。

```
Ruby ソース ──[コンパイラ.wasm]──▶ バイトコード (.mrb)
                                       │
VM.wasm (mruby/c 系 + stackchan ABI) ──┤ 埋め込み
                                       ▼
                                behavior.wasm ──PUT /wasm──▶ スタックちゃん
```

1. **コンパイラ**: mruby / PicoRuby のバイトコードコンパイラを Wasm (WASI)
   にビルドしてアプリに同梱し、Swift 製の Wasm ランタイム
   ([WasmKit](https://github.com/swiftwasm/WasmKit)) で動かす。iOS は JIT
   が使えないが、インタプリタなので問題ない。
2. **VM.wasm**: mruby/c 系 VM と、[M5 の ABI](./m5-firmware) を Ruby
   から呼ぶためのバインディング (`Stackchan.face(:happy)` など)
   を事前にビルドして 同梱する。
3. **リンク**: VM.wasm のデータセグメント (またはカスタムセクション) に
   バイトコードを入れて `behavior.wasm` を作る。Wasm のバイナリ操作だけなので
   Swift で書ける。
4. **確認**: アップロード前に iPad の WasmKit 上でホスト関数をモックして `init`
   と数回の `on_tick` を実行し、エラーや時間超過を検出する。

Ruby で書く挙動のイメージ:

```ruby
class Behavior
  def on_event(event)
    case event[:name]
    when "touch"
      Stackchan.face(:happy)
      Stackchan.say("くすぐったいよ")
    when "heard"
      Stackchan.face(:thinking)
    end
  end

  def on_tick(now)
    Stackchan.gaze(Math.sin(now / 1000.0) * 0.3, 0) if now % 5000 < 50
  end
end
```

## 接続まわりの注意

- スタックちゃんの AP への接続は `NEHotspotConfiguration` でアプリから行える
  (Hotspot Configuration の entitlement が必要)。
- 同じ Wi-Fi での探索は Bonjour (`_stackchan._tcp`)。Info.plist に
  `NSLocalNetworkUsageDescription` と `NSBonjourServices` を書く。
- AP 接続中は iPad にインターネットが無い (iOS は「インターネット未接続」と
  表示する) ので、その間はオンデバイスの機能だけで動かす。
