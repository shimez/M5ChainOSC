# M5ChainOSC Web Installer

M5ChainOSCの正式版ファームウェアをAtomS3Rへブラウザから書き込むためのWeb Installerです。

> [!IMPORTANT]
> M5ChainOSCは個人が開発する非公式プロジェクトです。M5Stack Technology Co., Ltd.による公式製品ではなく、同社との提携または承認を示すものではありません。

## 公開URL

```text
https://shimez.github.io/M5ChainOSC/installer/
```

デスクトップ版のChromeまたはEdgeを使用してください。

現在の正式版は`1.8.0`です。

- Version 1.8.0: 保存済みデバイス上限を40件へ拡張し、省容量なデバイス別保存方式と旧設定互換、JSONインポートの互換性・結果表示を改善
- Version 1.7.0: Wi-Fi切断後の自動再接続とOSC送信表示を改善し、Sequenceの検証・正規化・String送信をChainOSCminiと統一
- Version 1.6.1: Encoder、Angle、ToF、JoystickのWeb UIをグリッド配置へ整理し、狭い画面での表示を改善
- Version 1.6.0: デバイス単位プリセットをChainOSCminiと共通化し、旧M5ChainOSC形式のインポート互換性を維持
- Version 1.5.3: AtomS3R画面の描画範囲を最適化し、複数OSCメッセージの送信内容を順番に表示
- Version 1.5.2: OSC Addressの入力検証をすべてのChainデバイス設定へ適用し、設定のJSONエクスポート／インポート互換性を改善
- Version 1.5.1: Web UI送信時のタイムアウトを調整し、画面が白く表示されることがある問題を改善
- Version 1.5.0: Chainデバイス設定の日本語表示拡充、Web UIとユーザーガイドの用語統一、各設定パラメータの解説追加、EncoderのIncrementモードに応じた表示整理に対応
- Version 1.4.0: OSC送信時の画面ちらつきを抑制し、未保存表示、スクロール位置の維持、デバイス設定の折りたたみ、画面遷移のない設定保存・デバイス削除に対応
- Version 1.3.0: Web UIの英語／日本語切り替え、AtomS3R画面上のバージョン表示、Web UIの表示高速化に対応
- Version 1.2.0: 認識したChainデバイスの青色LED表示と、Web UIからの10秒間オレンジ識別表示に対応
- Version 1.1.0: Chain ToFの最大距離、範囲外でのOSC送信停止、出力方向の設定に対応

## ファームウェアの配置

Web Installerには、GitHub ActionsでビルドしてGitHub Releaseへ添付したmergedバイナリを、Pages配信Workflowが自動的に組み込みます。Version 1.8.0では次のパスで配信します。

```text
installer/firmware/M5ChainOSC-1.8.0-AtomS3R-merged.bin
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
