# M5ChainOSC

M5Stack AtomS3RとAtomic ToChain Baseを使い、M5Stack Chainデバイスの操作をOSCメッセージとして送信するファームウェアです。ブラウザーから送信先やデバイスごとの動作を設定でき、設定はChainデバイスのUID単位で本体へ保存されます。

主にVRChatのアバターパラメーター操作を想定していますが、OSCを受信できるアプリケーションで利用できます。

> [!NOTE]
> Chain ToF対応は実装されていますが、このブランチでは実機検証中の機能として扱っています。

## 主な機能

- Web UIによる設定
- Chainデバイスの接続・取り外し・再接続の検出
- UID単位の設定保存と再接続時の復元
- OSC値の`Float`、`Int`、`String`送信
- Key、Encoderクリック、Joystickクリックから複数OSCメッセージを送信
- Press／Release合計8件まで追加・削除・並べ替え
- Press／Releaseを0件にして「何も送信しない」設定
- ボタンを押すたびに値を進めるSequenceモード
- 設定のJSONエクスポート／インポート
- 画面回転
- Arduino IDEとPlatformIOの両方に対応

## 対応ハードウェア

- M5Stack AtomS3R
- M5Stack Atomic ToChain Base
- Chain Key
- Chain Encoder
- Chain Angle
- Chain Joystick
- Chain ToF（実機検証中）

## 必要なライブラリ

- M5Unified
- M5Chain
- ArduinoOSC
- ArduinoJson 6.x

PlatformIOでは`platformio.ini`の`lib_deps`から自動取得されます。Arduino IDEではライブラリマネージャーなどから事前にインストールしてください。

## 最短セットアップ

1. ファームウェアをAtomS3Rへ書き込みます。
2. 初回起動時にスマートフォンまたはPCからWi-Fiアクセスポイント`AtomS3R-OSC`へ接続します。
3. パスワード`12345678`を入力します。
4. 表示されたWi-Fi設定画面で、普段使用するWi-FiのSSIDとパスワードを保存します。
5. AtomS3Rの再起動後、画面に表示されたIPアドレス、または`http://atoms3r-osc.local/`をブラウザーで開きます。
6. OSC送信先と各Chainデバイスを設定し、`Save All Settings`を押します。

詳しい操作方法は[日本語ユーザーガイド](docs/USER_GUIDE_ja.md)を参照してください。

## PlatformIO

### ビルド

```sh
pio run
```

### AtomS3Rへ書き込み

```sh
pio run --target upload
```

### シリアルモニター

```sh
pio device monitor -b 115200
```

## Arduino IDE

1. このリポジトリを`M5ChainOSC`という名前のフォルダーへ配置します。
2. `M5ChainOSC.ino`をArduino IDEで開きます。
3. 必要なライブラリをインストールします。
4. AtomS3Rに適したESP32ボード設定とシリアルポートを選択します。
5. 検証後、書き込みを実行します。

`M5ChainOSC.ino`はArduino IDE用のエントリーポイントです。共通実装は`src/`以下にあります。

## 設定のバックアップ

Web UIの`Export Settings (JSON)`から保存済み設定をダウンロードできます。`Import Settings (JSON)`では、同じUIDの設定を上書きして復元します。

- JSONに含まれない既存デバイス設定は維持されます。
- OSC送信先と画面回転も復元されます。
- Wi-FiのSSIDとパスワードはJSONに含まれず、インポートでも変更されません。
- JSONには将来の互換性判定用の`format`と`schemaVersion`が含まれます。

形式の詳細は[SETTINGS_SCHEMA.md](SETTINGS_SCHEMA.md)を参照してください。

## 工場出荷状態へ戻す

AtomS3R本体のボタンを10秒間長押しすると、Wi-Fi、OSC、デバイス、画面回転など、本ファームウェアが保存した設定をすべて消去して再起動します。この操作は元に戻せないため、必要に応じて先にJSONをエクスポートしてください。

## ディレクトリ構成

```text
M5ChainOSC.ino          Arduino IDE用エントリーポイント
platformio.ini          PlatformIO設定と依存ライブラリ
src/
  main.cpp              共通実装とPlatformIO用エントリーポイント
  chain_devices.*       Chainデバイスの列挙・入力・OSC送信
  storage.*             NVSへの設定保存・読込
  web_ui.*              Web UIとJSON入出力
  display.*             AtomS3Rの画面表示
  wifi_manager.*        Wi-Fi STA／AP処理
  osc_send.*            OSC送信
docs/
  USER_GUIDE_ja.md      日本語ユーザーガイド
```

## 現在の制限

- 1つのKey／Encoderクリック／Joystickクリックにつき、PressとReleaseの合計は8メッセージです。
- OSC Addressは192 bytes、Valueは128 bytes、Device Nameは64 bytesまでです。
- JSONインポートのファイルサイズ上限は48 KiBです。
- Wi-Fi認証情報はJSONバックアップの対象外です。

## 開発状況

機能追加はブランチ単位で進めています。実機検証前の機能を含むブランチは、検証が完了するまで`main`へマージしない運用を推奨します。

不具合報告では、使用したブランチまたはコミット、接続したChainデバイス、再現手順、Arduino IDE／PlatformIOのビルドログを添えてください。
