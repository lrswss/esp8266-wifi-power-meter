/***************************************************************************
  Copyright (c) 2019-2022 Lars Wessels

  This file a part of the "ESP8266 Wifi Power Meter" source code.
  https://github.com/lrswss/esp8266-wifi-power-meter

  Licensed under the MIT License. You may not use this file except in
  compliance with the License. You may obtain a copy of the License at

  https://opensource.org/licenses/MIT

***************************************************************************/

#include "config.h"
#include "wlan.h"
#include "mqtt.h"
#include "web.h"
#include "utils.h"
#include "nvs.h"

WiFiClient espClient;
PubSubClient mqtt(espClient);


// connect to MQTT server (with changing id on every attempt)
// give up after three consecutive failures
static bool mqttConnect() {
    static char clientid[32];
    uint8_t mqtt_error = 0;

    mqtt.setServer(settings.mqttBroker, 1883);
    while (!mqtt.connected() && mqtt_error < 3) {
        snprintf(clientid, sizeof(clientid), MQTT_CLIENT_ID, (int)random(0xfffff));
        Serial.printf("Connecting to MQTT Broker %s as %s...", settings.mqttBroker, clientid);
        if (mqtt.connect(clientid)) {
            Serial.println(F("OK."));
            mqtt_error = 0;
            return true;
        } else {
            mqtt_error++;
            Serial.printf("failed (error %d)\n", mqtt.state());
            delay(1000);
        }
    }

    if (mqtt_error >= 3) {
        setMessage("publishFailed", 3);
        mqtt_error = 0;
        return false;
    }
    return true;
}


// publish data on multiple topics
static void publishDataSingle() {
    static char topicStr[128];
    float totalConsumption = 0.0;

    if (mqttConnect()) {
        snprintf(topicStr, sizeof(topicStr), "%s/%s/%s", 
            settings.mqttBaseTopic, systemID().c_str(), MQTT_SUBTOPIC_CNT);
        Serial.printf("MQTT %s %d\n", topicStr, settings.counterTotal);
        mqtt.publish(topicStr, String(settings.counterTotal).c_str(), true);
        delay(50);

        // publish total consumption if counter offset values is set
        if (settings.counterOffset > 0) {
            snprintf(topicStr, sizeof(topicStr), "%s/%s/%s", 
                settings.mqttBaseTopic, systemID().c_str(), MQTT_SUBTOPIC_CONS);
            totalConsumption = int((settings.counterTotal / (settings.turnsPerKwh * 1.0) 
                + settings.counterOffset / 100.0) * 100) / 100.0;        
            Serial.printf("MQTT %s ", topicStr);
            Serial.println(totalConsumption); // float!
            mqtt.publish(topicStr, String(totalConsumption).c_str(), true);
            delay(50);
        }

        snprintf(topicStr, sizeof(topicStr), "%s/%s/%s", 
            settings.mqttBaseTopic, systemID().c_str(), MQTT_SUBTOPIC_RUNT);
        Serial.printf("MQTT %s %s\n", topicStr, getRuntime(true));
        mqtt.publish(topicStr, getRuntime(true), true);
        delay(50);

        snprintf(topicStr, sizeof(topicStr), "%s/%s/%s", 
            settings.mqttBroker, systemID().c_str(), MQTT_SUBTOPIC_RSSI);
        Serial.printf("MQTT %s %d\n", topicStr, WiFi.RSSI());
        mqtt.publish(topicStr, String(WiFi.RSSI()).c_str(), true);
        delay(50);

#ifdef DEBUG_HEAP
        snprintf(topicStr, sizeof(topicStr), "%s/%s/%s",
            settings.mqttBroker, systemID().c_str(), MQTT_SUBTOPIC_HEAP);
        Serial.printf("%s %d\n", topicStr, ESP.getFreeHeap());
        mqtt.publish(topicStr, String(ESP.getFreeHeap()).c_str(), true);
#endif
    }
}


// publish data on base topic as JSON
static void publishDataJSON() {
    StaticJsonDocument<192> JSON;
    static char topicStr[128];
    char buf[128];

    JSON.clear();
    if (mqttConnect()) {
        JSON[MQTT_SUBTOPIC_CNT] = settings.counterTotal;
        if (settings.counterOffset > 0)
            JSON[MQTT_SUBTOPIC_CONS] = int((settings.counterTotal / (settings.turnsPerKwh * 1.0) 
                + settings.counterOffset / 100.0) * 100) / 100.0;
        JSON[MQTT_SUBTOPIC_RUNT] = getRuntime(true);
        JSON[MQTT_SUBTOPIC_RSSI] = WiFi.RSSI();
#ifdef DEBUG_HEAP
        JSON[MQTT_SUBTOPIC_HEAP] = ESP.getFreeHeap();
#endif
        size_t s = serializeJson(JSON, buf);
        snprintf(topicStr, sizeof(topicStr), "%s/%s", settings.mqttBaseTopic, systemID().c_str());
        Serial.printf("MQTT %s %s\n", topicStr, buf);
        mqtt.publish(topicStr, buf, s);
    }
}


// publish meter reading updates on single 
// topic as JSON or on multiple topics 
void mqttPublish() {
    setMessage("publishData", 3);
    if (settings.mqttJSON)
        publishDataJSON();
    else
        publishDataSingle();
}
