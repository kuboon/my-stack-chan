---
title: 直結モードのプロトコル
order: 4
date: "2026-10-16"
summary: スタックちゃん (M5) と iPad アプリの間のやり取りの仕様。AP、ペアリング、WebSocket の認証、音声と制御メッセージ。
---

直結モード (スタックちゃんが AP、iPad が頭) で M5 と iPad アプリが話す内容の
仕様。**ファームウェア (`m5/`) とアプリ (`ios/`) はこの文書に合わせる。**
変えるときはここを先に直す。

## AP

- SSID: `stackchan-XXXX` (`XXXX` は MAC アドレスの下位 2 バイトの 16
  進数、大文字)
- 暗号化: WPA2-PSK。パスワードは初回起動時に英数字 10 文字をランダムに作り、 NVS
  に保存する。
- M5 の画面に SSID・パスワードと、iPad のカメラで読める Wi-Fi の QR コード
  (`WIFI:T:WPA;S:<SSID>;P:<パスワード>;;`) を出す。
- M5 のアドレスは `192.168.4.1`。HTTP と WebSocket はポート 80。

## HTTP

| メソッド | パス           | 認証           | 内容                               |
| -------- | -------------- | -------------- | ---------------------------------- |
| `GET`    | `/info`        | 不要           | 機種、バージョン、ペアリング済みか |
| `POST`   | `/pair/start`  | 不要           | ペアリングを始める                 |
| `GET`    | `/pair/status` | 不要           | ペアリングの承認待ちの状態         |
| `GET`    | `/ws`          | (接続後に認証) | WebSocket                          |

ボディは JSON。バイナリは標準の Base64 (パディングあり) で書く。

### `GET /info`

```json
{
  "name": "stackchan-1A2B",
  "model": "K151",
  "firmware": "0.1.0",
  "paired": false
}
```

### ペアリング

```
アプリ                                   M5
  │ POST /pair/start { "public_key": A }    │  未ペアリングでなければ 409
  │────────────────────────────────────────▶│  鍵ペアを作り、確認コードを画面に出す
  │◀────────────────────────────────────────│ { "public_key": D }
  │  確認コードを画面に出す                  │
  │                                         │  ユーザーがコードを見比べ、M5 の画面をタッチ
  │ GET /pair/status (1 秒ごと)             │
  │────────────────────────────────────────▶│ { "state": "waiting" | "approved" | "rejected" }
```

- `A` はアプリ、`D` は M5 の X25519 公開鍵 (32 バイト、RFC 7748 の形)。
- 確認コード: `SHA-256("stackchan-pair-v1" ‖ D ‖ A)` の先頭 4 バイトを
  ビッグエンディアンの符号なし整数にして 1,000,000 で割った余りを、0 埋めの 6
  桁にする。
- 共有鍵:
  `HKDF-SHA256(IKM = X25519(自分の秘密鍵, 相手の公開鍵), salt = A ‖ D,
  info = "stackchan-v1-auth", L = 32)`。
- M5 は画面に「承認」と「やめる」を出す。承認すると共有鍵を NVS に保存し、
  `state` は `approved` になる。やめるか 60 秒たつと `rejected` になり、作った
  鍵は捨てる。
- アプリは `approved` を見てから共有鍵を Keychain に保存する。
- 一度ペアリングすると、ペアリングを消すまで `POST /pair/start` は 409 を返す。
  ペアリングを消すには、M5 の画面を 10 秒長押しする。

文字列はすべて UTF-8 のバイト列として連結する。テストベクタは
`m5/test/vectors/vectors.json` (`gen_vectors.py` が作る)。

## WebSocket `/ws`

同時に接続できるのは 1 つだけ。新しい接続が認証に成功したら古い接続は閉じる。

### 認証

```
M5 → { "type": "hello", "nonce": N }          N は 16 バイトの乱数
アプリ → { "type": "auth", "mac": M }          M = HMAC-SHA256(共有鍵, "stackchan-ws-v1" ‖ N)
M5 → { "type": "ready", "name": "stackchan-1A2B" }   失敗なら閉じる
```

`ready` の前に届いたほかのメッセージは無視する。5 秒以内に `auth`
が来なければ閉じる。

### 音声

- バイナリフレームは音声だけ。16 kHz / 16 bit / モノラル、リトルエンディアンの
  PCM。
- M5 → アプリ: `listen.start` と `listen.end` の間のマイクの音。
- アプリ → M5: `speak.start` と `speak.end` の間の、しゃべる音。

### 制御メッセージ (テキストフレーム、JSON)

M5 → アプリ

| `type`         | 内容                                                                     |
| -------------- | ------------------------------------------------------------------------ |
| `listen.start` | 録音を始めた。この後にマイクの音が続く                                   |
| `listen.end`   | 録音を終えた。アプリは認識を確定させ、返事を作る                         |
| `speak.done`   | 受け取った音声を鳴らし終えた                                             |
| `touch`        | 胴体のタッチ (`"gesture": "tap" \| "swipe_forward" \| "swipe_backward"`) |

アプリ → M5

| `type`         | 内容                                                                                  |
| -------------- | ------------------------------------------------------------------------------------- |
| `listen.start` | 録音を始めさせる (アプリの「押して話す」ボタン)                                       |
| `listen.end`   | 録音を止めさせる                                                                      |
| `speak.start`  | この後に音声が続く。`"text"` があれば吹き出しに出す                                   |
| `speak.end`    | 音声を送り終えた。M5 は鳴らし終えたら `speak.done` を返す                             |
| `face`         | 表情。`"expression": "neutral" \| "happy" \| "sad" \| "angry" \| "doubt" \| "sleepy"` |
| `look`         | 首を動かす。`"yaw"` (-1.0〜1.0)、`"pitch"` (0.0〜1.0)                                 |
| `state`        | アプリの状態を顔に出す。`"state": "idle" \| "thinking"`                               |

### 録音の始め方と止め方

- M5 の画面をタップすると録音を始め、もう一度タップすると止める。最長 15 秒で
  自動で止める。
- アプリから `listen.start` / `listen.end` でも同じことができる。
- しゃべっている間 (`speak.start` から `speak.done` まで) は録音しない (CoreS3
  はマイクとスピーカーを同時に使えない)。

### 会話の流れ

```
M5: listen.start → PCM … → listen.end
アプリ: state(thinking) → STT → LLM → 文ごとに TTS
アプリ: face(happy)? → speak.start(text) → PCM … → speak.end
M5: 鳴らし終えたら speak.done
アプリ: state(idle)
```
