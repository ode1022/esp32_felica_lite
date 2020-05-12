# Felica Lite-S Library For ESP32
ESP32(Arduino)向けのFelica Lite-Sのライブラリーです。

下記機能があります。
- 内部認証
- カード鍵の発行

詳しい詳細は下記記事を参考

[Suica(IDm)で認証するのは危険なのでFelica Lite-Sの内部認証を使う](https://qiita.com/odetarou/items/bcd65dbfd1f68735ac30)

## サンプルコード

### esp32_felia_lite_test
Felica Lite-S内部認証とカード鍵の発行を行うサンプルです

### esp32_sesame_ble_felica
Felica Lite-Sの内部認証で認証後にスマートロックのSesameの解錠を行うコードです（自宅にて実稼働中）

