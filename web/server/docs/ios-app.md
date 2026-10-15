---
title: iPad アプリ — 音声、LLM、Ruby → Wasm
order: 3
date: "2026-10-15"
summary: iPad 側の設計案。STT / TTS / ローカル LLM、Cloudflare AI と Home Assistant、Ruby を Wasm にしてスタックちゃんへ送る仕組み。
---

`ios/` に置く iPad アプリの設計案。SwiftUI で作り、iPadOS 26 以降を対象にする。

## 画面

- **スタックちゃん**: 接続状態、会話ログ、押して話すボタン
- **接続**: スタックちゃんの AP につなぐ / Wi-Fi 設定 (スキャン結果から SSID
  を選びパスワードを入力)
- **挙動 (Ruby)**: Ruby エディタ、ビルド、アップロード、ロールバック
- **画像**: 写真やファイルから選んでアップロード
- **設定**: LLM の切り替え (ローカル / Cloudflare)、Home Assistant の URL
  とトークン

## 音声と LLM

すべてオンデバイスで完結できる構成を基本にし、ネットがあれば上位のものに
切り替える。

| 機能 | オンデバイス (基本)                                            | ネットあり (任意) |
| ---- | -------------------------------------------------------------- | ----------------- |
| STT  | Speech フレームワーク (`SpeechAnalyzer` / `SpeechTranscriber`) | Workers AI の STT |
| LLM  | Foundation Models フレームワーク (Apple のオンデバイス LLM)    | Workers AI の LLM |
| TTS  | `AVSpeechSynthesizer`                                          | Workers AI の TTS |

- Foundation Models が使えない iPad 向けに、MLX Swift や llama.cpp で小さな
  モデルを動かす選択肢を残す。
- LLM には「スタックちゃんらしさ」のシステムプロンプトと、表情タグ (`[happy]`
  など) の出し方を指示する。
- STT は M5 から来た PCM を流し込む。iPad のマイクを使うモードも用意する。
- TTS の音声は PCM にして M5 へ送り、M5 のスピーカーで鳴らす。

## LLM のバックエンドを切り替える

```swift
protocol ChatBackend {
  func reply(to history: [Message], tools: [Tool]) async throws -> Reply
}
// LocalBackend (Foundation Models) / CloudflareBackend / ...
```

- ネットにつながっていて Worker に届けば Cloudflare、そうでなければローカル。
- Home Assistant に届くときは、HA の操作を **tool** として LLM に渡す (例:
  `call_service(domain, service, entity_id)`, `get_state(entity_id)`)。

## Home Assistant

- REST API / WebSocket API を長期アクセストークンで呼ぶ。トークンは Keychain
  に保存。
- 届くかどうか (`/api/` への疎通) で「自宅モード」を判定する。
- 公開するエンティティは設定画面で絞る (LLM に家中を触らせない)。

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
