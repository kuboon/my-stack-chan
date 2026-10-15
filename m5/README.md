# m5 — スタックちゃん ファームウェア

対象は M5Stack 公式 StackChan (SKU: K151、CoreS3)。未着手。設計案は
[`web/server/docs/m5-firmware.md`](../web/server/docs/m5-firmware.md)。

やること:

- 顔（m5stack-avatar）、マイク / スピーカー、SCS0009 シリアルサーボ
- Wi-Fi: AP (`stackchan-XXXX`) → スキャン → STA 接続、mDNS (`stackchan.local`)
- 最初にハンドシェイクしたアプリとだけ共有鍵を持つ認証
- HTTP / WebSocket API（ペアリング、設定、Wasm と画像の更新、音声と制御）
- 単独モード: Cloudflare Worker と WebSocket で会話、LAN 内の Home Assistant
  を呼ぶ
- Wasm ランタイム（WAMR）。Ruby 処理系は載せない
- 不正な Wasm からのロールバック
