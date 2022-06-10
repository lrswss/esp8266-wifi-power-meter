
/***************************************************************************
  Copyright (c) 2019-2022 Lars Wessels

  This file a part of the "ESP8266 Wifi Power Meter" source code.
  https://github.com/lrswss/esp8266-wifi-power-meter

  Licensed under the MIT License. You may not use this file except in
  compliance with the License. You may obtain a copy of the License at

  https://opensource.org/licenses/MIT

***************************************************************************/

#ifndef _CONFIG_H
#define _CONFIG_H

// uncomment for german ui, defaults to english
#define LANGUAGE_EN

//
// the following options can also be set in web ui
//

// this value has to be set according
// to the specs of your ferraris meter
#define TURNS_PER_KWH 75

// publish power meter readings via MQTT (optional)
//#define MQTT_ENABLE
#define MQTT_PUBLISH_JSON
#define MQTT_BROKER_HOSTNAME "__mqtt_broker__"
#define MQTT_BASE_TOPIC "__mqtt_topic__"
#define MQTT_PUBLISH_INTERVAL_SEC 60
// uncomment to enable MQTT authentication
//#define MQTT_USERNAME "admin"
//#define MQTT_PASSWORD "secret"

// the following settings should be changed with care
// better use web ui (expert settings) for fine-tuning 
#define READINGS_BUFFER_SEC 90
#define READINGS_INTERVAL_MS 50
#define READINGS_SPREAD_MIN 4
#define ABOVE_THRESHOLD_TRIGGER 3
#define PULSE_DEBOUNCE_MS 2000
#define BACKUP_CYCLE_MIN 60

// For debugging purposes only
// Raw analog readings (READINGS_INTERVAL_MS) from the IR sensor are 
// send to an InfluxDB which has to configured to accept data on a 
// dedicated UDP port. Can also be enabled/disabled in web ui but HOST, 
// PORT, TAG need to preset here. Use this if threshold auto-detection
// fails or you want to fine-tune it
//#define INFLUXDB_ENABLE
#define INFLUXDB_HOST "__influxdb_server__"
#define INFLUXDB_UDP_PORT 8089
#define INFLUXDB_DEVICE_TAG "__wifipowermeter__"

// to make Arduino IDE happy
// version number is set in platformio.ini
#ifndef FIRMWARE_VERSION
#define FIRMWARE_VERSION 200
#endif

#endif