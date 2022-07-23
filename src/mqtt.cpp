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
#include "ferraris.h"

static WiFiClient espClient;
static WiFiClientSecure espClientSecure;
static PubSubClient *mqtt = NULL;


// published or deletes Home Assistant auto discovery message
// https://www.home-assistant.io/docs/mqtt/discovery/
static void publishHADiscoveryMessage(bool publish) {
    StaticJsonDocument<512> JSON;
    JsonObject dev;
    char buf[384], devTopic[96];
    char topicTotalCon[80], topicPower[80], topicRSSI[80], topicRuntime[80], topicCount[80];

    snprintf(devTopic, sizeof(devTopic), "%s/%s", settings.mqttBaseTopic, settings.systemID);
    snprintf(topicCount, sizeof(topicCount),
        "%s%s/pulse_count/config", MQTT_TOPIC_DISCOVER, settings.systemID);
    snprintf(topicTotalCon, sizeof(topicTotalCon),
        "%s%s/total_consumption/config", MQTT_TOPIC_DISCOVER, settings.systemID);
    snprintf(topicPower, sizeof(topicPower),
        "%s%s/current_power/config", MQTT_TOPIC_DISCOVER, settings.systemID);
    snprintf(topicRSSI, sizeof(topicRSSI),
        "%s%s/signal_strength/config", MQTT_TOPIC_DISCOVER, settings.systemID);
    snprintf(topicRuntime, sizeof(topicRuntime),
        "%s%s/runtime/config", MQTT_TOPIC_DISCOVER, settings.systemID);

    if (publish) {
        Serial.println(F("Sending home asssistant MQTT auto-discovery message"));

        JSON.clear();
        JSON["name"] = "Ferraris Impuls Counter";
        JSON["unique_id"] = "wifipowermeter-" + String(settings.systemID)+ "-impuls-counter";
        JSON["ic"] = "mdi:counter";
        JSON["stat_t"] = devTopic;
        JSON["val_tpl"] = "{{ value_json."+ String(MQTT_SUBTOPIC_CNT) +" }}";
        dev = JSON.createNestedObject("dev");
        dev["name"] = "WiFi Power Meter " + String(settings.systemID);
        dev["ids"] = settings.systemID;
        dev["cu"] = "http://" + WiFi.localIP().toString();
        dev["mdl"] = "ESP8266";
        dev["mf"] = "https://github.com/lrswss";
        dev["sw"] = FIRMWARE_VERSION;
        serializeJson(JSON, buf);
        dev.clear();
        mqtt->publish(topicCount, buf, true);
        delay(50);

        JSON.clear();
        JSON["name"] = "Total Consumption";
        JSON["unique_id"] = "wifipowermeter-" + String(settings.systemID)+ "-total-consumption";
        JSON["unit_of_meas"] = "kWh";
        JSON["dev_cla"] = "energy";
        JSON["stat_t"] = devTopic;
        JSON["val_tpl"] = "{{ value_json."+ String(MQTT_SUBTOPIC_CONS) +" }}";
        dev = JSON.createNestedObject("dev");
        dev["name"] = "WiFi Power Meter " + String(settings.systemID);
        dev["ids"] = settings.systemID;
        dev["cu"] = "http://" + WiFi.localIP().toString();
        dev["mdl"] = "ESP8266";
        dev["mf"] = "https://github.com/lrswss";
        dev["sw"] = FIRMWARE_VERSION;
        serializeJson(JSON, buf);
        dev.clear();
        mqtt->publish(topicTotalCon, buf, true);
        delay(50);

        JSON.clear();
        JSON["name"] = "Current Power Consumption";
        JSON["unique_id"] = "wifipowermeter-" + String(settings.systemID)+ "-current-power";
        JSON["unit_of_meas"] = "W";
        JSON["dev_cla"] = "power";
        JSON["stat_t"] = devTopic;
        JSON["val_tpl"] = "{{ value_json."+ String(MQTT_SUBTOPIC_PWR) +" }}";
        dev = JSON.createNestedObject("dev");
        dev["name"] = "WiFi Power Meter " + String(settings.systemID);
        dev["ids"] = settings.systemID;
        dev["cu"] = "http://" + WiFi.localIP().toString();
        dev["mdl"] = "ESP8266";
        dev["mf"] = "https://github.com/lrswss";
        dev["sw"] = FIRMWARE_VERSION;
        serializeJson(JSON, buf);
        dev.clear();
        mqtt->publish(topicPower, buf, true);
        delay(50);

        JSON.clear();
        JSON["name"] = "WiFi Signal Strength";
        JSON["unique_id"] = "wifipowermeter-" + String(settings.systemID)+ "-rssi";
        JSON["unit_of_meas"] = "dbm";
        JSON["dev_cla"] = "signal_strength";
        JSON["stat_t"] = devTopic;
        JSON["val_tpl"] = "{{ value_json."+ String(MQTT_SUBTOPIC_RSSI) +" }}";
        dev = JSON.createNestedObject("dev");
        dev["name"] = "WiFi Power Meter " + String(settings.systemID);
        dev["ids"] = settings.systemID;
        dev["cu"] = "http://" + WiFi.localIP().toString();
        dev["mdl"] = "ESP8266";
        dev["mf"] = "https://github.com/lrswss";
        dev["sw"] = FIRMWARE_VERSION;
        serializeJson(JSON, buf);
        dev.clear();
        mqtt->publish(topicRSSI, buf, true);
        delay(50);

        JSON.clear();
        JSON["name"] = "Uptime";
        JSON["unique_id"] = "wifipowermeter-" + String(settings.systemID)+ "-uptime";
        JSON["ic"] = "mdi:clock-outline";
        JSON["dev_cla"] = "duration";
        JSON["stat_t"] = devTopic;
        JSON["val_tpl"] = "{{ value_json."+ String(MQTT_SUBTOPIC_RUNT) +" }}";
        dev = JSON.createNestedObject("dev");
        dev["name"] = "WiFi Power Meter " + String(settings.systemID);
        dev["ids"] = settings.systemID;
        dev["cu"] = "http://" + WiFi.localIP().toString();
        dev["mdl"] = "ESP8266";
        dev["mf"] = "https://github.com/lrswss";
        dev["sw"] = FIRMWARE_VERSION;
        serializeJson(JSON, buf);
        dev.clear();
        mqtt->publish(topicRuntime, buf, true);
        JSON.clear();

    } else {
        // send empty (retained) message to delete sensor autoconfiguration
        Serial.println(F("Removing home asssistant MQTT auto-discovery message"));

        mqtt->publish(topicCount, "", true);
        delay(50);
        mqtt->publish(topicTotalCon, "", true);
        delay(50);
        mqtt->publish(topicPower, "", true);
        delay(50);
        mqtt->publish(topicRSSI, "", true);
        delay(50);
        mqtt->publish(topicRuntime, "", true);
    }
}


static void mqttInit() {
    if (mqtt != NULL)
        return;

    if (settings.mqttSecure) {
        espClientSecure.setInsecure();
        // must reduce memory usage with Maximum Fragment Length Negotiation (supported by mosquitto)
        espClientSecure.probeMaxFragmentLength(settings.mqttBroker, settings.mqttBrokerPort, 512);
        espClientSecure.setBufferSizes(512, 512);
        mqtt = new PubSubClient(espClientSecure);
    } else {
        mqtt = new PubSubClient(espClient);
    }
    mqtt->setServer(settings.mqttBroker, settings.mqttBrokerPort);
    mqtt->setBufferSize(448); // for home assistant mqtt auto-discovery
    mqtt->setSocketTimeout(2); // keep web ui responsive
    mqtt->setKeepAlive(settings.mqttIntervalSecs + 10);
}


// connect to MQTT server (with changing id on every attempt)
// give up after three consecutive failures
static bool mqttConnect() {
    static char clientid[32];
    uint8_t mqtt_error = 0;

    mqttInit();
    if (mqtt->connected())
        return true;

    while (!mqtt->connected() && mqtt_error < 3) {
        snprintf(clientid, sizeof(clientid), MQTT_CLIENT_ID, (int)random(0xfffff));
        Serial.printf("Connecting to MQTT Broker %s as %s", settings.mqttBroker, clientid);
        if (settings.mqttEnableAuth)
            Serial.printf(" with username %s", settings.mqttUsername);
        Serial.printf(" on port %d%s...", settings.mqttBrokerPort, settings.mqttSecure ? " (TLS)" : "");
        if (settings.mqttEnableAuth && mqtt->connect(clientid, settings.mqttUsername, settings.mqttPassword)) {
            Serial.println(F("OK"));
            publishHADiscoveryMessage(settings.enableHADiscovery);
            mqtt_error = 0;
            return true;
        } else if (!settings.mqttEnableAuth && mqtt->connect(clientid)) {
            Serial.println(F("OK"));
            publishHADiscoveryMessage(settings.enableHADiscovery);
            mqtt_error = 0;
            return true;
        } else {
            mqtt_error++;
            Serial.printf("failed (error %d)\n", mqtt->state());
            delay(250);
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

    if (mqttConnect()) {
        setMessage("publishData", 3);
        snprintf(topicStr, sizeof(topicStr), "%s/%s/%s", 
            settings.mqttBaseTopic, systemID().c_str(), MQTT_SUBTOPIC_CNT);
        Serial.printf("MQTT %s %d\n", topicStr, settings.counterTotal);
        mqtt->publish(topicStr, String(settings.counterTotal).c_str(), true);
        delay(50);

        // need counter offset to publish total consumption (kwh)
        if (ferraris.consumption > 0) {
            snprintf(topicStr, sizeof(topicStr), "%s/%s/%s", 
                settings.mqttBaseTopic, systemID().c_str(), MQTT_SUBTOPIC_CONS);
            Serial.printf("MQTT %s ", topicStr);
            Serial.println(ferraris.consumption, 2); // float!
            mqtt->publish(topicStr, String(ferraris.consumption, 2).c_str(), true);
            delay(50);
        }

        if (ferraris.power > -1) {
            snprintf(topicStr, sizeof(topicStr), "%s/%s/%s",
            settings.mqttBaseTopic, systemID().c_str(), MQTT_SUBTOPIC_PWR);
            Serial.printf("MQTT %s %d\n", topicStr, ferraris.power);
            mqtt->publish(topicStr, String(ferraris.power).c_str(), true);
            delay(50);
        }

        snprintf(topicStr, sizeof(topicStr), "%s/%s/%s",
            settings.mqttBaseTopic, systemID().c_str(), MQTT_SUBTOPIC_RUNT);
        Serial.printf("MQTT %s %s\n", topicStr, getRuntime(true));
        mqtt->publish(topicStr, getRuntime(true), true);
        delay(50);

        snprintf(topicStr, sizeof(topicStr), "%s/%s/%s", 
            settings.mqttBaseTopic, systemID().c_str(), MQTT_SUBTOPIC_RSSI);
        Serial.printf("MQTT %s %d\n", topicStr, WiFi.RSSI());
        mqtt->publish(topicStr, String(WiFi.RSSI()).c_str(), true);
        delay(50);

        snprintf(topicStr, sizeof(topicStr), "%s/%s/version",
            settings.mqttBaseTopic, systemID().c_str());
        Serial.printf("MQTT %s %d\n", topicStr, FIRMWARE_VERSION);
        mqtt->publish(topicStr, String(FIRMWARE_VERSION).c_str(), true);
        delay(50);

#ifdef DEBUG_HEAP
        snprintf(topicStr, sizeof(topicStr), "%s/%s/%s",
            settings.mqttBroker, systemID().c_str(), MQTT_SUBTOPIC_HEAP);
        Serial.printf("%s %d\n", topicStr, ESP.getFreeHeap());
        mqtt->publish(topicStr, String(ESP.getFreeHeap()).c_str(), true);
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
        setMessage("publishData", 3);
        JSON[MQTT_SUBTOPIC_CNT] = settings.counterTotal;
        if (ferraris.consumption > 0)
            JSON[MQTT_SUBTOPIC_CONS] =  int(ferraris.consumption * 100) / 100.0;
        if (ferraris.power > -1)
            JSON[MQTT_SUBTOPIC_PWR] = ferraris.power;
        JSON[MQTT_SUBTOPIC_RUNT] = getRuntime(true);
        JSON[MQTT_SUBTOPIC_RSSI] = WiFi.RSSI();
        JSON["version"] = FIRMWARE_VERSION;
#ifdef DEBUG_HEAP
        JSON[MQTT_SUBTOPIC_HEAP] = ESP.getFreeHeap();
#endif
        memset(buf, 0, sizeof(buf));
        size_t s = serializeJson(JSON, buf);
        snprintf(topicStr, sizeof(topicStr), "%s/%s", settings.mqttBaseTopic, systemID().c_str());
        Serial.printf("MQTT %s %s\n", topicStr, buf);
        mqtt->publish(topicStr, buf, s);
    }
}


// publish meter reading updates on single 
// topic as JSON or on multiple topics 
void mqttPublish() {
    if (settings.mqttJSON)
        publishDataJSON();
    else
        publishDataSingle();
}


void mqttDisconnect() {
    if (mqtt != NULL) {
        mqtt->disconnect();
        mqtt->~PubSubClient();
        mqtt = NULL;
    }
}

