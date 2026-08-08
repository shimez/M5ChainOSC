# AtomS3R Chain OSC Sender

M5Stack **AtomS3R** + **Atomic ToChain Base** 向けの、Chain デバイス入力を OSC で送信するファームウェアです。  
主に **VRChat** のアバターパラメータ操作を想定しています。

本リポジトリのコードは、対話型 AI **Grok（xAI）** との反復的な開発・デバッグを通じて作成・整理したものです。

---

## 概要

Chain バス上の入力デバイス（Key / Encoder / Angle / Joystick）をポーリングし、設定に応じた OSC メッセージを Wi-Fi 経由で送信します。

- 初回は **AP モード + キャプティブポータル** で Wi-Fi 設定
- 以降はブラウザから **OSC 送信先・デバイスごとのパラメータ** を設定
- 設定は **デバイス UID 単位** で NVS に保存（抜き差し・再接続後も復元）
- **ホットスワップ**対応（接続構成の変化を検出して再列挙）

---

## Demo

動作デモ（動画）:

1. [https://x.com/ctake_shimez/status/2084966417183740093](https://x.com/ctake_shimez/status/2084966417183740093)
2. [https://x.com/ctake_shimez/status/2084979076373401990](https://x.com/ctake_shimez/status/2084979076373401990)
3. [https://x.com/ctake_shimez/status/2084993484382232908](https://x.com/ctake_shimez/status/2084993484382232908)

関連ポストは [#M5ChainOSC](https://x.com/search?q=%23M5ChainOSC&src=hashtag_click) でまとめています。

---

## 対応ハードウェア

| 役割 | 製品 |
|------|------|
| メイン | [M5Stack AtomS3R](https://docs.m5stack.com/en/core/AtomS3R) |
| ベース | [Atomic ToChain Base](https://docs.m5stack.com/en/atom/Atomic%20ToChain%20Base) |
| 入力 | M5Chain 系デバイス（下記） |

### 対応 Chain デバイス

| タイプ | 送信内容 |
|--------|----------|
| **Key** | Press / Release、または Sequence（押すたびに値を増減） |
| **Encoder** | 回転量（Absolute / Increment）+ クリック（Press/Release または Sequence） |
| **Angle** | 角度 ADC（8bit / 12bit）をレンジマップして送信 |
| **Joystick** | X / Y（デッドバンド・Invert・レンジマップ）+ クリック |

---

## 主な機能

- **Wi-Fi**
  - 未設定時は AP（`AtomS3R-OSC` / `12345678`）で設定画面を表示
  - 接続後は mDNS（`atoms3r-osc.local`）でもアクセス可能
- **Web UI**
  - OSC Host / Port
  - デバイス名、Press・Release / Sequence、レンジマップなど
  - Joystick の **Invert X / Invert Y**
  - 画面回転（0° / 90° / 180° / 270°、現在値を `*` で表示）
  - 保存済みデバイスの一覧・削除（未接続でも削除可）
  - Wi-Fi 設定の削除
- **永続化**
  - デバイス設定は UID ベース（ハッシュキー）で保存
  - デバイス名は専用キーでも保存
- **ディスプレイ**
  - IP / ホスト名の常時表示
  - OSC 送信時に Address / Value を一時表示
  - 10 秒長押しで全設定リセット（円弧プログレス）
- **VRChat 向け**
  - Float / Int / String の値型
  - 入出力レンジのクランプ・マップ

---

## 必要なライブラリ（Arduino IDE / PlatformIO）

- [M5Unified](https://github.com/m5stack/M5Unified)
- [M5Chain](https://github.com/m5stack/M5Chain)（または M5Stack 公式の Chain 用ライブラリ）
- [ArduinoOSC](https://github.com/hideakitai/ArduinoOSC)（`ArduinoOSCWiFi`）
- ESP32 標準: `WiFi` / `WebServer` / `DNSServer` / `Preferences` / `ESPmDNS`

ボードは **M5Stack AtomS3** 系（ESP32-S3）を選択してください。

---

## セットアップ

1. 上記ライブラリをインストールする  
2. 本スケッチを AtomS3R に書き込む  
3. Atomic ToChain Base を装着し、Chain デバイスを接続する  
4. 初回起動
   - 本体が AP `AtomS3R-OSC` を出す
   - スマホ等で接続し、表示された設定ページで自宅 Wi-Fi を登録
5. 再起動後、同じネットワーク上のブラウザで  
   `http://<本体のIP>/` または `http://atoms3r-osc.local/` を開く  
6. **OSC Destination** と各デバイスのパラメータを設定し **Save All Settings**

---

## OSC の例（VRChat）

| 用途 | Address 例 | 値 |
|------|------------|-----|
| ボタン ON | `/avatar/parameters/Key` | `1.0` (Float) |
| ボタン OFF | `/avatar/parameters/Key` | `0.0` |
| ジョイスティック X | `/avatar/parameters/JoyX` | `-1.0` ～ `1.0` |
| エンコーダ | `/avatar/parameters/Encoder` | マップ後の Float / Int |

VRChat 側では OSC を有効にし、パラメータ名を合わせてください。

---

## 操作メモ

- **画面長押し（約 10 秒）** … 全設定（Wi-Fi / OSC / デバイス）を消去して再起動  
- Chain デバイスの **抜き差し** … 自動で再検出。保存済み UID の設定は再接続時に復元  
- **Device Name** … Web UI で設定した名前が、本体画面の OSC フィードバックにも使われます  

---

## ピン（ToChain Base）

スケッチ内の UART 定義:

```cpp
#define RXD_PIN GPIO_NUM_6
#define TXD_PIN GPIO_NUM_5
```
Atomic ToChain Base 利用時の一般的な配線に合わせています。別ベースを使う場合はデータシートに合わせて変更してください。

## 開発について
本ファームウェアは、M5Stack Chain の挙動確認・NVS 永続化・ホットスワップ・Web UI・VRChat 向けレンジ処理など、実機フィードバックを踏まえた修正を繰り返し、Grok（xAI） が中心となってコードを生成・統合したものです。

不具合報告や改善案は Issue で歓迎します。

---

## ライセンス

MIT License

利用する各ライブラリ（M5Unified / M5Chain / ArduinoOSC 等）のライセンスも併せて確認してください。

---

## 参考リンク

- [M5Stack Docs](https://docs.m5stack.com/)
- [VRChat OSC](https://docs.vrchat.com/docs/osc-overview)
- [Grok / xAI](https://x.ai/)
```

