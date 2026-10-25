# m5 — スタックちゃん ファームウェア

対象は M5Stack 公式 StackChan (SKU: K151、CoreS3)。設計は
[`web/server/docs/m5-firmware.md`](../web/server/docs/m5-firmware.md)、iPad
アプリとのやり取りは
[`web/server/docs/protocol.md`](../web/server/docs/protocol.md)。

## いまできること (直結モード)

- AP `stackchan-XXXX` を立て、Wi-Fi の QR コードとパスワードを画面に出す
- iPad アプリとペアリング (X25519 + 6 桁の確認コードを画面で承認)。共有鍵は NVS
- 認証した WebSocket で、マイクの音を iPad へ送り、iPad から来た音声を鳴らす
- アバター (表情・口パク・吹き出し)、首のサーボ、胴体のタッチ
- 画面タップで録音開始/停止、画面 10 秒長押しでペアリング解除
- アプリから `photo.take` でカメラ (GC0308) で撮り、JPEG で返す
- アプリから家の Wi-Fi を設定すると、次からはそれにつなぐ (`stackchan-xxxx.local`、
  `_stackchan._tcp`)。つながらなければ AP に戻る

## ビルドと書き込み

```sh
cd m5
pio run              # ビルド
pio run -t upload    # USB で書き込み
pio device monitor   # シリアルログ
```

PlatformIO Core 6.2.0 以上 (`pio upgrade`) と pioarduino (Arduino core 3.x) を使う。ライブラリは GitHub の
タグで固定している (`platformio.ini`)。

## テスト

```sh
m5/test/host/run.sh   # ペアリングの暗号をホストで (libmbedtls-dev が要る)
```

`test/vectors/vectors.json` は `gen_vectors.py` が作り、iPad
アプリのテストも同じものを使う (`ios/StackChanKit/Tests/.../vectors.json`
にコピー。CI で一致を確かめる)。

## ファイル

| ファイル               | 役割                                         |
| ---------------------- | -------------------------------------------- |
| `src/main.cpp`         | 状態と画面の切り替え、タッチ、コマンドの処理 |
| `src/app_link.*`       | HTTP、ペアリング、WebSocket                  |
| `src/net.*`            | AP と家の Wi-Fi、スキャン、mDNS              |
| `src/camera.*`         | カメラで 1 枚撮って JPEG にする              |
| `src/pairing_crypto.*` | ペアリングと認証の暗号 (mbedtls のみ)        |
| `src/audio.*`          | マイクの録音、受け取った音声の再生           |
| `src/screen.*`         | セットアップ画面とアバター                   |
| `src/settings.*`       | NVS に保存するもの                           |
