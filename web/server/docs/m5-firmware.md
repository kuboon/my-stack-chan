---
title: M5 ファームウェア — Wasm ランタイムと API
order: 2
date: "2026-10-15"
summary: スタックちゃん (M5) 側の設計案。Wi-Fi の状態遷移、HTTP / WebSocket API、Wasm のホスト関数 (ABI)。
---

`m5/` に置くファームウェアの設計案。まだ案なので、作りながら直す。

## 土台

- 機種: M5Stack CoreS3 (ESP32-S3、PSRAM 8 MB) を第一候補
- フレームワーク: ESP-IDF (+ Arduino as component)、PlatformIO でビルド
- 顔: [m5stack-avatar](https://github.com/stack-chan/m5stack-avatar)
- Wasm ランタイム:
  [WAMR (WebAssembly Micro Runtime)](https://github.com/bytecodealliance/wasm-micro-runtime)
  を第一候補。ESP-IDF を公式にサポートし、メンテナンスも続いている。
  インタプリタ (fast-interp) で動かす。wasm3 は軽いが開発が止まっているので
  次点。
- 保存先: Wasm・画像・設定は LittleFS、Wi-Fi の認証情報は NVS

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
     [STA]  mDNS: stackchan.local / _stackchan._tcp
        │ ボタン長押し or POST /wifi/forget
        ▼
       [AP]
```

- AP 中もスキャンできるよう、スキャンと接続試行は AP+STA モードで行う。
- AP のパスワードは端末ごとに生成して NVS に保存し、画面に表示 (QR も)。

## HTTP API (案)

ベース: AP 中は `http://192.168.4.1`、STA 中は `http://stackchan.local`

| メソッド | パス             | 内容                                             |
| -------- | ---------------- | ------------------------------------------------ |
| `GET`    | `/info`          | 機種、ファームウェアのバージョン、空き容量、状態 |
| `GET`    | `/wifi/scan`     | 周りの SSID 一覧 (RSSI、暗号化の有無)            |
| `POST`   | `/wifi/connect`  | `{ ssid, password }` で接続を試み、結果を返す    |
| `POST`   | `/wifi/forget`   | 保存した Wi-Fi を消して AP に戻る                |
| `PUT`    | `/wasm`          | `behavior.wasm` を差し替える (本体はバイナリ)    |
| `POST`   | `/wasm/rollback` | ひとつ前の Wasm に戻す                           |
| `GET`    | `/images`        | 保存している画像の一覧                           |
| `PUT`    | `/images/{name}` | 画像 (PNG / JPEG) を保存                         |
| `DELETE` | `/images/{name}` | 画像を消す                                       |
| `GET`    | `/ws`            | WebSocket。音声と制御メッセージ (下記)           |

- 認証: 初回ペアリングで M5 がトークンを発行し (画面に確認コードを表示)、以降は
  `Authorization: Bearer` を必須にする。
- ブラウザからも叩けるよう CORS を許可する (トークンは必須のまま)。

## WebSocket `/ws`

- **バイナリフレーム**: 音声。M5 → iPad はマイク、iPad → M5 は TTS の音声。 16
  kHz / 16 bit / モノラル PCM。
- **テキストフレーム**: JSON の制御メッセージ。

```json
{ "type": "listen.start" }
{ "type": "listen.end" }
{ "type": "speak.start", "text": "こんにちは" }
{ "type": "speak.end" }
{ "type": "face", "expression": "happy" }
{ "type": "event", "name": "touch", "x": 120, "y": 80 }
```

## Wasm の ABI (案)

Wasm モジュールは 1 つ (`behavior.wasm`)。線形メモリは 1〜2 ページ (64〜128 KB)
程度を目安にし、上限をランタイム側で決める。

### M5 が提供する関数 (import、モジュール名 `stackchan`)

| 関数                          | 内容                                 |
| ----------------------------- | ------------------------------------ |
| `log(ptr, len)`               | ログ出力 (UTF-8)                     |
| `now_ms() -> i64`             | 起動からのミリ秒                     |
| `face_set(expression: i32)`   | 表情 (neutral / happy / sad / …)     |
| `gaze(x: f32, y: f32)`        | 視線                                 |
| `servo_move(axis, angle, ms)` | 首を動かす (axis: 0=パン, 1=チルト)  |
| `image_show(ptr, len)`        | 保存済み画像を名前で表示             |
| `say(ptr, len)`               | iPad にしゃべる内容を依頼 (TTS 経由) |
| `send(ptr, len)`              | iPad へ任意の JSON を送る            |

### Wasm が提供する関数 (export)

| 関数                            | 呼ばれるとき                             |
| ------------------------------- | ---------------------------------------- |
| `init()`                        | 読み込み直後に 1 回                      |
| `on_tick(now_ms: i64)`          | 一定間隔 (例: 50 ms)                     |
| `on_event(kind: i32, ptr, len)` | タッチ、ボタン、発話開始/終了、受信 JSON |
| `alloc(len) -> ptr`             | M5 が文字列を渡すときの領域確保          |

- 1 回の呼び出しに時間制限 (命令数 or 時間) をかけ、超えたら止める。
- `init` が trap したら前の Wasm に自動で戻す。
