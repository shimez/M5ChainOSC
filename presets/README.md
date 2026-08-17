# M5ChainOSC Device Presets

[English version](README_en.md)

M5ChainOSCで利用できる、Chainデバイス単体の設定サンプルです。

> [!IMPORTANT]
> M5ChainOSCは個人が開発する非公式プロジェクトです。M5Stack Technology Co., Ltd.による公式製品ではなく、同社との提携または承認を示すものではありません。

各プリセットはUIDとDevice Nameを含まないため、同じ種類の別のChainデバイスへインポートして利用できます。

初めてM5ChainOSCを使用する場合は、[プリセット・クイックスタート](https://shimez.github.io/M5ChainOSC/quick-start-presets/)を参照してください。

プリセットのファイル名は`デバイス種別-用途.json`の形式です。ダウンロード後も、ファイル名の先頭からインポート対象のChainデバイスを判別できます。

## 使い方

1. 下の一覧から使用するJSONファイルを開きます。
2. GitHubの`Download raw file`からJSONファイルをダウンロードします。
3. M5ChainOSCのWeb UIを開きます。
4. インポート先デバイスの右上にある`…`を押します。
5. `Import Preset (JSON)`を選択します。
6. ダウンロードしたJSONファイルを選び、インポートを実行します。
7. 必要に応じてOSC Address、軸、反転設定などを調整します。

> [!IMPORTANT]
> プリセットは同じ種類のChainデバイスにのみインポートできます。例えばKey用プリセットをJoystickやEncoderへインポートすることはできません。

## プリセット一覧

### Joystick

#### [VRChat Move](joystick/joystick-vrchat-move.json)

VRChat内の移動をJoystickで行うサンプルです。

- スティックの傾きで移動します。
- スティック押し込みでジャンプします。
- 使用しているデバイスの向きに合わせて、X／Y、OSC Address、軸の反転をカスタマイズしてください。

### Key

#### [VRChat AFK Control](key/key-vrchat-afk-control.json)

VRChatのAFKモードをON／OFFするサンプルです。

#### [VRChat Voice Control](key/key-vrchat-voice-control.json)

VRChat内でマイクのON／OFFを操作するサンプルです。

#### [VRChat Camera Controls](key/key-vrchat-camera-controls.json)

VRChat内のカメラを配信向けに切り替えるサンプルです。

- カメラモード
- Spout設定
- Smooth設定

これらの設定をまとめて変更します。

### Angle

#### [VRChat Camera Zoom](angle/angle-vrchat-camera-zoom.json)

VRChat内のカメラのズームをAngleで調整するサンプルです。

### Encoder

#### [VRChat Camera Zoom](encoder/encoder-vrchat-camera-zoom.json)

VRChat内のカメラのズームをEncoderで調整するサンプルです。

## 注意事項

- インポート先デバイスのUIDとDevice Nameは変更されません。
- インポートすると、そのデバイスのOSC設定や動作モードがプリセットの内容で上書きされます。
- 必要に応じて、インポート前に現在のデバイス設定を`Export Preset (JSON)`で保存してください。
- 本体全体の移行や復旧には、Web UI上部の`Settings Backup & Restore`を使用してください。
- VRChat側の設定、アバター、使用環境によっては、OSC Addressなどの調整が必要です。
- JSONファイルを編集するときは、`format`、`schemaVersion`、`deviceType`を変更しないでください。

プリセットJSONの形式については、[Settings JSON schema](../SETTINGS_SCHEMA.md#デバイス設定プリセット)を参照してください。
