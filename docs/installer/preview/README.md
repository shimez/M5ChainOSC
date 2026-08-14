# M5ChainOSC Preview Web Installer

開発中のM5ChainOSCをAtomS3Rへブラウザから書き込むためのPreview版Web Installerです。

## 配置するバイナリ

Arduino IDEの`Sketch` → `Export Compiled Binary`で生成したmergedバイナリを、次の名前でこのディレクトリへ配置してください。

```text
M5ChainOSC-preview-AtomS3R-merged.bin
```

`manifest.json`では、このファイルをESP32-S3のoffset `0x0`へ書き込むよう指定しています。

## 公開前の更新

正式に公開するビルドに合わせて、`manifest.json`の`version`を更新してください。

```json
"version": "preview-YYYY.MM.DD"
```

あわせて次の情報を記録することを推奨します。

- ビルド元ブランチ
- コミットID
- Arduino ESP32 Coreのバージョン
- Arduino IDEのボード設定
- mergedバイナリのSHA-256
- 実機検証済み機能と未検証機能

## ローカル確認

このディレクトリでローカルWebサーバーを起動します。

```powershell
py -m http.server 8000 --bind 127.0.0.1
```

デスクトップ版ChromeまたはEdgeで次を開きます。

```text
http://localhost:8000/
```

## GitHub Pages

リポジトリのGitHub Pages公開元を`/docs`にすると、通常は次のURLで利用できます。

```text
https://shimez.github.io/M5ChainOSC/installer/preview/
```

このInstallerはESP Web ToolsをCDNから読み込むため、利用時にはインターネット接続が必要です。
