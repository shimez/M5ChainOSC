# M5ChainOSC

M5Stack AtomS3RとAtomic ToChain Baseを使い、M5Stack Chainデバイスの操作をOSCメッセージとして送信するファームウェアです。ブラウザーから送信先やデバイスごとの動作を設定でき、設定はChainデバイスのUID単位で本体へ保存されます。

主にVRChatのアバターパラメーター操作を想定していますが、OSCを受信できるアプリケーションで利用できます。

> [!IMPORTANT]
> M5ChainOSCは個人が開発する非公式プロジェクトです。M5Stack Technology Co., Ltd.による公式製品ではなく、同社との提携または承認を示すものではありません。

## M5ChainOSCの特徴

- **PCへの常駐アプリは不要**: AtomS3RからWi-Fi経由で、PC上のVRChatなどOSCを受信できるアプリケーションへ直接メッセージを送信します。
- **ブラウザーだけで設定可能**: OSCの送信先やChainデバイスごとの動作を、同じネットワーク上のPCやスマートフォンから設定できます。
- **セットアップ後はPCとのUSB接続が不要**: ファームウェアの書き込みなど初期セットアップ後は、PCから独立して利用できます。動作には給電が必要なため、USB ACアダプターやモバイルバッテリーなどへ接続してください。

> [!NOTE]
> Chain ToFを含む対応デバイスは実機での動作確認を行っています。

## 主な機能

- Web UIによる設定
- Chainデバイスの接続・取り外し・再接続の検出
- 認識したChainデバイスの青色LED表示と、Web UIからの10秒間オレンジ識別表示
- UID単位の設定保存と再接続時の復元
- OSC値の`Float`、`Int`、`String`送信
- Key、Encoderクリック、Joystickクリックから複数OSCメッセージを送信
- Press／Release合計8件まで追加・削除・並べ替え
- Press／Releaseを0件にして「何も送信しない」設定
- ボタンを押すたびに値を進めるSequenceモード
- 設定のJSONエクスポート／インポート
- UIDを含まないデバイス単体プリセットのJSON共有
- Chain ToFの最大距離、範囲外送信停止、出力方向設定
- 画面回転
- Arduino IDEとPlatformIOの両方に対応

## 対応ハードウェア

- M5Stack AtomS3R
- M5Stack Atomic ToChain Base
- Chain Key
- Chain Encoder
- Chain Angle
- Chain Joystick
- Chain ToF

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

詳しい操作方法は[日本語ユーザーガイド](https://shimez.github.io/M5ChainOSC/user-guide/)を参照してください。ガイドのMarkdown原稿は[docs/USER_GUIDE_ja.md](docs/USER_GUIDE_ja.md)にあります。

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

## Web Installer

正式版ファームウェアは、[M5ChainOSC Web Installer](https://shimez.github.io/M5ChainOSC/installer/)からブラウザ経由でAtomS3Rへ書き込めます。

> [!WARNING]
> 再インストールや消去を行う前に、必要な設定をJSONでバックアップし、Installer画面に記載された注意事項を確認してください。

Web Installerの構成とローカル確認方法は、[Installer README](docs/installer/README.md)を参照してください。

## 設定のバックアップ

Web UIの`Export Settings (JSON)`から保存済み設定をダウンロードできます。`Import Settings (JSON)`では、同じUIDの設定を上書きして復元します。

- JSONに含まれない既存デバイス設定は維持されます。
- OSC送信先と画面回転も復元されます。
- Wi-FiのSSIDとパスワードはJSONに含まれず、インポートでも変更されません。
- JSONには将来の互換性判定用の`format`と`schemaVersion`が含まれます。

形式の詳細は[SETTINGS_SCHEMA.md](SETTINGS_SCHEMA.md)を参照してください。

## デバイス設定プリセット

接続中の各Chainデバイス右上にある`…`メニューから、そのデバイス単体の設定をJSONとしてエクスポート・インポートできます。

- プリセットにはUIDとDevice Nameを含みません。
- 同じ種類のChainデバイスにだけインポートできます。
- インポート先のUIDとDevice Nameは維持されます。
- OSC Address、値、出力範囲、動作モードなどの種類別設定が置き換わります。
- 編集中の内容をエクスポートする場合は、先に`Save All Settings`で保存してください。

この機能は、設定サンプルをほかの利用者へ共有する用途を想定しています。本体全体の移行や復旧には、従来のSettings Backupを使用してください。

すぐに利用できるVRChat向けサンプルは、[Device Presets](presets/README.md)で公開しています。

- JoystickによるVRChat内の移動とジャンプ
- KeyによるマイクのON／OFF
- Keyによる配信向けカメラ設定の切り替え

## すべての設定を消去する

AtomS3R本体の画面を10秒間長押しすると、Wi-Fi、OSC、デバイス、画面回転など、M5ChainOSCが保存した設定をすべて消去して再起動します。AtomS3Rのファームウェア自体は変更されません。この操作は元に戻せないため、必要に応じて先にJSONをエクスポートしてください。

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
  installer/            正式版Web Installer
presets/
  README.md              デバイスプリセット一覧と使用方法
  joystick/              Joystick用プリセット
  key/                   Key用プリセット
```

## 現在の制限

- 1つのKey／Encoderクリック／Joystickクリックにつき、PressとReleaseの合計は8メッセージです。
- OSC Addressは192 bytes、Valueは128 bytes、Device Nameは64 bytesまでです。
- JSONインポートのファイルサイズ上限は48 KiBです。
- Wi-Fi認証情報はJSONバックアップの対象外です。

## 開発状況

機能追加はブランチ単位で進めています。実機検証前の機能を含むブランチは、検証が完了するまで`main`へマージしない運用を推奨します。

不具合報告では、使用したブランチまたはコミット、接続したChainデバイス、再現手順、Arduino IDE／PlatformIOのビルドログを添えてください。

## 開発クレジット

本プロダクトのコードはGrokによって作成され、その後Codexによって機能拡張、構成改善、不具合修正および互換性向上が行われました。

README、ユーザーガイド、設定仕様をはじめとする本プロジェクトのドキュメントはCodexによって作成・整備されました。

プロジェクト全体の企画、要件定義、UIの方向づけ、実機検証および公開判断は、プロジェクトオーナーとの協働によって進められています。
