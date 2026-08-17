# Settings JSON schema

設定画面の`Export Settings (JSON)`から、NVSに保存済みの設定をダウンロードできます。

## バージョン

JSONの最上位には必ず次の識別情報を含めます。

```json
{
  "format": "M5ChainOSC-settings",
  "schemaVersion": 2
}
```

- `format`: 他のJSONファイルとの誤認を防ぐ固定文字列
- `schemaVersion`: JSON構造のバージョン。互換性のない変更時に増加させます

Version 2ではEncoder／Joystickのクリック`press`・`release`を、Keyと同じメッセージ配列へ変更しました。

既存項目の追加だけで旧リーダーが無視できる場合は、原則として同じバージョンを維持します。項目名、型、意味、必須構造を変更する場合はバージョンを増加させます。

## エクスポート範囲

- OSC送信先
- 画面回転
- Web UIの言語（`global.uiLanguage`: `en`または`ja`）
- 保存済みデバイスのUID、種類、表示名
- デバイス種類別のOSC設定
- Chain KeyのPress、Release、Sequence設定
- Chain ToFの最大距離、出力方向、出力範囲設定

Wi-FiのSSIDとパスワードは含めません。`wifiCredentialsIncluded`は常に`false`です。

## インポート

設定画面からエクスポート済みJSONを選択して復元できます。

- `format`が一致しないファイルは拒否
- 対応していない`schemaVersion`は拒否
- 全デバイスを検証してから保存開始
- JSONに含まれるUIDの設定を上書き
- JSONに含まれない既存デバイス設定は維持
- OSC送信先、画面回転、Web UIの言語を復元
- `uiLanguage`がないVersion 2ファイルでは、現在の言語設定を維持
- Wi-Fi設定は変更しない

現在のアップロード上限は48 KiBです。

## デバイス設定プリセット

デバイス設定プリセットは、本体全体のバックアップとは別形式です。接続中のChainデバイス単体の設定を、同じ種類の別個体へ共有するために使用します。

```json
{
  "format": "ChainOSC-device-preset",
  "schemaVersion": 1,
  "deviceType": 3,
  "deviceTypeName": "Key",
  "key": {
    "mode": 0,
    "press": [],
    "release": [],
    "sequence": {}
  }
}
```

- `format`はM5ChainOSCとChainOSCminiで共通の`ChainOSC-device-preset`です。
- M5ChainOSCは後方互換性のため、旧形式`M5ChainOSC-device-preset`もインポートできます。
- 新規エクスポートでは共通形式を使用します。
- UIDとDevice Nameは含めません。
- インポート先として選択した接続中デバイスのUIDとDevice Nameを維持します。
- `deviceType`がインポート先と一致しないプリセットは拒否します。
- デバイス種類別の各値は、全体バックアップと同じ検証を行います。
- プリセットのアップロード上限は16 KiBです。
