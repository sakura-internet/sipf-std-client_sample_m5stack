/*
 * Copyright (c) 2021 Sakura Internet Inc.
 *
 * SPDX-License-Identifier: MIT
 */
#include <Arduino.h>
#include <M5Stack.h>
#include <string.h>
#include "sipf_client.h"

/**
 * SIPF接続情報
 */
static uint8_t buff[256];
static uint32_t cnt_btn1, cnt_btn2, cnt_btn3;

static int resetSipfModule()
{
  digitalWrite(5, LOW);
  pinMode(5, OUTPUT);

  // Reset要求
  digitalWrite(5, HIGH);
  delay(10);
  digitalWrite(5, LOW);

  // UART初期化
  Serial2.begin(115200, SERIAL_8N1, 16, 17);

  // 起動完了メッセージ待ち
  int len;
  for (;;) {
    len = SipfUtilReadLine(buff, sizeof(buff), 30000);
    if (len < 0) {
      return -1;  //Serialのエラーかタイムアウト
    }

    if (len >= 13) {
      //ICCIDを出力
      if (memcmp(buff, "ICCID:", 6) == 0) {
        Serial.println((char*)buff);
        continue;
      }
      //起動完了メッセージを確認
      if (memcmp(buff, "+++ Ready +++", 13) == 0) {
        Serial.println((char*)buff);
        break;
      }
    }
  }
  return 0;
}

static void drawTitle(void)
{
  M5.Lcd.setTextSize(2);  
  M5.Lcd.fillRect(0, 0, 320, 20, 0xfaae);
  M5.Lcd.setCursor(2, 2);
  M5.Lcd.setTextColor(TFT_BLACK, 0xfaae);
  M5.Lcd.printf("SIPF Client for M5Stack\n");

  M5.Lcd.setTextSize(1);
  M5.Lcd.setTextColor(TFT_LIGHTGREY, TFT_BLACK);
  M5.Lcd.setCursor(0, 24);
}

static void drawButton(uint8_t button, uint32_t value)
{
  M5.Lcd.fillRect(35 + (95 * button), 200, 60, 40, 0xfaae);
  
  M5.Lcd.setTextSize(2);
  M5.Lcd.setTextColor(TFT_BLACK, 0xfaae);
  M5.Lcd.setCursor(40 + (95 * button), 210);
  M5.Lcd.printf("%4d", value);
}

static void setCursorResultWindow(void)
{
  M5.Lcd.setTextColor(TFT_BLACK, 0xce79);
  M5.Lcd.setCursor(0, 121);
}

static void drawResultWindow(void)
{
  M5.Lcd.setTextSize(1);

  M5.Lcd.fillRect(0, 110, 320, 10, 0xfaae);
  M5.Lcd.setTextColor(TFT_BLACK, 0xfaae);
  M5.Lcd.setCursor(1, 111);
  M5.Lcd.printf("RESULT");

  M5.Lcd.fillRect(0, 120, 320, 78, 0xce79);
  setCursorResultWindow();
}

void setup() {
  // put your setup code here, to run once:
  M5.begin();
  M5.Power.begin();

  Serial.begin(115200);

  Serial.printf("width: %d\n", M5.Lcd.width());

  M5.Lcd.setBrightness(127);

  drawTitle();

  M5.Lcd.printf("Booting...");
  if (resetSipfModule() == 0) {
    M5.Lcd.printf(" OK\n");
  } else {
    M5.Lcd.printf(" NG\n");
    return;
  }

  M5.Lcd.printf("Setting auth mode...");
  if (SipfSetAuthMode(0x01) == 0) {
    M5.Lcd.printf(" OK\n");
  } else {
    M5.Lcd.printf(" NG\n");
    return;
  }

  drawResultWindow();

  cnt_btn1 = 0;
  cnt_btn2 = 0;
  cnt_btn3 = 0;

  drawButton(0, cnt_btn1);
  drawButton(1, cnt_btn2);
  drawButton(2, cnt_btn3);
}

void loop() {
  // put your main code here, to run repeatedly:
  int available_len;
  /* PCとモジュールのシリアルポートを中継 */
  available_len = Serial.available();
  for (int i = 0; i < available_len; i++) {
    unsigned char b = Serial.read();
    Serial2.write(b);
  }

  available_len = Serial2.available();
  for (int i = 0; i < available_len; i++) {
    unsigned char b = Serial2.read();
    Serial.write(b);
  }

  /* ボタン */
  if (M5.BtnA.wasPressed()) {
    cnt_btn1++;
    drawResultWindow();
    M5.Lcd.printf("ButtonA pushed: TX(tag_id=0x00 value=%d)\n", cnt_btn1);
    memset(buff, 0, sizeof(buff));
    int ret = SipfCmdTx(0x01, OBJ_TYPE_UINT32, (uint8_t*)&cnt_btn1, 4, buff);
    if (ret == 0) {
      M5.Lcd.printf("OK\nOTID: %s\n", buff);
      drawButton(0, cnt_btn1);
    } else {
      M5.Lcd.printf("NG: %d\n", ret);
    }
  }

  if (M5.BtnB.wasPressed()) {
    cnt_btn2++;
    drawResultWindow();
    M5.Lcd.printf("ButtonB pushed: TX(tag_id=0x01 value=%d)\n", cnt_btn2);
    memset(buff, 0, sizeof(buff));
    int ret = SipfCmdTx(0x02, OBJ_TYPE_UINT32, (uint8_t*)&cnt_btn2, 4, buff);
    if (ret == 0) {
      M5.Lcd.printf("OK\nOTID: %s\n", buff);
      drawButton(1, cnt_btn2);
    } else {
      M5.Lcd.printf("NG: %d\n", ret);
    }
  }

  if (M5.BtnC.wasPressed()) {
    cnt_btn3++;
    drawResultWindow();
    M5.Lcd.printf("ButtonC pushed: TX(tag_id=0x02 value=%d)\n", cnt_btn3);
    memset(buff, 0, sizeof(buff));
    int ret = SipfCmdTx(0x02, OBJ_TYPE_UINT32, (uint8_t*)&cnt_btn3, 4, buff);
    if (ret == 0) {
      M5.Lcd.printf("OK\nOTID: %s\n", buff);
      drawButton(2, cnt_btn3);
    } else {
      M5.Lcd.printf("NG: %d\n", ret);
    }
  }

  M5.update();
}
