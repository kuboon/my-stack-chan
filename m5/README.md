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
- 決まった命令 (「右を向いて」「うなずいて」「LED を青にして」など)
  をスタックちゃんの中の KoeAct-JP
  で聞き取り、その場で実行する。アプリが無くても画面タップで聞く
  (下の「音声の命令」)
- アプリから家の Wi-Fi を設定すると、次からはそれにつなぐ
  (`stackchan-xxxx.local`、 `_stackchan._tcp`)。つながらなければ AP に戻る

## ビルドと書き込み

```sh
cd m5
pio run              # ビルド
pio run -t upload    # USB で書き込み
pio device monitor   # シリアルログ
```

PlatformIO Core 6.2.0 以上 (`pio upgrade`) と pioarduino (Arduino core 3.x)
を使う。ライブラリは GitHub の タグで固定している (`platformio.ini`)。

## 音声の命令 (KoeAct-JP) のモデルを書き込む

パーティション表 (`partitions.csv`) に `model` と `a2`
がある。ファームウェアを書き込んだあと、 一度だけモデルを書き込む (Hugging Face
から約 6MB を取得し、SHA-256 を確かめる):

```sh
pio run -t upload
m5/tools/flash_koeact.sh            # ポートを指定するなら m5/tools/flash_koeact.sh /dev/cu.usbmodemXXXX
```

書き込んでいなければ、音声の命令は自動で無効になり、ほかはこれまで通り動く。
シリアルログに `KoeAct: ready` が出れば有効。

**`default_16MB.csv` から `partitions.csv`
に変えたので、前のファームウェアから上げるときは アプリ領域の配置が変わる。**
NVS (ペアリングと家の Wi-Fi) は同じ場所なので消えない。

## テスト

```sh
m5/test/host/run.sh   # ペアリングの暗号と、命令のピース → JSON をホストで (libmbedtls-dev が要る)
```

`test/vectors/vectors.json` は `gen_vectors.py` が作り、iPad
アプリのテストも同じものを使う (`ios/StackChanKit/Tests/.../vectors.json`
にコピー。CI で一致を確かめる)。

## ファイル

| ファイル                | 役割                                         |
| ----------------------- | -------------------------------------------- |
| `src/main.cpp`          | 状態と画面の切り替え、タッチ、コマンドの処理 |
| `src/app_link.*`        | HTTP、ペアリング、WebSocket                  |
| `src/net.*`             | AP と家の Wi-Fi、スキャン、mDNS              |
| `src/camera.*`          | カメラで 1 枚撮って JPEG にする              |
| `src/pairing_crypto.*`  | ペアリングと認証の暗号 (mbedtls のみ)        |
| `src/audio.*`           | マイクの録音、受け取った音声の再生           |
| `src/screen.*`          | セットアップ画面とアバター                   |
| `src/settings.*`        | NVS に保存するもの                           |
| `src/commands.*`        | 音声の命令 (KoeAct-JP) を録音から聞き取る    |
| `src/action_pieces.*`   | 命令のデコーダの出力 → Action JSON           |
| `src/actions.*`         | 命令の実行 (首、表情、LED、音量、明るさ)     |
| `lib/koeact/`           | KoeAct-JP のデコーダと前処理 (Apache-2.0)    |
| `tools/flash_koeact.sh` | KoeAct-JP のモデルを取得して書き込む         |
