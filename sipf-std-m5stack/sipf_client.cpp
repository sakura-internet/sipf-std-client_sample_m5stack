/*
 * Copyright (c) 2021 Sakura Internet Inc.
 *
 * SPDX-License-Identifier: MIT
 */
#include <Arduino.h>
#include "sipf_client.h"
#include <stdio.h>
#include <string.h>

static char cmd[256];

//UARTの受信バッファを読み捨てる
void SipfClientFlushReadBuff(void)
{
    int len;
    uint8_t b;
    len = Serial2.available();
    for (int i = 0; i < len; i++) {
        Serial2.read();
    }
}

//１行([CR] or [LF]の手前まで)をバッファに詰める
int SipfUtilReadLine(uint8_t *buff, int buff_len, int timeout_ms)
{
    uint32_t t_recved, t_now;
    int ret;
    int len, idx = 0;
    uint8_t b;

    memset(buff, 0, buff_len);
    t_recved = millis();
    for (;;) {
        t_now = millis();
        len = Serial2.available();
        for (int i = 0; i < len; i++) {
            ret = Serial2.read();;
            if (ret >= 0) {
                b = (uint8_t)ret;
                //
                if (idx < buff_len) {
                    //行末を判定
                    if ((b == '\r') || (b == '\n')) {
                        buff[idx] = '\0';
                        return idx + 1; //長さを返す
                    }
                    //バッファに詰める
                    buff[idx] = b;
                    idx++;
                }
            }
            t_recved = t_now;
        }
        //タイムアウト判定
        if ((int32_t)((t_recved + timeout_ms) - t_now) < 0) {
            //キャラクター間タイムアウト
            return -3;
        }
    }
    return idx; //読み込んだ長さ
}

/**
 * $Wコマンドを送信
 */
static int sipfSendW(uint8_t addr, uint8_t value)
{
    int len;
    int ret;
    
    //UART受信バッファを読み捨てる
    SipfClientFlushReadBuff();

    // $Wコマンド送信
    len = sprintf(cmd, "$W %02X %02X\r\n", addr, value);
    ret = Serial2.write((uint8_t*)cmd, len);

    // $Wコマンド応答待ち
    for (;;) {
        ret = SipfUtilReadLine((uint8_t*)cmd, sizeof(cmd), 500); // キャラクタ間タイムアウト500msで1行読む
        if (ret == -3) {
            //タイムアウト
            return -3;
        }
        if (memcmp(cmd, "OK", 2) == 0) {
            //OK
            return 0;
        } else if (memcmp(cmd, "NG", 2) == 0) {
            //NG
            return 1;
        }
        delay(1);
    }
    return ret;
}

/**
 * $Rコマンド送信
 */
static int sipfSendR(uint8_t addr, uint8_t *read_value)
{
    int len, ret;
    char *endptr;
    SipfClientFlushReadBuff();

    // $Rコマンド送信
    len = sprintf(cmd, "$R %02X\r\n", addr);
    ret = Serial2.write((uint8_t*)cmd, len);

    // $Rコマンド応答待ち
    for (;;) {
        ret = SipfUtilReadLine((uint8_t*)cmd, sizeof(cmd), 10000);
        if (ret == -3) {
            //タイムアウト
            return -3;
        }
        if (cmd[0] == '$') {
            //エコーバック
            continue;
        }
        if (memcmp(cmd, "NG", 2) == 0) {
            //NG
            return 1;
        }
        if (strlen(cmd) == 2) {
            //Valueらしきもの
            *read_value = strtol(cmd, &endptr, 16);
            if (*endptr != '\0') {
                //Null文字以外で変換が終わってる
                return -1;
            }
            break;
        }
    }
    for (;;) {
        ret = SipfUtilReadLine((uint8_t*)cmd, sizeof(cmd), 500); // キャラクタ間タイムアウト500msで1行読む
        if (ret == -3) {
            //タイムアウト
            return -3;
        }
        if (memcmp(cmd, "OK", 2) == 0) {
            //OK
            break;
        }
    }
    return 0;   
}

/**
 * 認証モード設定
 * mode: 0x00 パスワード認証, 0x01: IPアドレス(SIM)認証
 */
int SipfSetAuthMode(uint8_t mode)
{
    int ret;
    uint8_t val;
    if (sipfSendW(0x00, mode) != 0) {
        //$W 送信失敗
        return -1;
    }
    for (;;) {
        delay(200);
        ret = sipfSendR(0x00, &val);
        if (ret != 0) {
            return ret;
        }
        if (val == mode) {
            break;
        }
    }
    return 0;
}

/**
 * 認証情報を設定
 */
int SipfSetAuthInfo(char *user_name, char *password)
{
    int len;
    //ユーザー名の長さを設定
    len = strlen(user_name);
    if (len > 80) {
        return -1;
    }
    if (sipfSendW(0x10, (uint8_t)len) != 0) {
        return -1;  //$W送信失敗
    }
    delay(200);
    //ユーザー名を設定
    for (int i = 0; i < len; i++) {
        if (sipfSendW(0x20 + i, (uint8_t)user_name[i]) != 0) {
            return -1;  //$W送信失敗
        }
        delay(200);
    }

    //パスワードの長さを設定
    len = strlen(password);
    if (len > 80) {
        return -1;
    }
    if (sipfSendW(0x80, (uint8_t)len) != 0) {
        return -1;  //$W送信失敗
    }
    delay(200);
    //パスワードを設定
    for (int i = 0; i < len; i++) {
        if (sipfSendW(0x90 + i, (uint8_t)password[i]) != 0) {
            return -1;  //$W送信失敗
        }
        delay(200);
    }

    return 0;
}


int SipfSetGnss(bool is_active) {
    int len;
    int ret;

    //UART受信バッファを読み捨てる
    SipfClientFlushReadBuff();

    // $$GNSSENコマンド送信
    len = sprintf(cmd, "$$GNSSEN %d\r\n", is_active?1:0);
    ret = Serial2.write((uint8_t*)cmd, len);

    // 応答待ち
    for (;;) {
        ret = SipfUtilReadLine((uint8_t*)cmd, sizeof(cmd), 1000); // 0.5秒間なにも応答なかったら諦める
        if (ret == -3) {
            // タイムアウト
            return -3;
        }
        if (cmd[0] == '$') {
            //エコーバック
            continue;
        }
        if (memcmp(cmd, "NG", 2) == 0) {
            // NG
            return -1;
        }
        if (memcmp(cmd, "OK", 2) == 0) {
            // OK
            break;
        }
    }

    return 0;
}


int SipfGetGnssLocation(GnssLocation *loc) {
    int len;
    int ret;

    if (loc == NULL) {
      return -1;
    }

    //UART受信バッファを読み捨てる
    SipfClientFlushReadBuff();

    // $$GNSSLOCコマンド送信
    len = sprintf(cmd, "$$GNSSLOC\r\n");
    ret = Serial2.write((uint8_t*)cmd, len);

    // 位置情報待ち
    for (;;) {
        ret = SipfUtilReadLine((uint8_t*)cmd, sizeof(cmd), 10000); // 10秒間なにも応答がなかったら諦める
        if (ret == -3) {
            //タイムアウト
            return -3;
        }

        if (cmd[0] == '$') {
            // エコーバック
            continue;
        }
        if (memcmp(cmd, "NG", 2) == 0) {
            // NG
            return -1;
        }
        if (cmd[0] == 'A' || cmd[0] == 'V') {
            // 位置情報
            int counter = 0;
            char *head = cmd;
            char *next = cmd;
            bool is_last = false;
            for(;;) {
              while(*next != '\0' && *next != ','){
                next++;
              }
              is_last = (*next == '\0');
              *next = '\0';
              next++;
              String str = String(head);

              switch(counter){
                case 0: // FIXED
                  if (head[0] == 'A')
                    loc->fixed = true;
                  else if (head[0] == 'V')
                    loc->fixed = false;
                  else
                    return -2;
                  break;
                case 1: // Longitude
                  loc->longitude = str.toFloat();
                  break;
                case 2: // Latitude
                  loc->latitude = str.toFloat();
                  break;
                case 3: // Altitude
                  loc->altitude = str.toFloat();
                  break;
                case 4: // Speed
                  loc->speed = str.toFloat();
                  break;
                case 5: // Heading
                  loc->heading = str.toFloat();
                  break;
                case 6: // Datetime
                  if (strlen(head) != 20) {
                    return -2;
                  }
                  if (head[4] != '-' || head[7] != '-' || head[10] != 'T' || head[13] != ':' ||  head[16] != ':' ||  head[19] != 'Z'){
                    return -2;
                  }
                  head[4] = '\0';
                  head[7] = '\0';
                  head[10] = '\0';
                  head[13] = '\0';
                  head[16] = '\0';
                  head[19] = '\0';
                  loc->year = atoi(&head[0]);
                  loc->month = atoi(&head[5]);
                  loc->day = atoi(&head[8]);
                  loc->hour = atoi(&head[11]);
                  loc->minute = atoi(&head[14]);
                  loc->second = atoi(&head[17]);
                  break;
              }

              if (is_last){
                if (counter != 6) {
                  return -2;
                }
                break;
              }
              head = next;
              counter++;
            }
            break;
        }
    }

    // OK応答待ち
    for (;;) {
        ret = SipfUtilReadLine((uint8_t*)cmd, sizeof(cmd), 1000); // 0.5秒間なにも応答なかったら諦める
        if (ret == -3) {
            // タイムアウト
            return -3;
        }
        if (memcmp(cmd, "NG", 2) == 0) {
            // NG
            return -1;
        }
        if (memcmp(cmd, "OK", 2) == 0) {
            // OK
            break;
        }
    }

    return 0;
}


int SipfCmdTx(uint8_t tag_id, SimpObjTypeId type, uint8_t *value, uint8_t value_len, uint8_t *otid)
{
    int len;
    int ret;
    
    //UART受信バッファを読み捨てる
    SipfClientFlushReadBuff();

    // $$TXコマンド送信
    len = sprintf(cmd, "$$TX %02X %02X ", tag_id, (uint8_t)type);
    switch (type) {
        case OBJ_TYPE_BIN_BASE64:
        case OBJ_TYPE_STR_UTF8:
			//順番どおりに文字列に変換
            for (int i = 0; i < value_len; i++) {
                len += sprintf(&cmd[len], "%02X", value[i]);
            }
            break;
        default:
            // リトルエンディアンだからアドレス上位から順に文字列に変換
            for (int i = (value_len - 1); i >= 0; i--) {
                len += sprintf(&cmd[len], "%02X", value[i]);
            }
            break;
    }

    len += sprintf(&cmd[len], "\r\n");
    ret = Serial2.write((uint8_t*)cmd, len);

    // OTID待ち
    for (;;) {
        ret = SipfUtilReadLine((uint8_t*)cmd, sizeof(cmd), 10000); // 10秒間なにも応答がなかったら諦める
        if (ret == -3) {
            //タイムアウト
            return -3;
        }
        if (cmd[0] == '$') {
            //エコーバック
            continue;
        }
        if (ret == 33) {
            //OTIDらしきもの
            memcpy(otid, cmd, 32);
            break;
        }
        if (memcmp(cmd, "NG", 2) == 0) {
            //NG
            return -1;
        }
    }
    for (;;) {
        ret = SipfUtilReadLine((uint8_t*)cmd, sizeof(cmd), 500); // 0.5秒間なにも応答なかったら諦める
        if (ret == -3) {
            //タイムアウト
            return -3;
        }
        if (memcmp(cmd, "OK", 2) == 0) {
            //OK
            break;
        }
    }
    return 0;
}
