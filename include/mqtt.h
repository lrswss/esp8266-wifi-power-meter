/***************************************************************************
  Copyright (c) 2019-2022 Lars Wessels

  This file a part of the "ESP8266 Wifi Power Meter" source code.
  https://github.com/lrswss/esp8266-wifi-power-meter

  Licensed under the MIT License. You may not use this file except in
  compliance with the License. You may obtain a copy of the License at

  https://opensource.org/licenses/MIT

***************************************************************************/

#ifndef _MQTT_H
#define _MQTT_H

#include <Arduino.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>

#define MQTT_CLIENT_ID      "PowerMeter_%x"
#define MQTT_SUBTOPIC_CNT   "counter"
#define MQTT_SUBTOPIC_CONS  "consumption"
#define MQTT_SUBTOPIC_RUNT  "runtime"
#define MQTT_SUBTOPIC_RSSI  "rssi"
#define MQTT_SUBTOPIC_HEAP  "freeheap"

extern PubSubClient mqtt;

void mqttPublish();

#endif
