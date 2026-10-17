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
- `ios/` — iPad アプリ（SwiftUI、XcodeGen）。端末の要らない部分は
  `ios/StackChanKit`（`swift test`）
- `m5/` — StackChan (K151) のファームウェア（PlatformIO + pioarduino、Arduino
  core 3.x、公式
  StackChan-BSP）。暗号はホストでテストできる（`m5/test/host/run.sh`）
- M5 と iPad のやり取りは `web/server/docs/protocol.md`
  が仕様。両方を変えるときはここを先に直す。ペアリングの暗号は
  `m5/test/vectors/vectors.json` で両側をテストする

## 規約

- ドキュメントは日本語
- Deno: Web API 優先、TypeScript strict、テストは `Deno.test()` +
  `@std/assert`、 ファイル名はスネークケース
