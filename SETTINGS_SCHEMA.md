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
- 保存済みデバイスのUID、種類、表示名
- デバイス種類別のOSC設定
- Chain KeyのPress、Release、Sequence設定

Wi-FiのSSIDとパスワードは含めません。`wifiCredentialsIncluded`は常に`false`です。

## インポート

設定画面からエクスポート済みJSONを選択して復元できます。

- `format`が一致しないファイルは拒否
- 対応していない`schemaVersion`は拒否
- 全デバイスを検証してから保存開始
- JSONに含まれるUIDの設定を上書き
- JSONに含まれない既存デバイス設定は維持
- OSC送信先と画面回転を復元
- Wi-Fi設定は変更しない

現在のアップロード上限は48 KiBです。
