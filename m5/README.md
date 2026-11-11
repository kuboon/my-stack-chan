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

KoeAct-JP を速く動かすため、ESP-DL を 3.3.13 に、キャッシュを 64KB にして
Arduino のライブラリをビルドし直す (`platformio.ini` の `custom_component_add` /
`custom_sdkconfig`)。最初のビルドは数分かかり、
`~/.platformio/packages/framework-arduinoespressif32-libs` を書き換える。Arduino
同梱の ESP-DL 3.3.11 では、0.4 秒の区切りに 1.4 秒かかって 録音に追いつかない
(3.3.13 では 0.26 秒)。

最初のビルドで `IncompatiblePlatform: ... PlatformIO Core v6.2.0-post.1`
と出て止まるときは、 pioarduino の Python 環境に 6.2.0.post1 が入っている
(上流の不具合)。`constraints.txt` で 6.2.0 に固定してビルドし直す:

```sh
rm -rf ~/.platformio/penv
UV_CONSTRAINT=$PWD/constraints.txt pio run
```

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

## Wi-Fi で更新する (OTA)

ファームウェアの置き場所が 2 つあるパーティション表 (`partitions.csv`)
になってからは、 USB なしで更新できる。iPad
アプリの「…」→「ファームウェアの更新…」で `firmware.bin` を選ぶ。`firmware.bin`
は GitHub Actions の m5 ワークフローの成果物 `firmware`、 または手元の `pio run`
が作る `.pio/build/stackchan/firmware.bin`。

新しい版が 30 秒動けばそれに決まり、それより前に止まれば前の版に戻る。

### Mac から署名つきで更新する

iPad アプリを通さずに、署名した `firmware.bin` を Mac から送れる
([プロトコル](../web/server/docs/protocol.md))。最初に 1 回だけ、署名鍵を作って公開鍵を
入れた版を USB で書く:

```sh
m5/tools/ota_keygen.sh      # 鍵を ~/.config/stackchan/ota_signing.pem に作り、src/ota_pubkey.h を書く
cd m5 && pio run -t upload  # 公開鍵の入った版を USB で 1 回だけ
```

そのあとは、ビルドして送るだけ:

```sh
cd m5 && pio run
m5/tools/ota_push.sh stackchan-ab12.local   # 署名して POST /ota/signed
```

- `src/ota_pubkey.h` (公開鍵) はコミットしてよい。秘密鍵 (`ota_signing.pem`) は
  コミットしない。なくすと、署名つきの更新はできなくなる (iPad アプリの OTA と USB は使える)。
- 鍵を作り直したら、公開鍵の入った版をもう一度 USB (か iPad の OTA) で入れる。

**パーティション表を変えたときは、1 回だけ USB で書き込み、モデルも書き直す**
(`model` / `a2` の位置が変わるため):

```sh
pio run -t upload
m5/tools/flash_koeact.sh
```

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
