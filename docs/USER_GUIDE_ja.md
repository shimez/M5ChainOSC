---
layout: default
title: M5ChainOSC 日本語ユーザーガイド
permalink: /user-guide/
---

# M5ChainOSC 日本語ユーザーガイド

このガイドでは、AtomS3Rへの書き込みが完了した後の初期設定と、Web UIの使い方を説明します。

> [!IMPORTANT]
> M5ChainOSCは個人が開発する非公式プロジェクトです。M5Stack Technology Co., Ltd.による公式製品ではなく、同社との提携または承認を示すものではありません。

## 1. 初回のWi-Fi設定

Wi-Fi設定が保存されていない場合、AtomS3Rはアクセスポイントモードで起動します。

1. スマートフォンまたはPCのWi-Fi設定を開きます。
2. SSID`AtomS3R-OSC`へ接続します。
3. パスワード`12345678`を入力します。
4. Wi-Fi設定ページが自動的に開かない場合は、ブラウザーで`http://192.168.4.1/`を開きます。
5. AtomS3Rを接続するWi-FiのSSIDとパスワードを入力して保存します。
6. AtomS3Rが再起動し、接続に成功すると`WiFi OK`とIPアドレスが表示されます。

2.4 GHz帯など、AtomS3Rが接続できるWi-Fiを指定してください。接続できなかった場合は再びアクセスポイントモードになります。

## 2. 設定画面を開く

AtomS3Rと同じネットワークに接続した端末から、次のいずれかをブラウザーで開きます。

- AtomS3Rの画面に表示されたIPアドレス
- `http://atoms3r-osc.local/`

環境によってはmDNSの`.local`アドレスを利用できません。その場合はIPアドレスを使用してください。

## 3. 画面の共通設定

### Language／言語

Web UIの表示言語を`English`または`日本語`から選択します。変更するとその場でページを再読み込みし、選択内容を本体へ保存します。

言語設定がまだ保存されていない場合は、初回アクセス時にブラウザーの優先言語を確認します。日本語が優先されているブラウザーでは日本語、それ以外では英語を使用します。一度選択した後は、アクセスする端末が変わっても本体に保存された言語を使用します。

### WiFi

現在のIPアドレスを確認できます。`Delete WiFi Settings`を押すと保存済みWi-Fi設定を削除します。次回起動時はアクセスポイントモードになります。

### Settings Backup／Restore Settings

- `Export Settings (JSON)`: 保存済み設定をJSONファイルとしてダウンロードします。
- `Import Settings (JSON)`: エクスポート済みJSONを選択して復元します。

インポート時は同じUIDのデバイス設定を上書きし、JSONに含まれない既存デバイス設定は残します。OSC送信先、画面回転、Web UIの言語も復元され、その場で反映されます。Wi-Fi設定はエクスポートにもインポートにも含まれません。

### Display Rotation

`0°`、`90°`、`180°`、`270°`からAtomS3Rの画面方向を選択します。選択後すぐに本体画面へ反映されます。

### OSC Destination

- `Host IP`: OSC受信アプリが動作するPCなどのIPアドレス
- `Port`: OSC受信ポート

例としてVRChatの標準受信ポートを使う場合、一般的には`9000`を指定します。実際の送信先アプリの設定に合わせてください。

## 4. デバイスの共通操作

接続中のChainデバイスはカードとして表示されます。

- `Device Name`: 画面表示や識別に使用する任意の名前
- UID: デバイス固有の識別子。設定はUID単位で保存されます。
- `Save All Settings`: 画面上のOSC送信先と接続中デバイス設定を保存します。

M5ChainOSCが認識した対応Chainデバイスは、通常時にLEDが青く点灯します。各デバイス右上の`…`から`Identify Device (Orange LED for 10s)`を選ぶと、そのUIDのデバイスだけが10秒間オレンジに点灯し、その後青へ戻ります。この識別操作は設定として保存されません。

設定後は必ず`Save All Settings`を押してください。保存したデバイスは、取り外しやAtomS3Rの再起動後も、同じUIDで再接続されると設定が復元されます。

### Saved Device Settings

一度保存したデバイスを、接続状態にかかわらず一覧表示します。

`Delete Settings`は、そのUIDに保存された設定を削除します。接続中のデバイス設定を削除した場合は初期値へ戻ります。

## 5. OSCメッセージの入力規則

### OSC Address

- `/`から始めます。
- 最大192 bytesです。
- 空白および`# * , ? [ ] { }`は使用できません。

例：

```text
/avatar/parameters/Flying
```

### TypeとValue

- `Float`: `0.0`、`1.0`、`-0.5`など
- `Int`: `0`、`1`、`-10`など
- `String`: 任意の文字列

Valueは最大128 bytesです。入力欄の下に現在のバイト数とエラーが表示されます。

## 6. Key

### Press / Releaseモード

Keyを押した時と離した時に、それぞれ複数のOSCメッセージを上から順に送信します。

- PressとReleaseの合計は最大8件です。
- `+ Add OSC Message`で追加します。
- 上下矢印で送信順を変更します。
- `Delete`で削除します。
- PressまたはReleaseを0件にすると、その操作では何も送信しません。

### Sequenceモード

Keyを押すたびに、同じOSC Addressへ値を順番に送信します。

- `Start`: 最初の値
- `End`: 終了値
- `Step`: 1回押すごとの増減量
- `Type`: 送信値の型

Endを越えるとStartへ戻ります。離した時には送信しません。

## 7. Encoder

### Encoder Rotation

- `Rotation Address`: 回転値の送信先
- `Mode / Absolute`: エンコーダー位置を入力範囲から出力範囲へ変換
- `Mode / Increment`: 回転差分へ`Inc Scale`を掛けて送信
- `Abs In Min / Max`: Absoluteモードの入力範囲
- `Inc Scale`: Incrementモードの倍率
- `Out Min / Max`: 出力範囲
- `Out Type`: `Float`または`Int`などの送信型

### Click

緑色の帯で囲まれた範囲がクリック設定です。動作はKeyと同じです。

- Press／Release合計8メッセージまで
- 追加・削除・並べ替え
- 0件なら送信しない
- Sequenceモードにも切り替え可能

## 8. Joystick

### Joystick XY

- `X Address / Y Address`: 各軸のOSC Address
- `Invert X / Invert Y`: 軸方向を反転
- `Deadband`: 小さな変動を送信しない範囲
- `Out Min / Max`: 出力範囲
- `Out Type`: 送信型

### Click

緑色の帯で囲まれた範囲がクリック設定です。KeyおよびEncoderクリックと同じ複数メッセージ／Sequence UIを使用します。

## 9. Angle

- `Address`: 角度値の送信先
- `Resolution`: 8-bitまたは12-bit
- `Deadband`: 前回値との差がこの値以上になった時だけ送信
- `Out Min / Max`: センサー入力から変換する出力範囲
- `Out Type`: 送信型

## 10. ToF

Chain ToFの距離をOSCへ変換します。

- `Address`: 距離値の送信先
- `Deadband (mm)`: 距離変化を送信する最小差
- `Maximum Distance (mm)`: OSC送信に使用する最大距離（31～2000 mm）
- `Output Direction`: 近づけたときに`Out Min`／`Out Max`のどちらへ変化させるか
- `Out Min / Max`: 有効な距離範囲に対応する出力値
- `Out Type`: `Float`または`Int`

> [!NOTE]
> 有効範囲は30 mm以上、`Maximum Distance`未満です。手を離した場合など、測定値が有効範囲外になるとOSC送信を停止します。有効範囲へ戻ると送信を再開します。

## 11. 設定のバックアップと復元

大きな変更、ファームウェア更新、全設定初期化の前にJSONをエクスポートすることをおすすめします。

JSONには次が含まれます。

- 設定形式名とスキーマバージョン
- OSC送信先
- 画面回転
- 保存済みデバイスのUID、名前、種類別設定
- Key／Encoderクリック／Joystickクリックの複数メッセージ

Wi-FiのSSIDとパスワードは含まれません。現在のJSONファイル上限は48 KiBです。

## 12. 全設定の初期化

AtomS3R本体のボタンを10秒間長押しします。画面に進捗が表示され、完了すると次の設定が消去されます。

- Wi-Fi
- OSC送信先
- 保存済みデバイス設定
- 画面回転
- 既知デバイス一覧

初期化は取り消せません。必要な設定は先にJSONへエクスポートしてください。

## 13. トラブルシューティング

### 設定画面を開けない

- AtomS3Rと操作端末が同じWi-Fiに接続されているか確認します。
- `.local`で開けない場合は本体画面のIPアドレスを使います。
- IPアドレスが変わった可能性があるためAtomS3Rを再起動して確認します。

### Chainデバイスが表示されない

- Atomic ToChain Baseとデバイスの接続を確認します。
- 一度取り外し、数秒待ってから再接続します。
- UID取得に失敗した仮IDのデバイス設定は永続保存されません。

### OSCが届かない

- Host IPとPortを確認します。
- AtomS3Rと受信端末が同じネットワークにあるか確認します。
- PCのファイアウォールと受信アプリのOSC設定を確認します。
- OSC Addressが`/`から始まっているか確認します。

### JSONをインポートできない

- M5ChainOSCからエクスポートしたJSONであることを確認します。
- `format`と対応する`schemaVersion`が必要です。
- ファイルサイズは48 KiB以下にします。
- 不正なAddress、値、重複UID、8件を超えるクリックメッセージは拒否されます。

### 保存後に設定が戻る

- `Save All Settings`の成功画面を確認します。
- デバイスのUIDが取得できているか確認します。
- 問題が続く場合は、使用中のブランチ／コミット、再現手順、シリアルログを添えて報告してください。
