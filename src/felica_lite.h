
#ifndef FELICA_LITE_H
#define FELICA_LITE_H

#include <Arduino.h>
#include "mbedtls/md.h"
#include <rcs620s.h>

#define MBEDTLS_DES_C 1
#include <des.h>

#include <sstream>
#include <iomanip>
#include <algorithm>

#define COMMAND_TIMEOUT  400
#define POLLING_INTERVAL 50


class FelicaLite
{
public:
    FelicaLite(RCS620S &rcs620s, const std::string &masterKey);

    int connectRcs620s();
    bool compareBuf(const uint8_t val1[16], const uint8_t val2[16]);
    bool authFelica();
    bool checkMac();
    bool compareMac(uint8_t CARD_MAC_A[8], uint8_t RC1[8], uint8_t RC2[8], uint8_t BLOCK_HIGH[8], uint8_t BLOCK_LOW[8]);
    bool readIdWithMacA(uint8_t macBlock[16]);
    bool writeWithoutEncryption(uint16_t serviceCode, uint8_t blockNumber, const uint8_t felicaBlock[16]);
    bool readWithoutEncryption(uint8_t blockNumber, uint8_t felicaBlock[16]);
    void tripleDES(uint8_t input[8], uint8_t XorInput[8], uint8_t input_key1[8], uint8_t input_key2[8], uint8_t input_key3[8], uint8_t output[8]);
    bool getCK(uint8_t CK1[8], uint8_t CK2[8]);
    bool generatePersonalizedCK(uint8_t CK[16]);
    bool issuanceFelica();
    void tripleDes2KeyCbc(uint8_t input[8], uint8_t iv[8], uint8_t input_key[16], uint8_t output[8]);
    void tripleDes3KeyCbc(uint8_t input[8], uint8_t iv[8], uint8_t input_key[24], uint8_t output[8]);
    bool generatePersonalizedCKInner(uint8_t ID[16], uint8_t CK[16]);

private:
    std::string vector_to_hex(const std::vector<byte>& input);
    std::string string_to_hex(const std::string& input);
    void fromHex(
    const std::string &in,     //!< Input hex string
    void *const data           //!< Data store
    );
    uint32_t swap32(uint32_t value);
    void swapByteOrder(uint8_t inout[8]);

public:
    RCS620S &rcs620s;
    const std::string &masterKey;
};

//定数
const uint16_t ADDRESS_RC = 0x80;
const uint16_t ADDRESS_ID = 0x82;
const uint16_t ADDRESS_CKV = 0x86;
const uint16_t ADDRESS_CK = 0x87;
const uint16_t ADDRESS_MAC_A = 0x91;
const uint16_t READ_WRITE_MODE = 0x0009;
const uint16_t READ_ONLY_MODE = 0x000b;

//鍵バージョンの書き込み
// 先頭 2 バイトのみ任意 の値に書き換えが可能です。データ配置を図 3-12 に示します。(FeliCa Lite-S ユーザーズマニュアル P27より)
const uint8_t  CKV[16] = {0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };




#endif /* !FELICA_LITE_H */