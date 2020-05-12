// サイズが大きいためPartition SchemeをMinimal SPIFFS(1.9MB App with OTA)にてコンパイルする必要あり。
// conifg.h.exampleをconfig.hにリネームし、各種設定をしてください
#include "config.h"

#include "felica_lite.h"
#include "BLEDevice.h"
#include <ArduinoOTA.h>



RCS620S rcs620s;
FelicaLite felica(rcs620s, masterKey);



#include "DFRobotDFPlayerMini.h"

DFRobotDFPlayerMini myDFPlayer;
HardwareSerial mySoftwareSerial(1);

TaskHandle_t th; //ESP32 マルチタスク　ハンドル定義
boolean isThRun;
SemaphoreHandle_t task_mux;

#include <queue>;
std::queue<int> playNoQueue;



void toggleLockSesame();
int getSesameLockStatus();
bool sesameLockOrUnLock(bool lock);
bool checkSesameTask(String task_id);
void playSound(int no);

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice);
};

class MyClientCallback : public BLEClientCallbacks {
  void onConnect(BLEClient* pclient);
  void onDisconnect(BLEClient* pclient);
};

// The remote service we wish to connect to.
BLEUUID serviceUUID("00001523-1212-efde-1523-785feabcd123");

// The characteristic of the remote service we are interested in.
BLEUUID    charUUIDCmd(         "00001524-1212-efde-1523-785feabcd123");
BLEUUID    charUUIDStatus(      "00001526-1212-efde-1523-785feabcd123");
BLEUUID    charUUIDAngleStatus( "00001525-1212-efde-1523-785feabcd123");

boolean doConnect = false;
boolean connected = false;
boolean doScan = false;
boolean doConnecting = false;
BLEClient*  pClient;
BLERemoteCharacteristic* pRemoteCharacteristic;
BLERemoteCharacteristic* pRemoteCharacteristicStatus;
BLERemoteCharacteristic* pRemoteCharacteristicAngleStatus;
BLEAddress bleAddress("");
std::string manufacturerDataMacDataString;

bool isLock = false;
uint_fast8_t lockStatusSet = 0;



// ボタン関連
//#include <JC_Button.h>        // https://github.com/JChristensen/JC_Button
//Button myBtn(27);


void setup() {
  Serial.begin(115200);
  Serial.println("Starting Arduino BLE Client application...");

  // 内蔵LED初期化
  pinMode(2, OUTPUT);

  // ボタン初期化
  //myBtn.begin();

  BLEDevice::init("");

  // Retrieve a Scanner and set the callback we want to use to be informed when we
  // have detected a new device.  Specify that we want active scanning and start the
  // scan to run for 5 seconds.

  pClient  = BLEDevice::createClient();
  pClient->setClientCallbacks(new MyClientCallback());

  if (address == "") {
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    // pBLEScan->setInterval(1349);
    // pBLEScan->setWindow(449);
    // https://electronics.stackexchange.com/questions/82098/ble-scan-interval-and-window
    pBLEScan->setInterval(40);
    pBLEScan->setWindow(30);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(10, false);
  } else {
    bleAddress = BLEAddress(address);
    manufacturerDataMacDataString = std::string(reinterpret_cast< char const* >(manufacturerDataMacData), sizeof(manufacturerDataMacData));
    //connectToServer();
  }


  int ret;

  mySoftwareSerial.begin(9600, SERIAL_8N1, 21, 22);  // speed, type, RX, TX

  // RC-S620Sの初期化
  Serial2.begin(115200);
  ret = rcs620s.initDevice();
  while (!ret) {}
  rcs620s.timeout = COMMAND_TIMEOUT;
  Serial.println("Initializing rcs620s...OK");

  
  // connect to wifi.
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  Serial.print("connecting");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println();
  Serial.print("connected: ");
  Serial.println(WiFi.localIP());
  configTime(9 * 3600L, 0, "ntp.nict.jp", "time.google.com", "ntp.jst.mfeed.ad.jp");


  if (!myDFPlayer.begin(mySoftwareSerial, true, false)) {  //Use softwareSerial to communicate with mp3.
    Serial.println(F("Unable to begin:"));
    Serial.println(F("1.Please recheck the connection!"));
    Serial.println(F("2.Please insert the SD card!"));
    while (true) {
      delay(0); // Code to compatible with ESP8266 watch dog.
    }
  }
  Serial.println(F("DFPlayer Mini online."));

  myDFPlayer.volume(30);  //Set volume value. From 0 to 30

  // Hostname defaults to esp3232-[MAC]
  ArduinoOTA.setHostname("smartlock-esp32");

  // No authentication by default
  // ArduinoOTA.setPassword("admin");

  // Password can be set with it's md5 value as well
  // MD5(admin) = 21232f297a57a5a743894a0e4a801fc3
  // ArduinoOTA.setPasswordHash("21232f297a57a5a743894a0e4a801fc3");

  ArduinoOTA
  .onStart([]() {
    String type;
    if (ArduinoOTA.getCommand() == U_FLASH)
      type = "sketch";
    else // U_SPIFFS
      type = "filesystem";

    // NOTE: if updating SPIFFS this would be the place to unmount SPIFFS using SPIFFS.end()
    Serial.println("Start updating " + type);
  })
  .onEnd([]() {
    Serial.println("\nEnd");
  })
  .onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  })
  .onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

// This is the Arduino main loop function.
void loop() {

  ArduinoOTA.handle(); 
  
  // bleサンプルのままのscan後に接続しにいく箇所。直接address指定していれば不要。
  // // If the flag "doConnect" is true then we have scanned for and found the desired
  // // BLE Server with which we wish to connect.  Now we connect to it.  Once we are 
  // // connected we set the connected flag to be true.
//  if (doConnect == true) {
//    if (connectToServer()) {
//      Serial.println("We are now connected to the BLE Server.");
//    } else {
//      Serial.println("We have failed to connect to the server; there is nothin more we will do.");
//    }
//    doConnect = false;
//  }
//  if (connected) {
//  } else if(doScan) {
//    BLEDevice::getScan()->start(0);  // this is just eample to start scan after disconnect, most likely there is better way to do it in arduino
//  }

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

        playSound(1);
        connectToServer();

      } else {
        Serial.println("auth ng!");
      }
    }

    // 通常は使用しないためコメントアウト。わずかだがreadで待ちもでるため。
    // 個別化カード鍵の発行
//    String str = Serial.readStringUntil('\n');
//    str.trim();
//    if (str != "") {
//      Serial.println("serial input: " + str);
//    }
//    if (str == "i") {
//      Serial.println("issuanceFelica");
//      felica.issuanceFelica();
//    }

    lastTouch = true;
  } else {
    lastTouch = false;
  }

  //停波
  rcs620s.rfOff();
  

  // ボタン処理
//  myBtn.read();
//  
//  static bool isLongPressed = false;
//  if (myBtn.wasReleased()) {
//    if (isLongPressed) {
//      Serial.println("button long press.");
//      if (connected) {
//        Serial.println("disconnect");
//        pClient->disconnect();
//      } else {
//        Serial.println("connectToServer");
//        connectToServer();
//      }
//
//      isLongPressed = false;
//    } else {
//      Serial.println("button press.");
//      if (!connected) {
//        Serial.println("not connected. connectToServer");
//        connectToServer();
//      } else {
//        if (isLock) {
//          Serial.println("unlock");
//          lock(2);
//        } else {
//          Serial.println("lock");
//          lock(1);
//        }
//      }
//    }
//  }
//  if (myBtn.pressedFor(1000)) { // 長押し
//    isLongPressed = true;
//  }


  // 接続した後にtoggleするサンプル
  if (lockStatusSet == 1) {
    if (isLock) {
      playSound(3);
      lock(2);
    } else {
      playSound(2);
      lock(1);
    }
  
    pClient->disconnect();
    lockStatusSet = 2;
  }
  
  delay(POLLING_INTERVAL);
}


bool connectToServer() {
  // リトライ行う。
  for (int i=0; i < 3; i++) {
    if (connectToServerInner()) {
      return true;
    }
  }
  return false;
}

bool connectToServerInner() {
    Serial.print("Forming a connection to ");
    Serial.println(bleAddress.toString().c_str());
    
    // 再利用されるようにsetupへ移動。再利用されればgetServiceのキャッシュがきくはずなので。
    // pClient  = BLEDevice::createClient();
    // Serial.println(" - Created client");

    // pClient->setClientCallbacks(new MyClientCallback());

    doConnecting = true;

    // Connect to the remove BLE Server.
    if (!pClient->connect(bleAddress, BLE_ADDR_TYPE_RANDOM)) {
      Serial.println(" - connect failure. return false.");
      return false;
    }
    Serial.println(" - Connected to server");

    if (!doConnecting) {
      return false;
    }

    // Obtain a reference to the service we are after in the remote BLE server.
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
      Serial.print("Failed to find our service UUID: ");
      Serial.println(serviceUUID.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our service");

    subscribeStatus(pRemoteService);
    subscribeAngleStatus(pRemoteService);

    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUIDCmd);
    if (pRemoteCharacteristic == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(charUUIDCmd.toString().c_str());
      pClient->disconnect();
      return false;
    }
    Serial.println(" - Found our characteristic Cmd");

    // Read the value of the characteristic.
    if(pRemoteCharacteristic->canRead()) {
      std::string value = pRemoteCharacteristic->readValue();
      Serial.print("The characteristic value was: ");
      Serial.println(string_to_hex(value).c_str());
      //Serial.println(value.length()); // cmdだと3
    } else {
      Serial.println(" - characteristic cant't read");
      return false;
    }

    // lock(0)だとErrorUnknownCmdになるが、認証することで現在の角度がわかるため実行する
    lock(0);

    connected = true;
    // 内蔵LEDをONにする
    digitalWrite(2, HIGH);

    return true;
}

void subscribeStatus(BLERemoteService* pRemoteService) {
  
    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristicStatus = pRemoteService->getCharacteristic(charUUIDStatus);
    if (pRemoteCharacteristicStatus == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(charUUIDStatus.toString().c_str());
//      pClient->disconnect();
      //return false;
    }
    Serial.println(" - Found our characteristic status");

    if(pRemoteCharacteristicStatus->canNotify()) {
      pRemoteCharacteristicStatus->registerForNotify(notifyCallbackStatus);
    }
}


void subscribeAngleStatus(BLERemoteService* pRemoteService) {
  
    // Obtain a reference to the characteristic in the service of the remote BLE server.
    pRemoteCharacteristicAngleStatus = pRemoteService->getCharacteristic(charUUIDAngleStatus);
    if (pRemoteCharacteristicAngleStatus == nullptr) {
      Serial.print("Failed to find our characteristic UUID: ");
      Serial.println(charUUIDAngleStatus.toString().c_str());
//      pClient->disconnect();
      //return false;
    }
    Serial.println(" - Found our characteristic angle status");

    if(pRemoteCharacteristicAngleStatus->canNotify()) {
      pRemoteCharacteristicAngleStatus->registerForNotify(notifyCallbackAngleStatus);
    }
}

static void notifyCallbackStatus(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify) {
    Serial.print("Notify callback for characteristic status");
    Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
    Serial.print(" of data length ");
    Serial.println(length);

    std::vector<byte> data(pData, pData + length);
    std::string dataString = vector_to_hex(data);

    Serial.print("data: ");
    Serial.println(dataString.c_str());

    uint32_t sn;
    memcpy(&sn, pData+6, 4);
    sn++;
    Serial.printf("sn: %u\n", sn);
    uint8_t err = *(pData+14);
    std::vector<std::string> errMsgs =
    {
      "Timeout",
      "Unsupported",
      "Success",
      "Operating",
      "ErrorDeviceMac",
      "ErrorUserId",
      "ErrorNumber",
      "ErrorSignature",
      "ErrorLevel",
      "ErrorPermission",
      "ErrorLength",
      "ErrorUnknownCmd",
      "ErrorBusy",
      "ErrorEncryption",
      "ErrorFormat",
      "ErrorBattery",
      "ErrorNotSend"
    };
    Serial.printf("status update %s, sn=%d, err=%s\n", pData, sn, errMsgs[err+1].c_str());
}


static void notifyCallbackAngleStatus(
  BLERemoteCharacteristic* pBLERemoteCharacteristic,
  uint8_t* pData,
  size_t length,
  bool isNotify)
{
  // Serial.print("Notify callback for characteristic angle status");
  // Serial.print(pBLERemoteCharacteristic->getUUID().toString().c_str());
  // Serial.print(" of data length ");
  // Serial.println(length);

  std::vector<byte> data(pData, pData + length);
  std::string dataString = vector_to_hex(data);

  // Serial.print("data: ");
  // Serial.println(dataString.c_str());

  uint16_t angleRaw;
  memcpy(&angleRaw, pData+2, 2);
  // Serial.printf("angleRaw = %d\n", angleRaw);
  double xx = (((double)angleRaw)/1024*360);
  // Serial.printf("xx = %lf.\n", xx);
  double angle = std::floor(xx);

  if (angle < lockMinAngle || angle > lockMaxAngle) {
    isLock = true;
  } else {        
    isLock = false;
  }
  Serial.printf("angle = %lf. lockStatus:%s\n", angle, isLock ? "true" : "false");

  // 初回ロック状態取得時の処理
  // 当初はここでコールバックメソッドを呼ぶ形にしようとしたが、ここでlockを呼ぶとpRemoteCharacteristic->canNotify()が動かなかった。ここ自体がBLEのコールバック処理なのでその中で新たなBLEのread, write処理はまずいのかもしれない。
  if (lockStatusSet == 0) {
    lockStatusSet = 1;   
  
//    if (isLock) {
//      playSound(3);
//      lock(2);
//    } else {
//      playSound(2);
//      lock(1);
//    }
  }
}

void MyClientCallback::onConnect(BLEClient* pclient) {
  Serial.println("onConnect");
}

void MyClientCallback::onDisconnect(BLEClient* pclient) {
  connected = false;
  doConnecting = false;
  lockStatusSet = 0;
  Serial.println("onDisconnect");
  // 内蔵LEDをOFFにする
  digitalWrite(2, LOW);
}

std::vector<byte> sign(uint8_t code, std::string payload, std::string password, std::string macData, std::string userId, uint32_t nonce) {

  byte md5Result[16];
  byte hmacResult[32];

  //  Serial.printf("macData: %s\n", macData.c_str());
  // Serial.printf("payload: %s, len:%d\n", payload.c_str(), payload.length());

  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_MD5;

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 0);
  mbedtls_md_starts(&ctx);
  mbedtls_md_update(&ctx, (const unsigned char *) userId.c_str(), userId.length());
  mbedtls_md_finish(&ctx, md5Result);
  mbedtls_md_free(&ctx);  

  // Serial.print("md5: ");
 
  // for(int i= 0; i< sizeof(md5Result); i++){
  //     char str[3];
 
  //     sprintf(str, "%02x", (int)md5Result[i]);
  //     Serial.print(str);
  // }
  // Serial.println("");

  size_t buf_length = payload.length() + 59;
  byte *buf = new byte[buf_length];
  // macData.copy(buf, 32); // len = 6
  memcpy(buf+32, macData.c_str(), 6);
  memcpy(buf+38, md5Result, 16);
  memcpy(buf+54, (uint32_t *) &nonce, 4);
  memcpy(buf+58, (uint8_t *) &code, 1);
  memcpy(buf+59, payload.c_str(), payload.length());

  // Serial.printf("pass length: %d\n", password.length());
  // Serial.printf("pass: %s\n", password.c_str());
  size_t password_buf_length = password.length()/2;
  unsigned char *password_buf = new unsigned char[password_buf_length];
  fromHex(password, password_buf);

  // Serial.print("pass from byte: ");
  // for(int i= 0; i< password_buf_length; i++){
  //     char str[3];
 
  //     sprintf(str, "%02x", (int)password_buf[i]);
  //     Serial.print(str);
  // }
  // Serial.println("");
  
  size_t buf_hmac_data_length = 59 - 32 + payload.length();
  byte *buf_hmac_data = new byte[buf_hmac_data_length];
  memcpy(buf_hmac_data, buf+32, buf_hmac_data_length);

  // Serial.print("buf_hmac_data: ");
  // for(int i= 0; i< buf_hmac_data_length; i++){
  //     char str[3];
 
  //     sprintf(str, "%02x", (int)buf_hmac_data[i]);
  //     Serial.print(str);
  // }
  // Serial.println("");

  md_type = MBEDTLS_MD_SHA256;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char *) password_buf, password_buf_length);
  mbedtls_md_hmac_update(&ctx, (const unsigned char *) buf_hmac_data, buf_hmac_data_length);
  mbedtls_md_hmac_finish(&ctx, hmacResult);
  mbedtls_md_free(&ctx);

  delete[] buf_hmac_data;
  delete[] password_buf;
         
  // Serial.print("Hash: ");
  // for(int i= 0; i< sizeof(hmacResult); i++){
  //     char str[3];
 
  //     sprintf(str, "%02x", (int)hmacResult[i]);
  //     Serial.print(str);
  // }
  // Serial.println("");

  memcpy(buf, hmacResult, 32);

  std::vector<byte> bufVector(buf, buf + buf_length);
  //std::string buf_str((char *)buf); stringだと途中に0が入った場合に終端扱いになりデータが切れるのでvectorにしたが、コンストラクタにlengthを渡せばstringでも大丈夫なのを後ほど知る
  delete[] buf;
  return bufVector;
}


// https://stackoverflow.com/questions/3381614/c-convert-string-to-hexadecimal-and-vice-versa
std::string vector_to_hex(const std::vector<byte>& input)
{
    static const char* const lut = "0123456789ABCDEF";
    size_t len = input.size();

    std::string output;
    output.reserve(2 * len);
    for (size_t i = 0; i < len; ++i)
    {
        const unsigned char c = input[i];
        output.push_back(lut[c >> 4]);
        output.push_back(lut[c & 15]);
    }
    return output;
}

std::string string_to_hex(const std::string& input)
{
    static const char* const lut = "0123456789ABCDEF";
    size_t len = input.size();

    std::string output;
    output.reserve(2 * len);
    for (size_t i = 0; i < len; ++i)
    {
        const unsigned char c = input[i];
        output.push_back(lut[c >> 4]);
        output.push_back(lut[c & 15]);
    }
    return output;
}

// https://tweex.net/post/c-anything-tofrom-a-hex-string/
void fromHex(
    const std::string &in,     //!< Input hex string
    void *const data           //!< Data store
    )
{
    size_t          length    = in.length();
    unsigned char   *byteData = reinterpret_cast<unsigned char*>(data);
    
    std::stringstream hexStringStream; hexStringStream >> std::hex;
    for(size_t strIndex = 0, dataIndex = 0; strIndex < length; ++dataIndex)
    {
        // Read out and convert the string two characters at a time
        const char tmpStr[3] = { in[strIndex++], in[strIndex++], 0 };

        // Reset and fill the string stream
        hexStringStream.clear();
        hexStringStream.str(tmpStr);

        // Do the conversion
        int tmpValue = 0;
        hexStringStream >> tmpValue;
        byteData[dataIndex] = static_cast<unsigned char>(tmpValue);
    }
}

inline uint32_t swap32(uint32_t value)
{
    uint32_t ret;
    ret  = value              << 24;
    ret |= (value&0x0000FF00) <<  8;
    ret |= (value&0x00FF0000) >>  8;
    ret |= value              >> 24;
    return ret;
}


bool lock(uint8_t cmdValue) {
  // Read the value of the characteristic.
  if(pRemoteCharacteristicStatus->canRead()) {
    std::string value = pRemoteCharacteristicStatus->readValue();
    Serial.print("The characteristic value was: ");
    Serial.println(string_to_hex(value).c_str());
    //Serial.println(value.length()); // statusだと15

    std::string substr = value.substr(6,4);
    uint32_t sn = substr[0] + (substr[1] << 8) + (substr[2] << 16) + (substr[3] << 24) + 1;
    //Serial.printf("%d %d %d %d\n", substr[0], substr[1], substr[2], substr[3]);
    Serial.printf("sn: %u\n", sn);

    std::string macData = manufacturerDataMacDataString;

    //Serial.print("macData:");
    //Serial.println(string_to_hex(macData).c_str());
    // Serial.println(macData.length());

    //uint8_t cmdValue = 0;
    //cmdValue = 1; // lock
    //cmdValue = 2; // unlock
    std::vector<byte> payload = sign(cmdValue, std::string(""), password, macData.substr(3), userId, sn);        
    
    Serial.printf("write payload hex: %s\n", vector_to_hex(payload).c_str());

    write(payload);
    return true;
  } else {
    Serial.println(" - characteristic status cant't read");
    return false;
  }
}

void write(std::vector<byte> payload) {

  for(int i=0; i<payload.size(); i+=19) {
    size_t sz = std::min((int) payload.size() - i, 19);
    std::vector<byte> buf(sz + 1);
    if (sz < 19) {
      buf[0] = 4;
    } else if (i == 0) {
      buf[0] = 1;
    } else {
      buf[0] = 2;
    }

    copy(payload.begin()+i, payload.begin()+(i+sz), buf.begin() + 1);
    pRemoteCharacteristic->writeValue(buf.data(), buf.size(), false);
  }
}



/**
 * Scan for BLE servers and find the first one that advertises the service we are looking for.
 */
void MyAdvertisedDeviceCallbacks::onResult(BLEAdvertisedDevice advertisedDevice) {
  Serial.print("BLE Advertised Device found: ");
  Serial.println(advertisedDevice.toString().c_str());

  // We have found a device, let us now see if it contains the service we are looking for.
  if (advertisedDevice.haveServiceUUID() && advertisedDevice.isAdvertisingService(serviceUUID)) {

    BLEDevice::getScan()->stop();

    bleAddress = BLEAddress(advertisedDevice.getAddress());
    manufacturerDataMacDataString = advertisedDevice.getManufacturerData();
    
    doConnect = true;
    doScan = true;

  } // Found our server
} // onResult














void playSound(int no) {
  Serial.printf("playSound no. %d\n", no );

  playNoQueue.push(no);
  // すでに再生中のタスクがある場合は抜ける。
  if (playNoQueue.size() >= 2) {
    return;
  }

  isThRun = true;
  if (th == NULL) {
    task_mux = xSemaphoreCreateBinary();
    int* arg = (int*)malloc(sizeof(int));
    *arg = no;
    xTaskCreatePinnedToCore(Task1, "Task1", 4096, (void*)arg, 5, &th, 0); //マルチタスク core 0 実行
  }
}

void printDetail(uint8_t type, int value) {
  switch (type) {
    case TimeOut:
      Serial.println(F("Time Out!"));
      break;
    case WrongStack:
      Serial.println(F("Stack Wrong!"));
      break;
    case DFPlayerCardInserted:
      Serial.println(F("Card Inserted!"));
      break;
    case DFPlayerCardRemoved:
      Serial.println(F("Card Removed!"));
      break;
    case DFPlayerCardOnline:
      Serial.println(F("Card Online!"));
      break;
    case DFPlayerUSBInserted:
      Serial.println("USB Inserted!");
      break;
    case DFPlayerUSBRemoved:
      Serial.println("USB Removed!");
      break;
    case DFPlayerPlayFinished:
      {
        Serial.print(F("Number:"));
        Serial.print(value);
        Serial.println(F(" Play Finished!"));

        Serial.printf("MP3 done\n");

        int no = playNoQueue.front();
        Serial.printf("play finish no. %d\n", no );
        playNoQueue.pop();

        if (playNoQueue.size() >= 1) {

          no = playNoQueue.front();

          Serial.printf("next play no. %d\n", no );

          myDFPlayer.play(no);

          Serial.printf("MP3 playback begins...\n");

        } else {
          //      xTaskNotify(th, 0, eNoAction);
          //      xTaskNotifyGive(th);
          xSemaphoreGive(task_mux);
          th = NULL;
          vTaskDelete(NULL);
        }
        break;
      }
    case DFPlayerError:
      Serial.print(F("DFPlayerError:"));
      switch (value) {
        case Busy:
          Serial.println(F("Card not found"));
          break;
        case Sleeping:
          Serial.println(F("Sleeping"));
          break;
        case SerialWrongStack:
          Serial.println(F("Get Wrong Stack"));
          break;
        case CheckSumNotMatch:
          Serial.println(F("Check Sum Not Match"));
          break;
        case FileIndexOut:
          Serial.println(F("File Index Out of Bound"));
          break;
        case FileMismatch:
          Serial.println(F("Cannot Find File"));
          break;
        case Advertise:
          Serial.println(F("In Advertise"));
          break;
        default:
          break;
      }
      break;
    default:
      break;
  }

}


//************* マルチタスク **********************
void Task1(void *pvParameters) {
  int* arg = (int*) pvParameters;
  int no = *arg;
  free(arg);

  Serial.printf("Task1 play no. %d\n", no );

  myDFPlayer.play(no);  //Play the first mp3

  Serial.printf("MP3 playback begins...\n");

  //  while (1) {
  //    if (myDFPlayer.available()) {
  //      printDetail(myDFPlayer.readType(), myDFPlayer.read()); //Print the detail message from DFPlayer to handle different errors and states.
  //    }
  //    delay(100);
  //    //    delay(1);//マルチタスクのwhileループでは必ず必要
  //  }

  // 上記だと終了検知が再生開始と同時に来る場合があったので、下記参考に対応
  // 間違いの終了コードがくるみたいなので、最低再生時間までいかないぐらいの間でdelayして最初の読み取ったstateは無視するように対応。
  // https://forum.arduino.cc/index.php?topic=522677.0
  while (1) {
    delay(600);
    myDFPlayer.readState();
    int stt = myDFPlayer.readState();
    while ( stt == 529 || stt == 513 || stt == 512) {
      delay(300);
      stt = myDFPlayer.readState();
    }

    Serial.printf("MP3 done\n");

    int no = playNoQueue.front();
    Serial.printf("play finish no. %d\n", no );
    playNoQueue.pop();

    if (playNoQueue.size() >= 1) {

      no = playNoQueue.front();

      Serial.printf("next play no. %d\n", no );

      myDFPlayer.play(no);  //Play the first mp3
    } else {
      break;
    }
  }

  Serial.println("task end");

  //      xTaskNotify(th, 0, eNoAction);
  //      xTaskNotifyGive(th);
  xSemaphoreGive(task_mux);
  th = NULL;
  vTaskDelete(NULL);
}
