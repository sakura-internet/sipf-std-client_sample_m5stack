/*
 * Copyright (c) 2021 Sakura Internet Inc.
 *
 * SPDX-License-Identifier: MIT
 */
#include <Arduino.h>
#include <M5Stack.h>
#include <string.h>
#include "sipf_client.h"

/*
#define ENABLE_GNSS
*/

#ifndef ENABLE_GNSS
#define WIN_RESULT_LEFT   (0)
#define WIN_RESULT_TOP    (50)
#define WIN_RESULT_WIDTH  (320)
#define WIN_RESULT_HEIGHT (148)
#define WIN_RESULT_TITLE_HEIGHT (10)
#else
#define WIN_RESULT_LEFT   (0)
#define WIN_RESULT_TOP    (110)
#define WIN_RESULT_WIDTH  (320)
#define WIN_RESULT_HEIGHT (88)
#define WIN_RESULT_TITLE_HEIGHT (10)
#endif
/**
 * SIPF接続情報
 */
static uint8_t buff[256];
static uint32_t cnt_btn1, cnt_btn2;

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
  Serial.println("### MODULE OUTPUT ###");
  int len, is_echo = 0;
  for (;;) {
    len = SipfUtilReadLine(buff, sizeof(buff), 300000); //タイムアウト300秒
    if (len < 0) {
      return -1;  //Serialのエラーかタイムアウト
    }
    if (len == 1) {
      //空行なら次の行へ
      continue;
    }
    if (len >= 13) {
      if (memcmp(buff, "*** SIPF Client", 15) == 0) {
        is_echo = 1;
      }
      //起動完了メッセージを確認
      if (memcmp(buff, "+++ Ready +++", 13) == 0) {
        Serial.println("#####################");
        break;
      }
      //接続リトライオーバー
      if (memcmp(buff, "ERR:Faild", 9) == 0) {
        Serial.println((char*)buff);
        Serial.println("#####################");
        return -1;
      }
    }
    if (is_echo) {
      Serial.printf("%s\r\n", (char*)buff);
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

  if (button != 2) {
    M5.Lcd.printf("%4d", value);
  } else {
    //受信ボタン
    M5.Lcd.printf(" RX ");
  }
}

static void setCursorResultWindow(void)
{
  M5.Lcd.setTextColor(TFT_BLACK, 0xce79);
  M5.Lcd.setCursor(0, WIN_RESULT_TOP + WIN_RESULT_TITLE_HEIGHT + 1);
}

static void drawResultWindow(void)
{
  M5.Lcd.setTextSize(1);

  M5.Lcd.fillRect(WIN_RESULT_LEFT, WIN_RESULT_TOP, WIN_RESULT_WIDTH, WIN_RESULT_TITLE_HEIGHT, 0xfaae);
  M5.Lcd.setTextColor(TFT_BLACK, 0xfaae);
  M5.Lcd.setCursor(1, WIN_RESULT_TOP + 1);
  M5.Lcd.printf("RESULT");

  M5.Lcd.fillRect(WIN_RESULT_LEFT, WIN_RESULT_TOP + WIN_RESULT_TITLE_HEIGHT, WIN_RESULT_WIDTH, WIN_RESULT_HEIGHT - WIN_RESULT_TITLE_HEIGHT, 0xce79);
  setCursorResultWindow();
}

#ifdef ENABLE_GNSS
static void printGnssLocation(GnssLocation *gnss_location_p) {
  if (!gnss_location_p->fixed) {
    M5.Lcd.printf("Not fixed\n");
  }else{
   M5.Lcd.printf("Fixed\n");
  }

   M5.Lcd.printf("%.6f %.6f\n", gnss_location_p->latitude, gnss_location_p->longitude);

   M5.Lcd.printf("%04d-%02d-%02d %02d:%02d:%02d (UTC)\n",
    gnss_location_p->year, gnss_location_p->month, gnss_location_p->day,
    gnss_location_p->hour, gnss_location_p->minute, gnss_location_p->second
   );
}

static void drawGnssLocation(GnssLocation *gnss_location_p) {

  M5.Lcd.setTextSize(1);

  M5.Lcd.fillRect(0, 70, 320, 10, 0xfaae);
  M5.Lcd.setTextColor(TFT_BLACK, 0xfaae);
  M5.Lcd.setCursor(1, 71);
  M5.Lcd.printf("GNSS");
  M5.Lcd.fillRect(0, 80, 320, 30, 0xce79);

  M5.Lcd.setTextColor(TFT_BLACK, 0xce79);
  M5.Lcd.setCursor(0, 81);

  if(gnss_location_p == NULL) {
    M5.Lcd.println("GNSS error");
  }else{
    printGnssLocation(gnss_location_p);
  }

  setCursorResultWindow();
}
#endif

void setup() {
  // put your setup code here, to run once:
  M5.begin();
  M5.Power.begin();

  Serial.begin(115200);

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
#ifdef ENABLE_GNSS
  M5.Lcd.printf("Enable GNSS..");
  if (SipfSetGnss(true) == 0) {
    M5.Lcd.printf(" OK\n");
  } else {
    M5.Lcd.printf(" NG\n");
    return;
  }
#endif
  drawResultWindow();

  cnt_btn1 = 0;
  cnt_btn2 = 0;

  drawButton(0, cnt_btn1);
  drawButton(1, cnt_btn2);
  drawButton(2, 0);

  Serial.println("+++ Ready +++");
  SipfClientFlushReadBuff();
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
#ifdef ENABLE_GNSS
  /* GNSS */
  static unsigned long last_gnss_updated = 0;
  if(last_gnss_updated + 1000 < millis()){
    last_gnss_updated = millis();
    GnssLocation gnss_location;
    int ret = SipfGetGnssLocation(&gnss_location);
    if (ret == 0) {
      drawGnssLocation(&gnss_location);
    } else {
      drawGnssLocation(NULL);
    }
  }
#endif

  /* `TX1'ボタンを押した */
  if (M5.BtnA.wasPressed()) {
    cnt_btn1++;
    drawResultWindow();
    M5.Lcd.printf("ButtonA pushed: TX(tag_id=0x01 value=%d)\n", cnt_btn1);
    memset(buff, 0, sizeof(buff));
    int ret = SipfCmdTx(0x01, OBJ_TYPE_UINT32, (uint8_t*)&cnt_btn1, 4, buff);
    if (ret == 0) {
      M5.Lcd.printf("OK\nOTID: %s\n", buff);
      drawButton(0, cnt_btn1);
    } else {
      M5.Lcd.printf("NG: %d\n", ret);
    }
  }

  /* `TX2'ボタンを押した */
  if (M5.BtnB.wasPressed()) {
    cnt_btn2++;
    drawResultWindow();
    M5.Lcd.printf("ButtonB pushed: TX(tag_id=0x02 value=%d)\n", cnt_btn2);
    memset(buff, 0, sizeof(buff));
    int ret = SipfCmdTx(0x02, OBJ_TYPE_UINT32, (uint8_t*)&cnt_btn2, 4, buff);
    if (ret == 0) {
      M5.Lcd.printf("OK\nOTID: %s\n", buff);
      drawButton(1, cnt_btn2);
    } else {
      M5.Lcd.printf("NG: %d\n", ret);
    }
  }

  /* `RX'ボタンを押した */
  if (M5.BtnC.wasPressed()) {
    drawResultWindow();
    M5.Lcd.printf("ButtonC pushed: RX request.\n");
    memset(buff, 0, sizeof(buff));

		static SipfObjObject objs[16];
		uint64_t stm, rtm;
		uint8_t remain, qty;
    int ret = SipfCmdRx(buff, &stm, &rtm, &remain, &qty, objs, 16);
    if (ret > 0) {
      time_t t;
      struct tm *ptm;
      static char ts[128];
      M5.Lcd.printf("OTID: %s\r\n", buff);
      //User send datetime.
      t = (time_t)(stm / 1000);
      ptm = localtime(&t);
      strftime(ts, sizeof(ts),"User send datetime(UTC)    : %Y/%m/%d %H:%M:%S\r\n", ptm);
      M5.Lcd.printf(ts);
      //SIPF receive datetime.
      t = (time_t)(rtm / 1000);
      ptm = localtime(&t);
      strftime(ts, sizeof(ts),"SIPF received datetime(UTC): %Y/%m/%d %H:%M:%S\r\n", ptm);
      M5.Lcd.printf(ts);
      //remain, qty
      M5.Lcd.printf("remain=%d, qty=%d\r\n", remain, qty);
      //obj
      for (int i = 0; i < ret; i++) {
        M5.Lcd.printf("obj[%d]:type=0x%02x, tag=0x%02x, len=%d, value=", i, objs[i].type, objs[i].tag_id, objs[i].value_len);
        uint8_t *p_value = objs[i].value;
        SipfObjPrimitiveType v;
        switch (objs[i].type) {
        case OBJ_TYPE_UINT8:
          memcpy(v.b, p_value, sizeof(uint8_t));
          M5.Lcd.printf("%u\r\n", v.u8);
          break;
        case OBJ_TYPE_INT8:
          memcpy(v.b, p_value, sizeof(int8_t));
          M5.Lcd.printf("%d\r\n", v.i8);
          break;
        case OBJ_TYPE_UINT16:
          memcpy(v.b, p_value, sizeof(uint16_t));
          M5.Lcd.printf("%u\r\n", v.u16);
          break;
        case OBJ_TYPE_INT16:
          memcpy(v.b, p_value, sizeof(int16_t));
          M5.Lcd.printf("%d\r\n", v.i16);
          break;
        case OBJ_TYPE_UINT32:
          memcpy(v.b, p_value, sizeof(uint32_t));
          M5.Lcd.printf("%u\r\n", v.u32);
          break;
        case OBJ_TYPE_INT32:
          memcpy(v.b, p_value, sizeof(int32_t));
          M5.Lcd.printf("%d\r\n", v.i32);
          break;
        case OBJ_TYPE_UINT64:
          memcpy(v.b, p_value, sizeof(uint64_t));
          M5.Lcd.printf("%llu\r\n", v.u64);
          break;
        case OBJ_TYPE_INT64:
          memcpy(v.b, p_value, sizeof(int64_t));
          M5.Lcd.printf("%lld\r\n", v.i64);
          break;
        case OBJ_TYPE_FLOAT32:
          memcpy(v.b, p_value, sizeof(float));
          M5.Lcd.printf("%f\r\n", v.f);
          break;
        case OBJ_TYPE_FLOAT64:
          memcpy(v.b, p_value, sizeof(double));
          M5.Lcd.printf("%lf\r\n", v.d);
          break;
        case OBJ_TYPE_BIN:
          M5.Lcd.printf("0x");
          for (int j = 0; j < objs[i].value_len; j++) {
            M5.Lcd.printf("%02x", objs[i].value[j]);
          }
          M5.Lcd.printf("\r\n");
          break;
        case OBJ_TYPE_STR_UTF8:
          for (int j = 0; j < objs[i].value_len; j++) {
            M5.Lcd.printf("%c", objs[i].value[j]);
          }
          M5.Lcd.printf("\r\n");
          break;
        default:
          break;
        } 
      }
      M5.Lcd.printf("OK\n");
    } else if (ret == 0) {
      M5.Lcd.printf("RX buffer is empty.\nOK\n");
    } else {
      M5.Lcd.printf("NG: %d\n", ret);
    }
  }

  M5.update();
}
