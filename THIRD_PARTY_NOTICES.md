# Third-Party Notices

M5ChainOSCの独自部分は、ルートの[MIT License](LICENSE)で提供されます。
コンパイル済みファームウェアには、以下の第三者ソフトウェアが含まれます。各コンポーネントには、それぞれのライセンスが適用されます。

## ビルド基盤

| Component | Version | License | Source |
| --- | --- | --- | --- |
| Arduino-ESP32 | 2.0.17 | LGPL-2.1 | <https://github.com/espressif/arduino-esp32/tree/2.0.17> |

Arduino-ESP32のLGPL-2.1ライセンス本文は[licenses/LGPL-2.1.txt](licenses/LGPL-2.1.txt)に収録しています。Arduino-ESP32およびその構成要素の対応ソース、著作権表示、追加のライセンス情報については、上記のバージョン固定リンクを参照してください。

## ライブラリ

| Component | Version / Revision | License | Copyright notice | Source |
| --- | --- | --- | --- | --- |
| M5Unified | 0.2.19 | MIT | Copyright (c) 2021 M5Stack | <https://github.com/m5stack/M5Unified> |
| M5GFX | 0.2.26 | MIT | Copyright (c) 2021 M5Stack | <https://github.com/m5stack/M5GFX> |
| M5Chain | 1.0.8 / `2f0e28c9dd290b5948eb51f0aa3b9e61d06858ad` | MIT | Copyright (c) 2026 M5Stack Technology CO LTD | <https://github.com/m5stack/M5Chain/tree/2f0e28c9dd290b5948eb51f0aa3b9e61d06858ad> |
| ArduinoOSC | 0.6.0 | MIT | Copyright (c) 2017 Hideaki Tai | <https://github.com/hideakitai/ArduinoOSC> |
| ArduinoJson | 6.21.6 | MIT | Copyright © 2014-2026 Benoit BLANCHON | <https://github.com/bblanchon/ArduinoJson/tree/v6.21.6> |
| ArxContainer | 0.7.0 | MIT | Copyright (c) 2019 Hideaki Tai | <https://github.com/hideakitai/ArxContainer> |
| ArxSmartPtr | 0.3.0 | MIT | Copyright (c) 2020 Hideaki Tai | <https://github.com/hideakitai/ArxSmartPtr> |
| ArxTypeTraits | 0.3.2 | MIT | Copyright (c) 2020 Hideaki Tai | <https://github.com/hideakitai/ArxTypeTraits> |
| DebugLog | 0.8.4 | MIT | Copyright (c) 2019 Hideaki Tai | <https://github.com/hideakitai/DebugLog> |

MITライセンスの全文はルートの[LICENSE](LICENSE)に収録しています。第三者コンポーネントの著作権は、それぞれの著作権者に帰属します。

## 再現可能なビルド

配布ファームウェアに対応するM5ChainOSCのソースコード、`platformio.ini`、ビルド手順はこのリポジトリで公開しています。リリース時のGitタグをチェックアウトし、PlatformIOで次を実行してください。

```sh
pio run -e atoms3r
```

PlatformIOとesptoolはビルドおよびファームウェア生成に使用するツールであり、M5ChainOSCのファームウェアへ組み込まれるコンポーネントとしては扱っていません。
