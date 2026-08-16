---
layout: default
title: M5ChainOSC プリセット・クイックスタート
permalink: /quick-start-presets/
---

# M5ChainOSC プリセット・クイックスタート

[English version](../en/quick-start-presets/)

このガイドでは、公開されているデバイスプリセットを使い、M5ChainOSCからVRChatへOSCメッセージを送信するまでを説明します。各設定項目の詳細は[日本語ユーザーガイド](../user-guide/)を参照してください。

> [!IMPORTANT]
> M5ChainOSCは個人が開発する非公式プロジェクトです。M5Stack Technology Co., Ltd.およびVRChat Inc.による公式製品ではありません。

## 用意するもの

- M5Stack AtomS3R
- 対応するM5Stack Chainデバイス
- データ通信対応USBケーブル
- 2.4 GHz帯のWi-Fi
- VRChatを実行するPC
- デスクトップ版Google ChromeまたはMicrosoft Edge

## 1. ファームウェアを書き込む

1. [M5ChainOSC Web Installer](https://shimez.github.io/M5ChainOSC/installer/)をChromeまたはEdgeで開きます。
2. AtomS3RをUSBでPCへ接続します。
3. `Install M5ChainOSC`を押し、AtomS3Rのシリアルポートを選択します。
4. 画面の案内に従ってインストールします。

ポートが表示されない場合は、AtomS3R本体のボタンを押しながらUSBへ接続し直してください。

## 2. Wi-Fiを設定する

1. AtomS3Rを起動します。
2. スマートフォンまたはPCからWi-Fiアクセスポイント`AtomS3R-OSC`へ接続します。
3. パスワード`12345678`を入力します。
4. 設定画面が自動的に開かない場合は、ブラウザーで`http://192.168.4.1/`を開きます。
5. AtomS3Rを接続するWi-FiのSSIDとパスワードを保存します。

AtomS3Rが接続できるWi-Fiは2.4 GHz帯のみです。AtomS3RとVRChatを実行するPCは、互いに通信できる同じネットワークへ接続してください。

## 3. VRChatでOSCを有効にする

VRChat内で次の順に操作し、OSCを有効にします。

```text
リングメニュー → オプション → OSC → 有効
```

## 4. PCのIPアドレスを確認する

WindowsではPowerShellまたはコマンドプロンプトで次を実行します。

```powershell
ipconfig
```

AtomS3Rと同じネットワークに接続しているアダプターの`IPv4 Address`を確認します。VPNや仮想ネットワークではなく、通常利用しているWi-FiまたはEthernetのアドレスを使用してください。

## 5. M5ChainOSCの設定画面を開く

1. 使用するChainデバイスをAtomS3Rへ接続します。
2. AtomS3Rの画面に表示されたIPアドレス、または`http://atoms3r-osc.local/`をブラウザーで開きます。
3. `OSC Destination（OSC送信先）`の`Host IP`へ、手順4で確認したPCのIPアドレスを入力します。
4. `Port`へVRChatの標準受信ポート`9000`を入力します。

## 6. プリセットをダウンロードする

[Device Presets](https://github.com/shimez/M5ChainOSC/tree/main/presets)から、接続したデバイスと同じ種類のJSONファイルを選びます。GitHubでファイルを開き、`Download raw file`を押して保存してください。

利用できるサンプルには次のようなものがあります。

| デバイス | プリセット | 動作 |
| --- | --- | --- |
| Joystick | `joystick/vrchat-move.json` | スティックで移動し、押し込みでジャンプします。 |
| Key | `key/vrchat-voice-control.json` | マイクのON／OFFを操作します。 |
| Key | `key/vrchat-camera-controls.json` | 配信用カメラ設定をまとめて変更します。 |
| Key | `key/vrchat-afk-control.json` | AFKモードをON／OFFします。 |
| Angle | `angle/vrchat-camerazoom.json` | 角度でカメラのズームを調整します。 |
| Encoder | `encoder/vrchat-camerazoom.json` | 回転でカメラのズームを調整します。 |

## 7. プリセットをインポートする

1. Web UIで設定対象のChainデバイスを確認します。
2. デバイス右上の`…`を押します。
3. `Import Preset (JSON)（プリセットをインポート）`を選択します。
4. ダウンロードしたJSONファイルを選択します。
5. 確認画面でインポートを実行します。
6. `Save All Settings（すべての設定を保存）`を押します。

プリセットは同じ種類のChainデバイスにのみインポートできます。プリセットにはUIDとDevice Nameが含まれないため、現在接続しているデバイスの識別情報は維持されます。

## 8. 動作を確認する

VRChatが起動し、OSCが有効になっている状態でChainデバイスを操作します。

- Joystickの移動プリセットでは、スティックを倒してアバターが移動することを確認します。
- Keyの音声プリセットでは、キーを押してマイク状態が切り替わることを確認します。
- Angle／Encoderのカメラプリセットでは、VRChatのカメラを表示してズームが変化することを確認します。

## OSCが届かない場合

次の項目を順番に確認してください。

- VRChat内でOSCが`有効`になっている
- `Host IP`がVRChatを実行しているPCの現在のIPv4アドレスになっている
- `Port`が`9000`になっている
- AtomS3RとPCが互いに通信できる同じネットワークに接続されている
- Windows Defender FirewallなどでVRChatの通信が遮断されていない
- 設定変更後に`Save All Settings`を押している
- 接続したデバイスとプリセットの種類が一致している

詳しい確認方法と各設定項目については、[日本語ユーザーガイド](../user-guide/)を参照してください。
