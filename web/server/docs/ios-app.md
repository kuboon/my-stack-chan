---
title: iPad アプリ — 音声、LLM、Ruby → Wasm、設定
order: 3
date: "2026-10-15"
summary: iPad 側の設計案。直結モードの頭 (STT / ローカル LLM / TTS)、ペアリングと設定、Ruby を Wasm にしてスタックちゃんへ送る仕組み。
---

`ios/` に置く iPad アプリの設計案。SwiftUI で作り、iPadOS 26 以降を対象にする。

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

| 機能 | 使うもの                                                       |
| ---- | -------------------------------------------------------------- |
| STT  | Speech フレームワーク (`SpeechAnalyzer` / `SpeechTranscriber`) |
| LLM  | Foundation Models フレームワーク (Apple のオンデバイス LLM)    |
| TTS  | `AVSpeechSynthesizer`                                          |

- Foundation Models が使えない iPad 向けに、MLX Swift や llama.cpp で小さな
  モデルを動かす選択肢を残す。
- LLM には「スタックちゃんらしさ」のシステムプロンプトと、表情タグ (`[happy]`
  など) の出し方を指示する。Worker 側と同じプロンプトを使い、どちらが頭でも
  同じキャラクターにする。
- STT は M5 から来た PCM を流し込む (マイクは M5 のものを使う)。
- TTS の音声は PCM にして M5 へ送り、M5 のスピーカーで鳴らす。
- 単独モードでネットが落ちたときも、同じ Wi-Fi にいれば iPad が頭を代われる。

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
