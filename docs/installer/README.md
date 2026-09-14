# M5ChainOSC Web Installer

M5ChainOSCの正式版ファームウェアをAtomS3Rへブラウザから書き込むためのWeb Installerです。

> [!IMPORTANT]
> M5ChainOSCは個人が開発する非公式プロジェクトです。M5Stack Technology Co., Ltd.による公式製品ではなく、同社との提携または承認を示すものではありません。

## 公開URL

```text
https://shimez.github.io/M5ChainOSC/installer/
```

デスクトップ版のChromeまたはEdgeを使用してください。

現在の正式版は`1.12.5`です。

変更履歴はリポジトリの `CHANGELOG.md` を参照してください。


## ファームウェアの配置

Web Installerには、GitHub ActionsでビルドしてGitHub Releaseへ添付したmergedバイナリを、Pages配信Workflowが自動的に組み込みます。Version 1.12.5では次のパスで配信します。

```text
installer/firmware/M5ChainOSC-1.12.5-AtomS3R-merged.bin
```

`manifest.json`は、この同一オリジンのファイルをESP32-S3のoffset `0x0`へ書き込みます。GitHub Release AssetのURLはブラウザのCORS制限で取得できない場合があるため、ブラウザから直接参照せず、GitHub ActionsがPages成果物へコピーします。

## リリース前の確認

- `manifest.json`の`version`、Releaseタグ、バイナリのファイル名が一致している
- Installer用バイナリのSHA-256がGitHub Release Assetと一致している
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
