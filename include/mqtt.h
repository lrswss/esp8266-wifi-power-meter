/***************************************************************************
  Copyright (c) 2019-2023 Lars Wessels

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
#include <WiFiClient.h>
#include <WiFiClientSecure.h>

#define MQTT_CLIENT_ID      "WifiPowerMeter_%x"
#define MQTT_SUBTOPIC_CNT   "counter"
#define MQTT_SUBTOPIC_CONS  "consumption"
#define MQTT_SUBTOPIC_PWR   "power"
#define MQTT_SUBTOPIC_RUNT  "runtime"
#define MQTT_SUBTOPIC_RSSI  "rssi"
#define MQTT_SUBTOPIC_HEAP  "freeheap"
#define MQTT_SUBTOPIC_WIFI  "wificounter"
#define MQTT_SUBTOPIC_TXINT "mqttinterval"
#define MQTT_SUBTOPIC_ONAIR "wifisecs"
#define MQTT_SUBTOPIC_PSAVE "powersave"
#define MQTT_SUBTOPIC_RST   "restart"
#define MQTT_TOPIC_DISCOVER "homeassistant/sensor/wifipowermeter-"

#define MQTT_BROKER_LEN_MIN 4
#define MQTT_BROKER_PORT_MIN 1023
#define MQTT_BROKER_PORT_MAX 65535
#define MQTT_BASETOPIC_LEN_MIN 4
#define MQTT_INTERVAL_MIN 30
#define MQTT_INTERVAL_MAX 1800
#define MQTT_USER_LEN_MIN 4
#define MQTT_PASS_LEN_MIN 4

// minimum mqtt message interval (secs) if power saving mode is enabled
// wifi is switched off at least for given number of seconds
#define MQTT_INTERVAL_MIN_POWERSAVING 180

// power saving mode enables moving average for current power reading
// to return reasonable values
#define POWER_AVG_SECS_POWERSAVING 90

// retry connecting to MQTT broker after given number of seconds
#define MQTT_CONN_RETRY_SECS 15

void mqttPublish();
void mqttDisconnect(bool unsetHAdiscovery);
void mqttLoop();
void mqttUnsetTopic(const char* topic);

#endif
