/*
 * Copyright (c) 2022 SAKURA internet Inc.
 *
 * SPDX-License-Identifier: MIT
 */

#include <Arduino.h>
#include <stdint.h>

extern "C" {

int XmodemGetByte(uint8_t *b)
{
  int len = Serial2.available();
  if (len == 0) {
    return -1;  //EMPTY
  }

  int ret = Serial2.read();
  if (ret < 0) {
    return -1;  //ERROR
  }

  *b = (uint8_t)ret;

  return 0;
}

int XmodemGetByteTimeout(uint8_t *b, uint32_t timeout)
{
  int ret, len;
  uint32_t t_recved, t_now;

  t_recved = millis();
  for (;;) {
    t_now = millis();
    //タイムアウト判定
    if ((int32_t)((t_recved + timeout) - t_now) < 0) {
        //タイムアウト
        return -3;
    }

    len = Serial2.available();
    if (len > 0) {
      ret = Serial2.read();
      if (ret < 0) {
        return -1;  // ERROR
      }
      *b = (uint8_t)ret;
      return 0; // OK
    }
  }
}

int XmodemPutByte(uint8_t b)
{
  int ret;
  ret = Serial2.write(b);
  if (ret < 0) {
    return -1;
  }
  return 0;
}

int XmodemPut(uint8_t *buff, int sz)
{
  int ret;
  ret = Serial2.write(buff, sz);
  if (ret < 0) {
    return -1;
  }
  return 0;
}

void XmodemDelay(uint32_t d)
{
  delay(d);
}
  
}

