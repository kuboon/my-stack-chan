# my-stack-chan

スタックちゃん (M5Stack) に話しかけると、iPad 経由で返事をしてくれる。
挙動の一部は Ruby で書いて Wasm にし、iPad からスタックちゃんへ書き込める。

構想: [web/server/docs/concept.md](web/server/docs/concept.md)（GitHub Pages の
`/docs/concept` でも読める）

| ディレクトリ   | 中身                                                                                |
| -------------- | ----------------------------------------------------------------------------------- |
| [`web/`](web/) | 構想・設計ドキュメントのサイト（Deno + Remix v3、Markdown → GitHub Pages）          |
| [`ios/`](ios/) | iPad アプリ。STT / TTS / ローカル LLM、Ruby → Wasm、スタックちゃんへのアップロード  |
| [`m5/`](m5/)   | M5Stack ファームウェア。Wasm ランタイム、Wasm と画像を更新する HTTP API、Wi-Fi 制御 |

## Web（ドキュメント）

```bash
cd web
deno task dev    # http://localhost:8000
deno task build  # web/dist に静的サイトを生成
```

ドキュメントを足すには `web/server/docs/` に Markdown を置くだけ（front-matter
に `title` / `date` / `summary`、並び順を決めたいときは `order`）。
