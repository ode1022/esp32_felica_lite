// conifg.h.exampleをconfig.hにリネームし、各種設定をしてください
#include "config.h"

#include "felica_lite.h"

RCS620S rcs620s;
FelicaLite felica(rcs620s, masterKey);

void setup() {
  Serial.begin(115200);
  
  // 内蔵LED初期化
  pinMode(2, OUTPUT);

  // 個別化カード鍵の作成テスト
  // uint8_t CK[16];
  // // 先頭 8byteがIDm
  // uint8_t ID[16] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };
  // felica.generatePersonalizedCKInner(ID, CK);
  
  int ret;

  // RC-S620Sの初期化
  Serial2.begin(115200);
  ret = rcs620s.initDevice();
  while (!ret) {}
  rcs620s.timeout = COMMAND_TIMEOUT;
  Serial.println("Initializing rcs620s...OK");
}

// This is the Arduino main loop function.
void loop() {

  int ret ;
  String IDm = "";
  char buf[30];
  static bool lastTouch = false;

  // FeliCaのタッチ状態を得る
  ret = rcs620s.polling();

  // FeliCaがタッチされた場合
  if (ret) {
    // 連続で処理されないように最初の1度のみ処理。
    if (!lastTouch) {
      // IDmを取得する
      for (int i = 0; i < 8; i++) {
        sprintf(buf, "%02X", rcs620s.idm[i]);
        IDm += buf;
      }

      // IDmを表示する
      Serial.println(IDm);

      Serial.println("authFelica");
      if (felica.authFelica()) {
        Serial.println("auth ok!");
      } else {
        Serial.println("auth ng!");
      }
    }

    // 個別化カード鍵の発行
    String str = Serial.readStringUntil('\n');
    str.trim();
    if (str != "") {
      Serial.println("serial input: " + str);
    }
    if (str == "i") {
      Serial.println("issuanceFelica");
      felica.issuanceFelica();
    }

    lastTouch = true;
  } else {
    lastTouch = false;
  }

  //停波
  rcs620s.rfOff();
  
  delay(POLLING_INTERVAL);
}
