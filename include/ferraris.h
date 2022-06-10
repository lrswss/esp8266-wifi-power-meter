/***************************************************************************
  Copyright (c) 2019-2022 Lars Wessels

  This file a part of the "ESP8266 Wifi Power Meter" source code.
  https://github.com/lrswss/esp8266-wifi-power-meter

  Licensed under the MIT License. You may not use this file except in
  compliance with the License. You may obtain a copy of the License at

  https://opensource.org/licenses/MIT

***************************************************************************/

#ifndef _FERRARIS_H
#define _FERRARIS_H

#include <Arduino.h>

typedef struct {
    uint16_t size;
    uint16_t index;
    uint16_t spread;
    uint16_t max;
    uint16_t min; 
} ferrarisReadings_t;

extern ferrarisReadings_t ferraris;
extern bool thresholdCalculation;

void initFerraris();
bool readFerraris();
void calibrateFerraris();

#endif
