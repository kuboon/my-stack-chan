import { css, type Handle } from "@remix-run/component";

import { routes } from "../routes.ts";
import { cardStyle } from "../theme.ts";

export const title = "my-stack-chan";
export const description =
  "M5Stack のスタックちゃんを iPad と Wasm で動かすプロジェクト。";

/** No island on this page, so it ships no JavaScript. */
export const hydrate = false;

export default function Home(_handle: Handle) {
  return () => (
    <>
      <h1>my-stack-chan</h1>
      <p>
        スタックちゃんに話しかけると返事をしてくれる。ネットが無い場所では iPad
        が、Wi-Fi があれば iPad なしで Cloudflare の AI が頭になる。
        挙動の一部は Ruby で書いて Wasm にし、iPad から書き込める。
      </p>

      <section mix={cardStyle}>
        <h2>構成</h2>
        <ul mix={listStyle}>
          <li>
            <code>web/</code>{" "}
            — このサイト。構想・設計ドキュメント (Markdown → GitHub Pages)
          </li>
          <li>
            <code>ios/</code>{" "}
            — iPad アプリ。STT / TTS / ローカル LLM、Ruby → Wasm
            コンパイル、スタックちゃんへのアップロード
          </li>
          <li>
            <code>m5/</code>{" "}
            — StackChan (K151) のファームウェア。Wasm
            ランタイム、Wasm・画像を更新する API、 Wi-Fi AP / STA
          </li>
        </ul>
      </section>

      <section mix={cardStyle}>
        <h2>ドキュメント</h2>
        <p>
          <a href={routes.docs.index.href()}>Docs</a>{" "}
          に構想と設計を置いています。
        </p>
      </section>
    </>
  );
}

const listStyle = css({
  paddingInlineStart: "1.25rem",
  "& li + li": { marginTop: "0.5rem" },
});
