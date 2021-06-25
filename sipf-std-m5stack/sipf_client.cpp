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
static void flushReadBuff(void)
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
    flushReadBuff();

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
    flushReadBuff();

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

int SipfCmdTx(uint8_t tag_id, SimpObjTypeId type, uint8_t *value, uint8_t value_len, uint8_t *otid)
{
    int len;
    int ret;
    
    //UART受信バッファを読み捨てる
    flushReadBuff();

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
