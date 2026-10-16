# ios — iPad アプリ

対象は A17 Pro の iPad。未着手。設計案は
[`web/server/docs/ios-app.md`](../web/server/docs/ios-app.md)。

やること:

- スタックちゃんの AP へ接続、同じ Wi-Fi では Bonjour (`_stackchan._tcp`) で探索
- ペアリング（X25519 + 確認コード）で共有鍵を作り、Keychain に保存
- 直結モードの会話: M5 からの音声 → STT → LLM → TTS → M5 へ（すべてオンデバイス:
  Speech (`SpeechAnalyzer`) / Foundation Models / `AVSpeechSynthesizer`）。 STT
  / LLM / TTS は protocol にして、あとで speech.cpp
  などに差し替えられるようにする
- スタックちゃんへの設定の書き込み: Wi-Fi（スキャン → SSID 選択 → パスワード）、
  Cloudflare Worker、Home Assistant
- Ruby → Wasm（mruby/c 系 VM.wasm + バイトコード埋め込み、WasmKit で実行）と
  スタックちゃんへのアップロード
- 画像のアップロード
