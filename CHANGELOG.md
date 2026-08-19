# Changelog

M5ChainOSCの主な変更履歴を記録します。

形式は[Keep a Changelog](https://keepachangelog.com/ja/1.1.0/)を参考にし、バージョン番号には[Semantic Versioning](https://semver.org/lang/ja/)を使用します。

## [Unreleased]

## [1.6.1]

### Changed

- Encoder、Angle、ToF、Joystickの設定項目を横幅に応じたグリッド配置へ整理
- 狭い画面では1列表示へ切り替え、クリック設定のはみ出しを改善

## [1.6.0]

### Added

- M5ChainOSCとChainOSCminiで共有するデバイスプリセット形式`ChainOSC-device-preset`に対応
- 旧`M5ChainOSC-device-preset`形式のインポート互換性を維持

### Changed

- デバイス単位の新規エクスポートと公式Presetsを共通プリセット形式へ移行
- エクスポートファイル名を`ChainOSC-<DeviceType>-preset.json`へ変更

## [1.5.3]

### Added

- 日本語ユーザーガイド、プリセット・クイックスタート、デバイスプリセット一覧の英語版を追加
- Web Installerへ簡単な英語インストール手順を追加
- プリセットを使ってVRChatで動作確認するまでを案内する日本語クイックスタートガイドを追加
- VRChatでOSCを有効にする手順を日本語ユーザーガイドへ追加
- KeyのAFK操作、Angle／Encoderのカメラズーム用VRChatプリセットを一覧へ追加
- EncoderのAbsoluteモードにおける入力範囲と循環動作の詳しい解説を追加
- Web UIを信頼できるローカルネットワークで使用するよう、READMEと各ガイドへ注意事項を追加

### Changed

- GitHub PagesをGitHub Actionsから配信し、公開済みReleaseのmergedバイナリをWeb Installerへ自動的に組み込む構成へ変更
- GitHub Pages関連ActionsをNode.js 24対応版へ更新
- AtomS3R起動直後の`Waiting...`表示位置を調整
- OSC送信時に区切り線を再描画せず、画面の書き換え範囲を縮小
- 複数OSCメッセージの送信内容を200 ms間隔で順番に表示

### Fixed

- ボタンを離した際に、Press側のOSCメッセージ表示待ちが消去される問題を修正

## [1.5.2]

### Fixed

- OSC Addressの入力検証をEncoder、Angle、Joystick、ToFおよび各Sequence設定へ適用
- Web UIで保存できた設定がJSONインポート時に長さエラーになる問題を修正
- 既存の不正なOSC AddressをWeb UIの初期表示時に検出できるよう改善

## [1.5.1]

### Fixed

- Web UIの送信待機処理とタイムアウトを調整
- OSC Settingsが白い画面のまま読み込み完了になることがある問題を改善

## 1.5.0

### Added

- Chainデバイス設定の日本語表示を拡充
- 日本語ユーザーガイドへ各デバイスの設定パラメーター解説を追加

### Changed

- Web UIと日本語ユーザーガイドの用語を統一
- EncoderのIncrementモードでは使用しないAbsolute入力設定を非表示化

## 1.4.0

### Added

- Web UIへ未保存状態の表示を追加
- デバイス設定の折りたたみ機能を追加
- 保存後のスクロール位置維持に対応

### Changed

- 設定保存と保存済みデバイス削除を画面遷移なしで実行する方式へ変更
- OSC送信時のAtomS3R画面更新範囲を限定

### Fixed

- OSCメッセージ送信時に画面全体がちらつく問題を改善

## 1.3.0

### Added

- Web UIの英語／日本語切り替えに対応
- AtomS3Rの画面上部へファームウェアバージョンを表示

### Changed

- Web UIのHTML送信処理を分割・最適化

### Fixed

- Web UIを繰り返しリロードすると応答が徐々に遅くなる問題を改善

## 1.2.0

### Added

- 認識したChainデバイスのLEDを青色で点灯
- Web UIの`Identify Device`から、指定したデバイスのLEDを10秒間オレンジ色に変更する機能を追加

## 1.1.0

### Added

- Chain ToFで実際に使用する最大距離を設定する機能を追加
- ToFの距離に対する出力方向を選択する機能を追加

### Changed

- ToFの測定範囲外では最長距離を送信せず、OSC送信を停止する動作へ変更

## 1.0.0

### Added

- AtomS3RとAtomic ToChain Baseに対応した初回正式版
- Chain Key、Encoder、Joystick、Angle、ToFに対応
- ブラウザーからOSC送信先とChainデバイスを設定するWeb UIを搭載
- ChainデバイスのUID単位で設定を保存・復元
- Key、Encoderクリック、Joystickクリックの複数OSCメッセージとSequenceモードに対応
- 全体設定およびデバイス単体プリセットのJSONエクスポート／インポートに対応
- Arduino IDEとPlatformIOの両方に対応
- AtomS3R用Web Installerを公開

[Unreleased]: https://github.com/shimez/M5ChainOSC/compare/v1.6.1...HEAD
[1.6.1]: https://github.com/shimez/M5ChainOSC/releases/tag/v1.6.1
[1.6.0]: https://github.com/shimez/M5ChainOSC/releases/tag/v1.6.0
[1.5.3]: https://github.com/shimez/M5ChainOSC/releases/tag/v1.5.3
[1.5.2]: https://github.com/shimez/M5ChainOSC/releases/tag/v1.5.2
[1.5.1]: https://github.com/shimez/M5ChainOSC/releases/tag/v1.5.1
