#include "felica_lite.h"

FelicaLite::FelicaLite(RCS620S &rcs620s, const std::string &masterKey) :
        rcs620s(rcs620s),    //  初期化子
        masterKey(masterKey)     //  初期化子  
{}

// https://stackoverflow.com/questions/3381614/c-convert-string-to-hexadecimal-and-vice-versa
std::string FelicaLite::vector_to_hex(const std::vector<byte>& input)
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

std::string FelicaLite::string_to_hex(const std::string& input)
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
void FelicaLite::fromHex(
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

inline uint32_t FelicaLite::swap32(uint32_t value)
{
    uint32_t ret;
    ret  = value              << 24;
    ret |= (value&0x0000FF00) <<  8;
    ret |= (value&0x00FF0000) >>  8;
    ret |= value              >> 24;
    return ret;
}


/*
  バイトオーダーを入れ替える(ポインタの先を直に入れ替えるので注意！)
*/
void FelicaLite::swapByteOrder(uint8_t inout[8])
{
  uint8_t swap[8];
  for (int i = 0; i < 8; i++)
  {
    swap[7 - i] = inout[i];
  }
  for (int i = 0; i < 8; i++)
  {
    inout[i] = swap[i];
  }
}









/*
  値の比較
*/
bool FelicaLite::compareBuf(const uint8_t val1[16], const uint8_t val2[16])
{
  printf("compareBuf start\n");
  bool rtn;
  rtn = true;
  for (int i = 0; i < 16; i++)
  {
    printf("%02X %02X\n", val1[i], val2[i]);
    if (val1[i] != val2[i]) rtn = false;
  }
  printf("compareBuf finish\n");

  return rtn;
}

/*
  カードリーダに接続
*/
int FelicaLite::connectRcs620s()
{
  //カードに接続
  int ret = rcs620s.initDevice();
  if (!ret) {
    printf("カード接続失敗\n");
    return false;
  }

  ret = rcs620s.polling();
  if (!ret) {
    printf("polling失敗\n");
    return false;
  }
  return true;
}

/*
  カードの正当性チェック(単独呼び出し用)
*/
bool FelicaLite::authFelica()
{
  int ret = connectRcs620s(); //カード接続
  ret = checkMac();
  rcs620s.rfOff();  //カード接続断

  if (ret == 0)
  {
    return true;
  } else {
    return false;
  }

}

/*
  カードの正当性チェック
  戻り値：
  カードが正当：0
  カードが不正：-1
  カード接続失敗：-2
*/
bool FelicaLite::checkMac()
{
  int ret;
  uint8_t RC[16];

  /* RC用に1～100の擬似乱数を16個生成 */
  srand((unsigned) time(NULL));
  for (int i = 1; i <= 16; i++) {
    RC[i] = (uint8_t)rand() % 100 + 1;
  }

  uint8_t RC1[8];
  uint8_t RC2[8];
  for (int i = 0; i < 8; i++)
  {
    RC1[i] = RC[i];
    RC2[i] = RC[8 + i];
  }

  //RCを書き込む
  if (!writeWithoutEncryption(READ_WRITE_MODE, ADDRESS_RC, RC)) {
    printf("writeWithoutEncryption失敗\n");
    return -2;
  }

  //IDとMAC_Aを読み出す
  uint8_t macBlock[44];
  if (!readIdWithMacA(macBlock)) {
    printf("readIdWithMacA失敗\n");
    return -2;
  }

  uint8_t BLOCK_LOW[8], BLOCK_HIGH[8];
  for (int i = 0; i < 8; i++)
  {
    BLOCK_LOW[i] = macBlock[12 + i];
    BLOCK_HIGH[i] = macBlock[12 + 8 + i];
  }

  uint8_t CARD_MAC_A[8];
  for (int i = 28; i < 36; i++)
  {
    CARD_MAC_A[i - 28] = macBlock[i];
  }

  //MACの比較
  if (compareMac(CARD_MAC_A, RC1, RC2, BLOCK_LOW, BLOCK_HIGH))
  {
    return 0;
  } else {
    return -1;
  }
}

/*
  MACの比較
*/
bool FelicaLite::compareMac(uint8_t CARD_MAC_A[8], uint8_t RC1[8], uint8_t RC2[8], uint8_t BLOCK_LOW[8], uint8_t BLOCK_HIGH[8])
{
  uint8_t ZERO[8] = { 0 };
  uint8_t IV1[8];
  uint8_t IV2[8];
  uint8_t SK1[8];
  uint8_t SK2[8];

  uint8_t ck[16];
  generatePersonalizedCK(ck);

  swapByteOrder(RC1);
  swapByteOrder(RC2);

  swapByteOrder(ck);
  swapByteOrder(ck+8);

  //SK1を生成
  tripleDes2KeyCbc(RC1, ZERO, ck, IV1);
  for (int i = 0; i < 8; i++)
  {
    SK1[i] = IV1[i];
  }

  // skの定義的にはバイトオーダー反転が必要だが、sk使用時にも反転で使うため意味ないのでスキップする
  //  swapByteOrder(SK1);

  //SK2を生成
  tripleDes2KeyCbc(RC2, IV1, ck, IV2);
  for (int i = 0; i < 8; i++)
  {
    SK2[i] = IV2[i];
  }
  //uint16_t ADDRESS_MAC_A = 0x91;

  uint8_t OUT_1[8];
  uint8_t OUT_2[8];
  uint8_t CALC_MAC_A[8];
  // skの定義的にはバイトオーダー反転が必要だが、sk使用時にも反転で使うため意味ないのでスキップする
  //  swapByteOrder(SK2);

  //MAC_Aを生成
  //uint16_t ADDRESS_ID = 0x82;

  uint8_t BlockInfo[8] = { (uint8_t)(ADDRESS_ID & 0xFF), 0, (uint8_t)(ADDRESS_MAC_A & 0xFF), 0, 0xFF, 0xFF, 0xFF, 0xFF };

  swapByteOrder(BlockInfo);
  swapByteOrder(BLOCK_LOW);
  swapByteOrder(BLOCK_HIGH);

  uint8_t sk[16];
  memcpy(sk, SK1, 8);
  memcpy(sk+8, SK2, 8);
  
  tripleDes2KeyCbc(BlockInfo, RC1, sk, OUT_1);
  tripleDes2KeyCbc(BLOCK_LOW, OUT_1, sk, OUT_2);
  tripleDes2KeyCbc(BLOCK_HIGH, OUT_2, sk, CALC_MAC_A);

  swapByteOrder(CALC_MAC_A);

  fprintf(stdout, "生成したMAC_A [");
  for (int i = 0; i < 8; i++)
  {
    printf("%02X ", CALC_MAC_A[i]);
  }
  printf("]\n");


  //内部認証（カードのMAC_Aと生成したMAC_Aを比較）
  fprintf(stdout, "カードのMAC_A [");
  for (int i = 0; i < 8; i++)
  {
    printf("%02X ", CARD_MAC_A[i]);
  }
  printf("]\n");

  for (int i = 0; i < 8; i++)
  {
    if (CALC_MAC_A[i] != CARD_MAC_A[i])
    {
      printf("MAC不一致\n");
      return false;
    }
  }
  printf("MAC一致\n");

  return true;
}

/*
  IDをMAC付で読み出す
*/
bool FelicaLite::readIdWithMacA(uint8_t felicaBlock[16]) {
  int ret;
  uint8_t buf[RCS620S_MAX_CARD_RESPONSE_LEN];
  uint8_t responseLen = 0;
  uint16_t serviceCode = 0x000b;

  buf[0] = 0x06;
  memcpy(buf + 1, rcs620s.idm, 8);
  buf[9] = 0x01;      // サービス数
  buf[10] = (uint8_t)((serviceCode >> 0) & 0xff);
  buf[11] = (uint8_t)((serviceCode >> 8) & 0xff);
  buf[12] = 0x02;     // ブロック数
  buf[13] = 0x80;
  buf[14] = 0x82;
  buf[15] = 0x80;
  buf[16] = 0x91;

  ret = rcs620s.cardCommand(buf, 17, buf, &responseLen);
  if (!ret) {
    return false;
  }

  printf("ID, MAC_A Block : ");
  for (int i = 0; i < 44; i++)
  {
    felicaBlock[i] = buf[i];
    printf("%02X ", buf[i]);
  }
  printf("\n");

  return true;
}

/*
  Read Without Encryption
*/
bool FelicaLite::readWithoutEncryption(uint8_t blockNumber, uint8_t felicaBlock[16]) {
  int ret;
  uint8_t buf[RCS620S_MAX_CARD_RESPONSE_LEN];
  uint8_t responseLen = 0;
  uint16_t serviceCode = 0x0009;

  buf[0] = 0x06;
  memcpy(buf + 1, rcs620s.idm, 8);
  buf[9] = 0x01;      // サービス数
  buf[10] = (uint8_t)((serviceCode >> 0) & 0xff);
  buf[11] = (uint8_t)((serviceCode >> 8) & 0xff);
  buf[12] = 0x01;     // ブロック数
  buf[13] = 0x80;
  buf[14] = blockNumber;

  ret = rcs620s.cardCommand(buf, 15, buf, &responseLen);

  if (!ret || (responseLen != 28) || (buf[0] != 0x07) ||
      (memcmp(buf + 1, rcs620s.idm, 8) != 0)) {
    printf("read faild.\n");
    return false;
  }

  for (int i = 12; i < 28; i++)
  {
    felicaBlock[i - 12] = buf[i];
  }

  return true;
}

/*
  Write Without Encryption
*/
bool FelicaLite::writeWithoutEncryption(uint16_t serviceCode, uint8_t blockNumber, const uint8_t felicaBlock[16]) {
  int ret;
  uint8_t buf[RCS620S_MAX_CARD_RESPONSE_LEN];
  uint8_t responseLen = 0;

  buf[0] = 0x08;
  memcpy(buf + 1, rcs620s.idm, 8);
  buf[9] = 0x01;      // サービス数
  buf[10] = (uint8_t)((serviceCode >> 0) & 0xff);
  buf[11] = (uint8_t)((serviceCode >> 8) & 0xff);
  buf[12] = 0x01;     // ブロック数
  buf[13] = 0x80;
  buf[14] = blockNumber;
  for (int i = 0; i < 16; i++)
  {
    buf[i + 15] = felicaBlock[i];
  }

  ret = rcs620s.cardCommand(buf, 31, buf, &responseLen);
  if (!ret || (responseLen != 11) || (buf[0] != 0x09) || (buf[9] != 0x00) || (buf[10] != 0x00)) {
    return false;
  }

  //  for(int i=12; i<28; i++)
  //  {
  //    felicaBlock[i - 12] = buf[i];
  //  }

  return true;
}

void FelicaLite::tripleDes2KeyCbc(uint8_t input[8], uint8_t iv[8], uint8_t input_key[16], uint8_t output[8])
{

  uint8_t input_key3[24];
  memcpy(input_key3, input_key, 16);
  memcpy(input_key3+16, input_key, 8);

  tripleDes3KeyCbc(input, iv, input_key3, output);
}

void FelicaLite::tripleDes3KeyCbc(uint8_t input[8], uint8_t iv[8], uint8_t input_key[24], uint8_t output[8])
{
  mbedtls_des3_context des3_ctx;
  mbedtls_des3_init(&des3_ctx);

  mbedtls_des3_set3key_enc(&des3_ctx, input_key);

  // ivの値が破壊されていたのでコピーしたものを使う。
  uint8_t iv_copy[8];
  memcpy(iv_copy, iv, 8);

  // xorの計算はここでやる必要はないみたい。xorをやってくれるのがcbcらしい。
  // https://github.com/hirokuma/NfcTest4/blob/a9c0c46e01e7ddfc36bfb4c58cf4f45304ef60cb/src/com/blogpost/hiro99ma/nfc/FelicaLiteIssuance.java#L591
  mbedtls_des3_crypt_cbc(&des3_ctx, MBEDTLS_DES_ENCRYPT, 8, iv_copy, input, output);

  mbedtls_des3_free(&des3_ctx);
}

/*
  個別化カード鍵の作成
*/
bool FelicaLite::generatePersonalizedCK(uint8_t CK[16]) {
  //IDブロックの値を読み出す
  uint8_t ID[16];
  if (!readWithoutEncryption(ADDRESS_ID, ID)) {
    printf("readWithoutEncryption失敗\n");
    return false;
  }

  printf("IDブロック　　 [");
  for (int i = 0; i < 16; i++)
  {
    printf("%02X ", ID[i]);
  }
  printf("]\n");

  return generatePersonalizedCKInner(ID, CK);
}

/*
  個別化カード鍵の作成 id引き渡し版（単体テスト用に切り出し)
*/
bool FelicaLite::generatePersonalizedCKInner(uint8_t ID[16], uint8_t CK[16])
{
  uint8_t ZERO[8] = { 0 };
  uint8_t ZERO_1B[8] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1b };
  uint8_t K1[8];
  uint8_t iv[8] = { 0 };

  uint8_t mk[24];
  std::string masterKeyCopy = masterKey;
  // https://teratail.com/questions/94151
  masterKeyCopy.erase(std::remove(masterKeyCopy.begin(), masterKeyCopy.end(), ':'), masterKeyCopy.end());
  //std::replace(masterKeyCopy.begin(), masterKeyCopy.end(), ':', '');
  fromHex(masterKeyCopy, mk);

  //0と個別化マスター鍵で3DES
  tripleDes3KeyCbc(ZERO, iv, mk, K1);
  
  bool msb = false;
  for (int i = 7; i >= 0; i--)
  {
    bool bak = msb;
    msb = ((K1[i] & 0x80) != 0) ? true : false;
    K1[i] = K1[i] << 1;
    if (bak)
    {
      //下のバイトからのcarry
      K1[i] = K1[i] | 0x01;
    }
  }

  //Lの最上位ビットが1の場合、最下位バイトと0x1bをXORする
  if (msb)
  {
    K1[7] = K1[7] ^ 0x1b;
  }

  uint8_t ID1[8];
  uint8_t ID2[8];

  //  //Mを先頭から8byteずつに分け、M1, M2*とする(M2* xor K1 → M2)
  for (int i = 0; i < 8; i++)
  {
    ID1[i] = ID[i];
    ID2[i] = ID[8 + i] ^ K1[i];
  }

  //M1を平文、Kを鍵として3DES→結果C1
  uint8_t C1[8];
  tripleDes3KeyCbc(ID1, iv, mk, C1);

  //C1とM2をXORした結果を平文、Kを鍵として3DES→結果T
  uint8_t T1[8];
  memcpy(iv, C1, 8);
  tripleDes3KeyCbc(ID2, iv, mk, T1);

  //M1の最上位ビットを反転→M1'
  ID1[0] = ID1[0] ^ 0x80;

  //M1'を平文、Kを鍵として3DES→結果C1'
  uint8_t C1_1[8];
  memset(iv, 0, 8);
  tripleDes3KeyCbc(ID1, iv, mk, C1_1);

  // (C1' xor M2)を平文、Kを鍵として3DES→結果T'
  memcpy(iv, C1_1, 8);
  uint8_t T1_1[8];
  tripleDes3KeyCbc(ID2, iv, mk, T1_1);

  //Tを上位8byte、T'を下位8byte→結果C→個別化カード鍵
  for (int i = 0; i < 8; i++)
  {
    CK[i] = T1[i];
    CK[8 + i] = T1_1[i];
  }

  printf("個別化カード鍵 [");
  for (int i = 0; i < 16; i++)
  {
    printf("%02X ", CK[i]);
  }
  printf("]\n");

  return true;

}

/*
  カードへ個別化カード鍵の発行
*/
bool FelicaLite::issuanceFelica()
{

  int ret;
  if (!connectRcs620s()) return false; //カード接続

  //カード鍵の書き込み
  //カード鍵の設定（書き込み） ブロック135番(0x87番=CKブロック)の内容を16進数[bb]で書き込む
  uint8_t CK[16];
  if (!generatePersonalizedCK(CK))
  {
    printf("カード鍵の取得 失敗\n");
    rcs620s.rfOff();  //カード接続断
    return false;
  }
  ret = writeWithoutEncryption(READ_WRITE_MODE, ADDRESS_CK, CK);

  //書き込んだカード鍵の内容を確認
  if (checkMac() != 0)
  {
    printf("書き込んだカード鍵の内容を確認 失敗\n");
    rcs620s.rfOff();  //カード接続断
    return false;
  }

  //鍵バージョンの書き込み
  //ブロック134番(0x86番=CKVブロック)の内容を16進数[bb]で書き込む
  // 3つめの引数CKVはconstのためコンパイルエラーでたのでメソッド側をconstにした。
  // ちなみにメソッド側変えられない場合はconst_cast<uint8_t*>(CKV)が使えるみたい。
  // https://tmatsuu.hatenadiary.org/entry/20090717/1247835994
  ret = writeWithoutEncryption(READ_WRITE_MODE, ADDRESS_CKV, CKV);
  if (!ret)
  {
    printf("鍵バージョンの書き込み 失敗\n");
    rcs620s.rfOff();  //カード接続断
    return false;
  }

  uint8_t Val[16];
  //書き込んだCKVと読み出したCKVを比較
  readWithoutEncryption(ADDRESS_CKV, Val);
  if (!ret || !compareBuf(CKV, Val))
  {
    printf("書き込んだCKVと読み出したCKVを比較 失敗\n");
    rcs620s.rfOff();  //カード接続断
    return false;
  }

  rcs620s.rfOff();  //カード接続断

  printf("個別化カード鍵の発行　正常終了\n");
  return true;
}