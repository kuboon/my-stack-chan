---
title: M5 ファームウェア — Wasm ランタイム、API、単独モード
order: 2
date: "2026-10-15"
summary: スタックちゃん (M5) 側の設計案。ハードウェア、Wi-Fi の状態遷移、認証、HTTP / WebSocket API、単独モード、Wasm のホスト関数 (ABI)。
---

`m5/` に置くファームウェアの設計案。まだ案なので、作りながら直す。

## 土台

- 機種: M5Stack 公式 **StackChan (SKU: K151)**。本体は CoreS3 (ESP32-S3)
  - 顔: 2.0 インチのタッチ画面
  - 耳・口: マイク 2 つ、1 W スピーカー
  - 首: SCS0009 シリアルサーボ 2 つ (左右 yaw は 360° 連続回転、上下 pitch は
    90°)。位置のフィードバックあり。UART1 (GPIO6 TX / GPIO7 RX、1 Mbps 半二重)
    につながっているとされる
    ([Zephyr のボード資料](https://docs.zephyrproject.org/latest/boards/m5stack/m5stack_stackchan/doc/index.html)。
    M5Stack 公式資料で要確認)
  - ほか: カメラ、9 軸 IMU、近接・照度、RGB LED、赤外線、3 ゾーンのタッチ、NFC、
    microSD、バッテリー
  - 出荷時のファームウェアは使わず焼き替える
- フレームワーク: ESP-IDF (+ Arduino as component)、PlatformIO でビルド
- 顔: [m5stack-avatar](https://github.com/stack-chan/m5stack-avatar)
- Wasm ランタイム:
  [WAMR (WebAssembly Micro Runtime)](https://github.com/bytecodealliance/wasm-micro-runtime)。
  ESP-IDF を公式にサポートし、メンテナンスも続いている。インタプリタ
  (fast-interp) で動かす。

  > WAMR と wasm3 の違い: どちらも小さなマイコンで Wasm を動かすランタイム。
  > wasm3 はより小さく速いインタプリタだが、2022 年ごろから開発がほぼ
  > 止まっている。WAMR は少し大きいが、Bytecode Alliance (Wasmtime と同じ団体)
  > が開発を続けていて、ESP-IDF 向けのサンプルや資料もある。長く使うので WAMR
  > にする。

- 保存先: Wasm・画像・設定は LittleFS (大きいものは microSD)、Wi-Fi の認証情報・
  共有鍵・各種トークンは NVS (暗号化パーティション)

## Wi-Fi の状態

```
       起動
        │
保存済み Wi-Fi あり? ──no──▶ [AP]  stackchan-XXXX / 192.168.4.1
        │yes                    │ POST /wifi/connect
        ▼                       ▼
     [STA 接続中] ──失敗──▶ [AP] (失敗理由を iPad に返す)
        │成功
        ▼
     [STA]  単独モード。mDNS: stackchan.local / _stackchan._tcp
        │ 本体操作 or POST /wifi/forget
        ▼
       [AP]
```

- AP 中もスキャンできるよう、スキャンと接続試行は AP+STA モードで行う。
- いまの実装では、Wi-Fi の設定は下の HTTP API ではなく、認証した WebSocket の
  `wifi.scan` / `wifi.connect` / `wifi.forget` で行う
  ([プロトコル](./protocol))。STA 中も頭は同じ Wi-Fi の iPad で、Worker
  (単独モード) はまだない。
- AP のパスワードは端末ごとに生成して NVS に保存し、画面に表示 (QR も)。
- AP のときは iPad が頭 (直結モード)、STA のときは Cloudflare Worker が頭
  (単独モード)。

## API の認証

M5 の API を使えるのは、**最初にハンドシェイクしたアプリだけ**。

### ペアリング

1. M5 がペアリング済みでないときだけ `POST /pair` を受け付ける。
2. アプリと M5 が X25519 の公開鍵を交換し、ECDH で共有の秘密を作る。
3. 両方の画面に、両者の公開鍵から作った 6 桁の確認コードを表示する。ユーザーが
   一致を確かめ、M5 の画面をタッチして承認する。一致しなければ中止する
   (間に入られた場合はコードが食い違う)。
4. HKDF で共有の秘密から鍵 (署名用と暗号化用) を作り、M5 は NVS に、iPad は
   Keychain に保存する。
5. 以降 `POST /pair` は 403 を返す。

ペアリングを消すのは本体操作だけ (例: 胴体のタッチを長押ししながら起動)。API
からは消せない。

### リクエストの署名と暗号化

- すべての HTTP リクエストに、署名用の鍵による HMAC-SHA256 を付ける。署名する
  のは メソッド・パス・時刻・ノンス・ボディのハッシュ。M5 は時刻のずれと
  ノンスの再利用を拒否する (M5 は起動時に時刻を知らないことがあるので、
  `GET /pair/challenge` で M5 が出すチャレンジを使う方式も用意する)。
- WebSocket は接続時に同じ方式で認証する。
- Wi-Fi のパスワード、Worker の鍵、Home Assistant のトークンなど秘密を含む
  ボディは、暗号化用の鍵で AES-GCM 暗号化する。
- 平文の HTTP なので、盗聴されても困らない内容 (音声を除く) 以外は暗号化する
  ことを基本にする。音声も将来は暗号化を検討する。

## HTTP API (案)

ベース: AP 中は `http://192.168.4.1`、STA 中は `http://stackchan.local`

| メソッド | パス             | 内容                                              |
| -------- | ---------------- | ------------------------------------------------- |
| `GET`    | `/info`          | 機種、ファームウェアのバージョン、空き容量、状態  |
| `POST`   | `/pair`          | ペアリング (未ペアリングのときだけ)               |
| `PUT`    | `/config/worker` | Cloudflare Worker の URL とデバイス用の鍵         |
| `PUT`    | `/config/ha`     | Home Assistant の URL、トークン、使うエンティティ |
| `GET`    | `/wifi/scan`     | 周りの SSID 一覧 (RSSI、暗号化の有無)             |
| `POST`   | `/wifi/connect`  | `{ ssid, password }` で接続を試み、結果を返す     |
| `POST`   | `/wifi/forget`   | 保存した Wi-Fi を消して AP に戻る                 |
| `PUT`    | `/wasm`          | `behavior.wasm` を差し替える (本体はバイナリ)     |
| `POST`   | `/wasm/rollback` | ひとつ前の Wasm に戻す                            |
| `GET`    | `/images`        | 保存している画像の一覧                            |
| `PUT`    | `/images/{name}` | 画像 (PNG / JPEG) を保存                          |
| `DELETE` | `/images/{name}` | 画像を消す                                        |
| `GET`    | `/ws`            | WebSocket。音声と制御メッセージ (下記)            |

- `/info` (公開してよい項目だけ) と `/pair` 以外は、[認証](#api-の認証) の署名が
  必須。

## WebSocket `/ws`

直結モードで iPad が頭になるときに使う。単独モードで M5 が Worker に張る
WebSocket も、向きが逆なだけで同じメッセージを使う。

- **バイナリフレーム**: 音声。M5 → 頭はマイク、頭 → M5 は TTS の音声。16 kHz /
  16 bit / モノラル PCM。
- **テキストフレーム**: JSON の制御メッセージ。

```json
{ "type": "listen.start" }
{ "type": "listen.end" }
{ "type": "speak.start", "text": "こんにちは" }
{ "type": "speak.end" }
{ "type": "face", "expression": "happy" }
{ "type": "event", "name": "touch", "x": 120, "y": 80 }
{ "type": "tool.call", "id": "c1", "name": "ha.call_service", "args": { "domain": "light", "service": "turn_off", "entity_id": "light.living" } }
{ "type": "tool.result", "id": "c1", "ok": true, "result": {} }
```

## 単独モード

STA でつながっているときは、iPad なしで動く。

- 起動したら Worker (`wss://…`) に WebSocket でつなぎ、デバイス用の鍵で認証
  する。切れたら間隔を空けて再接続する。
- 会話: タッチ (のちに VAD) で録音を始め、PCM を Worker に送る。Worker が STT →
  LLM → TTS をして、音声と制御メッセージ (表情など) を返す。
- Home Assistant: Worker が `tool.call` を送ってきたら、M5 が LAN 内の HA の
  REST API を呼び、`tool.result` を返す。設定で許可したエンティティ以外は
  拒否する。HA の WebSocket API を購読し、決めた出来事を `event` として Wasm と
  Worker に渡す。
- Worker に届かないときは画面に表示する。同じ Wi-Fi の iPad が `/ws`
  につないできたら、iPad を頭にする。

## Wasm の ABI (案)

Wasm モジュールは 1 つ (`behavior.wasm`)。線形メモリは 1〜2 ページ (64〜128 KB)
程度を目安にし、上限をランタイム側で決める。

### M5 が提供する関数 (import、モジュール名 `stackchan`)

| 関数                          | 内容                              |
| ----------------------------- | --------------------------------- |
| `log(ptr, len)`               | ログ出力 (UTF-8)                  |
| `now_ms() -> i64`             | 起動からのミリ秒                  |
| `face_set(expression: i32)`   | 表情 (neutral / happy / sad / …)  |
| `gaze(x: f32, y: f32)`        | 視線                              |
| `servo_move(axis, angle, ms)` | 首を動かす (axis: 0=yaw, 1=pitch) |
| `image_show(ptr, len)`        | 保存済み画像を名前で表示          |
| `say(ptr, len)`               | 頭にしゃべる内容を依頼 (TTS 経由) |
| `send(ptr, len)`              | 頭へ任意の JSON を送る            |

### Wasm が提供する関数 (export)

| 関数                            | 呼ばれるとき                             |
| ------------------------------- | ---------------------------------------- |
| `init()`                        | 読み込み直後に 1 回                      |
| `on_tick(now_ms: i64)`          | 一定間隔 (例: 50 ms)                     |
| `on_event(kind: i32, ptr, len)` | タッチ、ボタン、発話開始/終了、受信 JSON |
| `alloc(len) -> ptr`             | M5 が文字列を渡すときの領域確保          |

- 1 回の呼び出しに時間制限 (命令数 or 時間) をかけ、超えたら止める。
- `init` が trap したら前の Wasm に自動で戻す。
