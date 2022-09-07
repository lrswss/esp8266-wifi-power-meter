/***************************************************************************
  Copyright (c) 2019-2022 Lars Wessels

  This file a part of the "ESP8266 Wifi Power Meter" source code.
  https://github.com/lrswss/esp8266-wifi-power-meter

  Licensed under the MIT License. You may not use this file except in
  compliance with the License. You may obtain a copy of the License at

  https://opensource.org/licenses/MIT

***************************************************************************/

#ifndef _INFLUX_H
#define _INFLUX_H

#include <Arduino.h>
#include <WiFiUdp.h>

void send2influx_udp(uint16_t counter, uint16_t threshold, uint16_t pulse);

#endif
