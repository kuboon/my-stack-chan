# my-stack-chan

スタックちゃん (M5Stack) + iPad + Wasm のプロジェクト。全体像は
`web/server/docs/concept.md`。設計を変えたらまずそこ（と `m5-firmware.md` /
`ios-app.md`）を直す。

## 構成

- `web/` — ドキュメントサイト。kuboon/deno-remix-tmpl の blog 機能（Markdown →
  静的 HTML → GitHub Pages）だけを残したもの。Deno のルートは `web/`
  （`web/deno.json`）。
  - `web/server/docs/*.md` が記事。`web/server/docs/mod.tsx` が読む
  - 変更後は `cd web && deno task check && deno task test && deno task build`
  - 見た目は `@remix-run/component` の `css()` と `web/client/tokens.ts`
  - ブラウザへ渡るコードは `web/client/` に置き、`Deno.` を参照しない
- `ios/` — iPad アプリ（SwiftUI、未着手）
- `m5/` — StackChan (K151) のファームウェア（ESP-IDF / PlatformIO、未着手）

## 規約

- ドキュメントは日本語
- Deno: Web API 優先、TypeScript strict、テストは `Deno.test()` +
  `@std/assert`、 ファイル名はスネークケース
