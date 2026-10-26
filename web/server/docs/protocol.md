---
title: 直結モードのプロトコル
order: 4
date: "2026-10-16"
summary: スタックちゃん (M5) と iPad アプリの間のやり取りの仕様。AP と家の Wi-Fi、ペアリング、WebSocket の認証、音声と制御メッセージ。
---

直結モード (iPad が頭) で M5 と iPad アプリが話す内容の仕様。M5 が AP
になってそこに iPad がつなぐときも、M5 と iPad が同じ家の Wi-Fi
にいるときも同じ。**ファームウェア (`m5/`) とアプリ (`ios/`)
はこの文書に合わせる。** 変えるときはここを先に直す。

## AP

- SSID: `stackchan-XXXX` (`XXXX` は MAC アドレスの下位 2 バイトの 16
  進数、大文字)
- 暗号化: WPA2-PSK。パスワードは初回起動時に英数字 10 文字をランダムに作り、 NVS
  に保存する。
- M5 の画面に SSID・パスワードと、iPad のカメラで読める Wi-Fi の QR コード
  (`WIFI:T:WPA;S:<SSID>;P:<パスワード>;;`) を出す。
- M5 のアドレスは `192.168.4.1`。HTTP と WebSocket はポート 80。

## 家の Wi-Fi (STA)

iPad アプリから家の Wi-Fi を設定すると ([Wi-Fi の設定](#wi-fi-の設定))、M5
はそれにつなぐ。iPad も同じ Wi-Fi にいれば、iPad はインターネットにつながった
まま頭になれる。

- 起動したとき、保存した Wi-Fi があればそれにつなぐ (最長 20 秒待つ)。
  つながったら AP は立てない。つながらなければ AP を立てる。保存した Wi-Fi
  は消さず、次に起動したときまた試す。
- ホスト名は `stackchan-xxxx` (AP の SSID を小文字にしたもの)。mDNS で
  `stackchan-xxxx.local` と、サービス `_stackchan._tcp` (ポート 80、
  インスタンス名は `stackchan-XXXX`) を出す。
- HTTP と WebSocket は AP のときと同じ。ペアリングも同じ手順でできる (承認は M5
  の画面でするので、同じ Wi-Fi のほかの機械が勝手にはできない)。
- 一度切れても、M5 は自分でつなぎ直す。

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
  "paired": false,
  "wifi": "ap"
}
```

`wifi` は `ap` (自分が AP) か `sta` (家の Wi-Fi につながっている)。

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

| `type`         | 内容                                                                                  |
| -------------- | ------------------------------------------------------------------------------------- |
| `listen.start` | 録音を始めた。この後にマイクの音が続く                                                |
| `listen.end`   | 録音を終えた。アプリは認識を確定させ、返事を作る (「スタックちゃんで聞き取った命令」) |
| `speak.done`   | 受け取った音声を鳴らし終えた                                                          |
| `touch`        | 胴体のタッチ (`"gesture": "tap" \| "swipe_forward" \| "swipe_backward"`)              |
| `photo`        | `photo.take` に答えて撮った写真 (「写真」を見る)                                      |

アプリ → M5

| `type`         | 内容                                                                                                                                        |
| -------------- | ------------------------------------------------------------------------------------------------------------------------------------------- |
| `listen.start` | 録音を始めさせる (アプリの「押して話す」ボタン)                                                                                             |
| `listen.end`   | 録音を止めさせる                                                                                                                            |
| `speak.start`  | この後に音声が続く。`"text"` があれば吹き出しに出す                                                                                         |
| `speak.end`    | 音声を送り終えた。M5 は鳴らし終えたら `speak.done` を返す                                                                                   |
| `face`         | 表情。`"expression": "neutral" \| "happy" \| "sad" \| "angry" \| "doubt" \| "sleepy"`                                                       |
| `look`         | 首を動かす。`"yaw"` (-1.0〜1.0、正がスタックちゃんから見て右)、`"pitch"` (0.0〜1.0、大きいほど上)。この後 30 秒は M5 が自分で首を動かさない |
| `state`        | アプリの状態を顔に出す。`"state": "idle" \| "thinking"`                                                                                     |
| `photo.take`   | カメラで写真を撮らせる。`"size": "vga" \| "qvga"` (省略時 `vga`)。M5 は `photo` で答える                                                    |

### 録音の始め方と止め方

- M5 の画面をタップすると録音を始め、もう一度タップすると止める。最長 15 秒で
  自動で止める。
- アプリから `listen.start` / `listen.end` でも同じことができる。
- アプリは届いた音の大きさで話し終わりを見つけ (声のあと 0.8 秒静か)、
  `listen.end` を送って止める。6 秒たっても声がなければ、そこで止める。
- しゃべっている間 (`speak.start` から `speak.done` まで) は録音しない (CoreS3
  はマイクとスピーカーを同時に使えない)。

### スタックちゃんで聞き取った命令

M5 に KoeAct-JP のモデルが書き込まれていれば (`m5/tools/flash_koeact.sh`)、M5
は録音を自分でも聞き、「右を向いて」「うなずいて」のような決まった命令かどうかを
判断する ([M5 ファームウェア](./m5-firmware#音声の命令-))。そのとき `listen.end`
は結果を待って (話し終わりから 0.5 秒ほど、長くても 3 秒) 送り、次を付ける。

```json
{
  "type": "listen.end",
  "actions": [
    { "name": "look", "arguments": { "direction": "right", "degrees": 45 } }
  ],
  "accepted": true,
  "p_act": 0.996,
  "kana": "みぎをむいて"
}
```

- `actions`: KoeAct-JP の Action スキーマ v1 の呼び出し (最大 2
  つ)。命令でなければ `[]`。
- `accepted`: `p_act` が閾値 (0.97) 以上で、`actions` が空でない。**true のとき
  M5 はもう `actions` を実行している**。アプリはこの録音に返事をしない。
- `p_act`: 命令である確率。
- `kana`: 同じモデルの文字起こし (ひらがな、漢字なし)。粗いので参考。
- モデルの無い M5 は、これまで通り `type` だけの `listen.end` を送る。

アプリがつながっていないときも、M5 の画面をタップすると顔を出して命令だけを聞く
(アプリへは何も送らない)。

### 写真

アプリが `photo.take` を送ると、M5 は本体のカメラ (GC0308、0.3 MP) で 1 枚撮り、
JPEG を Base64 にして `photo` で返す。大きさは `size` で選ぶ: `vga` は 640×480、
`qvga` は 320×240。

```json
{
  "type": "photo",
  "width": 640,
  "height": 480,
  "ms": {
    "init": 120,
    "warmup": 400,
    "shot": 60,
    "jpeg": 300,
    "frames": [60, 62, 61]
  },
  "missed": 0,
  "jpeg": "/9j/4AAQ…"
}
```

`ms` と `missed` は時間がかかるわけを調べるための値で、アプリは表示するだけ。
`ms` はカメラの起動、露出が落ち着くまで捨てるフレーム、撮る 1 枚、JPEG
への変換にかかったミリ秒。`frames` はフレームごとの待ち時間
(捨てたもの、最後が撮った 1 枚)。`missed` はカメラから届かなかったフレームの数
(1 つにつき 4 秒待つ)。

撮れなかったときは `jpeg` の代わりに `error` を返す。

```json
{ "type": "photo", "error": "busy" }
```

| `error`  | 理由                                         |
| -------- | -------------------------------------------- |
| `busy`   | 録音中かしゃべっている間で、撮れない         |
| `camera` | カメラが動かない、または JPEG にできなかった |

- 撮っている間 (1 秒ほど) は M5 のほかの処理が止まるので、録音中としゃべっている
  間は撮らない。撮っている間、M5 の顔は吹き出しで「とるよ〜」と出し、アバターの
  動きを止める (アバターの描画とカメラの DMA が同じ PSRAM を使うため)。
- 写真はバイナリフレームでは送らない (バイナリフレームは音声だけ)。

### Wi-Fi の設定

アプリ → M5

| `type`         | 内容                                                       |
| -------------- | ---------------------------------------------------------- |
| `wifi.scan`    | 周りの Wi-Fi を探させる。M5 は `wifi.networks` で答える    |
| `wifi.connect` | 家の Wi-Fi につながせる。M5 は `wifi.result` で答える      |
| `wifi.forget`  | 保存した Wi-Fi を消させる。M5 は再起動して AP で立ち上がる |

M5 → アプリ

```json
{ "type": "wifi.networks", "networks": [{ "ssid": "home", "rssi": -52, "secure": true }] }
{ "type": "wifi.result", "ok": true, "ssid": "home", "ip": "192.168.1.23" }
{ "type": "wifi.result", "ok": false, "error": "wrong_password" }
```

- `wifi.networks` は SSID ごとに 1 つ (いちばん強いもの)、強い順に最大 20。 SSID
  の無いものは除く。
- `wifi.result` の `error`: `wrong_password`、`not_found` (その SSID が無い)、
  `timeout` (15 秒でつながらない)、`bad_request` (パスワードを開けない など)。
- つながったら、M5 は Wi-Fi を NVS に保存し、`wifi.result` を送って 3
  秒後に再起動し、家の Wi-Fi で立ち上がる。
- つなぐのを試す間、M5 の AP のチャンネルが家の Wi-Fi に合わせて変わるので、
  iPad との接続が切れることがある。M5 は結果を画面にも出す。

`wifi.connect` のパスワードは共有鍵から作った鍵で暗号化して送る。

```json
{
  "type": "wifi.connect",
  "ssid": "home",
  "nonce": "AAECAwQFBgcICQoL",
  "password": "…"
}
```

- 鍵:
  `HKDF-SHA256(IKM = 共有鍵, salt = なし, info = "stackchan-v1-wifi", L = 32)`
- `password`: `AES-256-GCM(鍵, nonce, 平文 = パスワード, AAD = SSID)` の暗号文の
  後ろに 16 バイトのタグをつけたもの。`nonce` は 12 バイトの乱数。
- パスワードの無い Wi-Fi は空の文字列を暗号化する。
- テストベクタは `m5/test/vectors/vectors.json` の `wifi_*`。

### 会話の流れ

```
M5: listen.start → PCM … → listen.end (+ 聞き取った命令)
M5: accepted なら命令を実行して終わり
アプリ: そうでなければ state(thinking) → STT → LLM → 文ごとに TTS
アプリ: face(happy)? → speak.start(text) → PCM … → speak.end
M5: 鳴らし終えたら speak.done
アプリ: state(idle)
```
