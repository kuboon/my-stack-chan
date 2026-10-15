# ios — iPad アプリ

未着手。設計案は [`web/server/docs/ios-app.md`](../web/server/docs/ios-app.md)。

やること:

- スタックちゃんの AP へ接続、同じ Wi-Fi では Bonjour (`_stackchan._tcp`) で探索
- 会話: M5 からの音声 → STT → LLM → TTS → M5 へ
  - オンデバイス: Speech (`SpeechAnalyzer`) / Foundation Models /
    `AVSpeechSynthesizer`
  - ネットあり: Cloudflare Workers AI（自分の Worker 経由）
  - 自宅: Home Assistant を LLM の tool として呼ぶ
- Ruby → Wasm（mruby/c 系 VM.wasm + バイトコード埋め込み、WasmKit で実行）と
  スタックちゃんへのアップロード
- 画像のアップロード
- スタックちゃんの Wi-Fi 設定（スキャン → SSID 選択 → パスワード入力）
