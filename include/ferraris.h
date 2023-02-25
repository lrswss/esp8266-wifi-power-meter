/***************************************************************************
  Copyright (c) 2019-2023 Lars Wessels

  This file a part of the "ESP8266 Wifi Power Meter" source code.
  https://github.com/lrswss/esp8266-wifi-power-meter

  Licensed under the MIT License. You may not use this file except in
  compliance with the License. You may obtain a copy of the License at

  https://opensource.org/licenses/MIT

***************************************************************************/

#ifndef _FERRARIS_H
#define _FERRARIS_H

#include <Arduino.h>
#include <movingAvg.h>

// sanity checks for web ui and settings import
#define KWH_TURNS_MIN 75
#define KWH_TURNS_MAX 800
#define METER_ID_LEN_MIN 2
#define POWER_AVG_SECS_MIN 60
#define POWER_AVG_SECS_MAX 300
#define PULSE_HISTORY_SIZE 64
#define PULSE_THRESHOLD_MIN 10
#define PULSE_THRESHOLD_MAX 1023
#define READINGS_SPREAD_MIN 3
#define READINGS_SPREAD_MAX 30
#define READINGS_INTERVAL_MS_MIN 15
#define READINGS_INTERVAL_MS_MAX 50
#define READINGS_BUFFER_SECS_MIN 30
#define READINGS_BUFFER_SECS_MAX 120
#define THRESHOLD_TRIGGER_MIN 3
#define THRESHOLD_TRIGGER_MAX 8
#define DEBOUNCE_TIME_MS_MIN 1000
#define DEBOUNCE_TIME_MS_MAX 3000

typedef struct {
    float consumption;
    int16_t power;
    uint16_t size;
    uint16_t index;
    uint16_t spread;
    uint16_t max;
    uint16_t min;
    uint16_t offsetNoWifi;
} ferrarisReadings_t;

extern ferrarisReadings_t ferraris;
extern bool thresholdCalculation;

void initFerraris();
bool readFerraris();
void calibrateFerraris();
void resetWifiOffset();

#endif
