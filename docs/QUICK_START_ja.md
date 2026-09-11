---
layout: default
title: M5ChainOSC クイックスタート
permalink: /quick-start/
---

# M5ChainOSC クイックスタート

[English version](../en/quick-start/)

このガイドでは、M5Stack Chain Keyを1台手動設定し、M5ChainOSCからVRChatへOSCメッセージを送信して動作を確認します。詳しい設定は[日本語ユーザーガイド](../user-guide/)を参照してください。

## 用意するもの

- M5Stack AtomS3R
- M5Stack Atomic ToChain Base
- M5Stack Chain Key
- データ通信対応USBケーブル
- 2.4 GHz帯Wi-Fi
- VRChatを実行するPC
- デスクトップ版Google ChromeまたはMicrosoft Edge

## 1. ファームウェアを書き込む

1. [M5ChainOSC Web Installer](https://shimez.github.io/M5ChainOSC/installer/)をChromeまたはEdgeで開きます。
2. AtomS3RをUSBでPCへ接続します。
3. `Install M5ChainOSC`を押し、AtomS3Rのシリアルポートを選択します。
4. 画面の案内に従ってインストールします。

ポートが表示されない場合は、AtomS3R本体のボタンを押しながらUSBへ接続し直してください。この操作は書き込みのためのもので、OSC入力には使用しません。

## 2. Wi-Fiを設定する

1. AtomS3Rを起動します。
2. `AtomS3R-OSC`へ接続します。
3. パスワード`12345678`を入力します。
4. 設定画面が開かない場合は`http://192.168.4.1/`を開きます。
5. 2.4 GHz帯Wi-FiのSSIDとパスワードを保存します。

AtomS3RとVRChatを実行するPCは、相互通信できる同じネットワークへ接続してください。Web UIには認証機能がないため、信頼できるローカルネットワークで使用してください。

## 3. VRChatでOSCを有効にする

VRChatのリングメニュー → オプション → OSC → 有効の順に操作します。

## 4. VRChat PCのIPv4アドレスを確認する

WindowsでPowerShellまたはコマンドプロンプトを開き、`ipconfig`を実行します。AtomS3Rと同じネットワークのWi-FiまたはEthernetアダプターのIPv4 Addressを確認してください。VPNや仮想ネットワークのアドレスは使用しません。

## 5. 設定画面を開いて送信先を設定する

1. M5Stack Chain KeyをAtomic ToChain Base経由でAtomS3Rへ接続します。
2. AtomS3Rの画面に表示されたIPアドレス、または`http://m5chainosc.local/`を開きます。
3. 接続したChain Keyのカードを開き、OSC送信先のHost IPに手順4のPCのIPv4アドレスを入力します。
4. Portに`9000`を入力します。

## 6. Chain KeyにVoice操作を設定する

接続したM5Stack Chain Keyのカードで、Key ModeがPress / Releaseになっていることを確認します。「押した時」に次の値を入力します。

- OSCアドレス：`/input/Voice`
- 型：`Int`
- 値：`1`

「離した時」に切り替えて、次の値を入力します。

- OSCアドレス：`/input/Voice`
- 型：`Int`
- 値：`0`

## 7. 保存して動作を確認する

1. `Save All Settings（すべての設定を保存）`を押します。
2. VRChatが起動しOSCが有効な状態で、設定したChain Keyを押します。
3. VRChatのVoice入力状態が切り替わることを確認します。
4. Chain Keyを離し、Voice入力状態が元に戻ることを確認します。

これでM5ChainOSCからVRChatへOSCメッセージを送信できています。Encoder、Joystick、Angle、ToF、Sequence、Device Preset、バックアップなどの詳細は[日本語ユーザーガイド](../user-guide/)を参照してください。設定の再利用や共有にはDevice Presetを利用できます。
