# m5 — スタックちゃん (M5Stack) ファームウェア

未着手。設計案は
[`web/server/docs/m5-firmware.md`](../web/server/docs/m5-firmware.md)。

やること:

- 顔（m5stack-avatar）、マイク / スピーカー、サーボ
- Wi-Fi: AP (`stackchan-XXXX`) → スキャン → STA 接続、mDNS (`stackchan.local`)
- HTTP / WebSocket API（Wi-Fi 設定、Wasm と画像の更新、音声と制御）
- Wasm ランタイム（WAMR 第一候補）。Ruby 処理系は載せない
- 不正な Wasm からのロールバック
