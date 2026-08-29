# Saved-device per-type limit fixtures

M5ChainOSCの保存上限「Key、Encoder、Angle、Joystick、ToFそれぞれ40件」を実機で確認するための全体設定JSONです。

## Fixtureと期待結果

| Fixture | 期待結果 |
|---|---|
| `M5ChainOSC-settings-key-40.json` | 空の状態でKey 40件を受け入れる |
| `M5ChainOSC-settings-key-41.json` | Keyの種別上限超過として全件拒否する |
| `M5ChainOSC-settings-encoder-40.json` | 空の状態でEncoder 40件を受け入れる |
| `M5ChainOSC-settings-encoder-41.json` | Encoderの種別上限超過として全件拒否する |
| `M5ChainOSC-settings-angle-40.json` | 空の状態でAngle 40件を受け入れる |
| `M5ChainOSC-settings-angle-41.json` | Angleの種別上限超過として全件拒否する |
| `M5ChainOSC-settings-joystick-40.json` | 空の状態でJoystick 40件を受け入れる |
| `M5ChainOSC-settings-joystick-41.json` | Joystickの種別上限超過として全件拒否する |
| `M5ChainOSC-settings-tof-40.json` | 空の状態でToF 40件を受け入れる |
| `M5ChainOSC-settings-tof-41.json` | ToFの種別上限超過として全件拒否する |
| `M5ChainOSC-settings-mixed-key40-encoder1.json` | 合計41件を受け入れ、旧「全種類合計40件」制限がないことを確認できる |

## 推奨手順

1. 現在の設定をエクスポートします。
2. 全設定をリセットし、空の状態で40件fixtureをインポートします。
3. 40件が保存済み一覧に表示され、再起動後も復元されることを確認します。
4. 同じ種別の41件fixtureをインポートし、拒否されることと、既存40件が変更されないことを確認します。
5. 空の状態で混合41件fixtureをインポートし、Key 40件とEncoder 1件がすべて復元されることを確認します。
6. 5種類の40件fixtureを順番にインポートし、合計200件を保存・再起動後に復元できることを確認します。

41件fixtureの拒否後は、保存済み一覧だけでなく、全体設定を再エクスポートして既存設定が不変であることも確認してください。

## 再生成

PowerShellで次を実行します。

```powershell
.\scripts\generate_storage_limit_fixtures.ps1
```
