# M5ChainOSC Web Installer

M5ChainOSCの正式版ファームウェアをAtomS3Rへブラウザから書き込むためのWeb Installerです。

> [!IMPORTANT]
> M5ChainOSCは個人が開発する非公式プロジェクトです。M5Stack Technology Co., Ltd.による公式製品ではなく、同社との提携または承認を示すものではありません。

## 公開URL

```text
https://shimez.github.io/M5ChainOSC/installer/
```

デスクトップ版のChromeまたはEdgeを使用してください。

現在の正式版は`1.4.0`です。

- Version 1.4.0: OSC送信時の画面ちらつきを抑制し、未保存表示、スクロール位置の維持、デバイス設定の折りたたみ、画面遷移のない設定保存・デバイス削除に対応
- Version 1.3.0: Web UIの英語／日本語切り替え、AtomS3R画面上のバージョン表示、Web UIの表示高速化に対応
- Version 1.2.0: 認識したChainデバイスの青色LED表示と、Web UIからの10秒間オレンジ識別表示に対応
- Version 1.1.0: Chain ToFの最大距離、範囲外でのOSC送信停止、出力方向の設定に対応

## バイナリの配置

Arduino IDEの`Sketch` → `Export Compiled Binary`で生成したmergedバイナリを、次の名前で配置します。

```text
docs/installer/firmware/M5ChainOSC-1.4.0-AtomS3R-merged.bin
```

`manifest.json`は、このファイルをESP32-S3のoffset `0x0`へ書き込みます。

## リリース前の確認

- `manifest.json`の`version`とバイナリのファイル名が一致している
- mergedバイナリをoffset `0x0`から実機へ書き込める
- 消去済みAtomS3Rで起動、Wi-Fi設定、Web UI表示、設定保存・復元が動作する
- 対応するChainデバイスの主要操作が動作する
- バイナリのSHA-256と、ビルド元コミットをリリース記録へ残す
- HTTPSで公開したInstallerをChromeまたはEdgeから利用できる

## ローカル確認

`docs/installer`ディレクトリでローカルWebサーバーを起動します。

```powershell
py -m http.server 8000 --bind 127.0.0.1
```

次のURLをデスクトップ版ChromeまたはEdgeで開きます。

```text
http://localhost:8000/
```

ESP Web ToolsをCDNから読み込むため、Installerの利用時にはインターネット接続が必要です。
