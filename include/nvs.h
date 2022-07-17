/***************************************************************************
  Copyright (c) 2019-2022 Lars Wessels

  This file a part of the "ESP8266 Wifi Power Meter" source code.
  https://github.com/lrswss/esp8266-wifi-power-meter

  Licensed under the MIT License. You may not use this file except in
  compliance with the License. You may obtain a copy of the License at

  https://opensource.org/licenses/MIT

***************************************************************************/

#ifndef _NVS_H
#define _NVS_H

#include <Arduino.h>
#include <EEPROM_Rotate.h>

#define EEPROM_ADDR 10

typedef struct {
    uint32_t counterTotal;
    uint32_t counterOffset;
    uint16_t pulseThreshold;
    uint16_t turnsPerKwh;
    uint16_t backupCycleMin;
    bool calculateCurrentPower;
    bool calculatePowerMvgAvg;
    uint16_t powerAvgSecs;
    uint8_t readingsBufferSec;
    uint8_t readingsIntervalMs;
    uint8_t readingsSpreadMin;
    uint8_t aboveThresholdTrigger;
    uint16_t pulseDebounceMs;
    bool enableMQTT;
    char mqttBroker[65];
    uint16_t mqttBrokerPort;
    char mqttBaseTopic[65];
    uint16_t mqttIntervalSecs;
    bool mqttEnableAuth;
    char mqttUsername[33];
    char mqttPassword[33];
    bool mqttJSON;
    bool enableInflux;
    char systemID[17];
    uint8_t magic;
} settings_t;

// use rotating pseudo EEPROM (actually ESP8266 flash)
extern EEPROM_Rotate EEP;
extern settings_t settings;

void initNVS();
void saveNVS();
void resetNVS();

#endif