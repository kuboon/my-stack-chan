# koeact — KoeAct-JP のデコーダと前処理

[ayousanz/KoeAct-JP](https://huggingface.co/ayousanz/KoeAct-JP)
(Apache-2.0、revision `55ca64478dc146a2672e6fffe8f7799a4e85f41e`) の CoreS3
ファームウェアから、次のファイルを 持ってきたもの。`src/features.cpp`
だけ、フィルタバンクの重み (41 KB) を内部 RAM から PSRAM に移す変更を入れた
(ファイルの先頭に記載)。ほかは変更していない。帰属は `NOTICE`、ライセンスは
`LICENSE`。

| ファイル             | 元の場所                                  | SHA-256 (リリースの `SHA256SUMS` と一致) |
| -------------------- | ----------------------------------------- | ---------------------------------------- |
| `src/a2.c`           | `cores3/firmware/a2/a2.c`                 | `85daf6df…`                              |
| `src/a2.h`           | `cores3/firmware/a2/a2.h`                 | `7d470210…`                              |
| `src/a2_grammar.h`   | `cores3/firmware/a2/a2_grammar.h`         | `aa32ca12…`                              |
| `src/features.cpp`   | `cores3/firmware/e05/main/features.cpp`   | `9f75157b…` (変更前)                     |
| `src/features.h`     | `cores3/firmware/e05/main/features.h`     | `ea3b9a79…`                              |
| `src/vocabulary.inc` | `cores3/firmware/e05/main/vocabulary.inc` | `ca336617…`                              |

- `a2.*`: Ralomi の layer 4 / layer 8 から Action JSON のピース ID
  を出す、文法制約つきのデコーダ
- `features.*`: 16 kHz の音声 → 40 次元 log-Mel (ESP-DSP の FFT)
- `vocabulary.inc`: Ralomi の CTC の語彙 (かな 157 個)

モデル本体 (`ralomi_e13_stream40.espdl`、`a2_weights.bin`)
はここに入れない。`m5/tools/flash_koeact.sh` が同じ revision
から取得して、フラッシュの `model` / `a2` パーティションに書き込む。
これらを使うコードは `m5/src/commands.*`。
