/***************************************************************************
  Copyright (c) 2019-2023 Lars Wessels

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


// publish JSON on given MQTT topic
static bool publishJSON(JsonDocument& json, char *topic, bool retain, bool verbose) {
    static char buf[592];
    bool rc = false;
    size_t bytes;

    memset(buf, 0, sizeof(buf));
    bytes = serializeJson(json, buf, sizeof(buf)-1);
    if (json.overflowed()) {
        Serial.printf("MQTT %s aborted, JSON overflow!\n", topic);
    } else {
        if (mqtt->publish(topic, buf, retain)) {
            rc = true;
            if (verbose)
                Serial.printf("MQTT %s %s\n", topic, buf);
            else
                Serial.printf("MQTT %s (%d bytes)\n", topic, bytes);
        } else {
            Serial.printf("MQTT %s failed!\n", topic);
        }
    }
    json.clear();
    delay(50);
    return rc;
}


// add device description to HA discovery sensor topic
static void addDeviceDescription(JsonDocument& json) {
    JsonObject dev = json.createNestedObject("dev");
    dev["name"] = "WiFi Power Meter " + String(settings.systemID);
    dev["ids"] = settings.systemID;
    dev["cu"] = "http://" + WiFi.localIP().toString();
    dev["mdl"] = "ESP8266";
    dev["mf"] = "https://github.com/lrswss";
    dev["sw"] = FIRMWARE_VERSION;
}


// published or deletes Home Assistant auto discovery message
// https://www.home-assistant.io/docs/mqtt/discovery/
static void publishHADiscoveryMessage(bool publish) {
    DynamicJsonDocument JSON(640);
    static char systemID[17], mqttBaseTopic[65];
    static bool discoveryPublished = false;
    char devTopic[96], topicTotalCon[80], topicPower[80], topicRSSI[80];
    char topicRuntime[80], topicCount[80], topicWifiCnt[96], topicWifiOnAir[96];

    // message topic didn't change skip HA discovery message
    if ((discoveryPublished == publish) && !strncmp(systemID, settings.systemID, 64) &&
            !strncmp(mqttBaseTopic, settings.mqttBaseTopic, 64))
        return;

    if (publish) {
        // if message topic has changed update HA discovery message
        if ((strlen(systemID) > 0) && (strlen(mqttBaseTopic) > 0) &&
            ((strncmp(systemID, settings.systemID, 64) != 0) ||
            (strncmp(mqttBaseTopic, settings.mqttBaseTopic, 64) != 0))) {
            publishHADiscoveryMessage(false);
            delay(250);
        }
        strlcpy(systemID, settings.systemID, 16);
        strlcpy(mqttBaseTopic, settings.mqttBaseTopic, 64);
    }

    snprintf(devTopic, sizeof(devTopic), "%s/%s/state", mqttBaseTopic, systemID);
    snprintf(topicCount, sizeof(topicCount),
        "%s%s/pulse_count/config", MQTT_TOPIC_DISCOVER, systemID);
    snprintf(topicTotalCon, sizeof(topicTotalCon),
        "%s%s/total_consumption/config", MQTT_TOPIC_DISCOVER, systemID);
    snprintf(topicPower, sizeof(topicPower),
        "%s%s/current_power/config", MQTT_TOPIC_DISCOVER, systemID);
    snprintf(topicRSSI, sizeof(topicRSSI),
        "%s%s/signal_strength/config", MQTT_TOPIC_DISCOVER, systemID);
    snprintf(topicWifiCnt, sizeof(topicWifiCnt),
        "%s%s/wifi_reconnect_counter/config", MQTT_TOPIC_DISCOVER, systemID);
    snprintf(topicWifiOnAir, sizeof(topicWifiOnAir),
        "%s%s/wifi_powersaving_uptime/config", MQTT_TOPIC_DISCOVER, systemID);
    snprintf(topicRuntime, sizeof(topicRuntime),
        "%s%s/runtime/config", MQTT_TOPIC_DISCOVER, systemID);

    if (publish) {
        Serial.printf("Sending Home Assistant MQTT discovery message for %s...\n", devTopic);

        JSON["name"] = "WiFi Power Meter " + String(settings.systemID) + " Ferraris Impuls Counter";
        JSON["unique_id"] = "wifipowermeter-" + String(settings.systemID)+ "-impuls-counter";
        JSON["ic"] = "mdi:rotate-360";
        JSON["stat_t"] = devTopic;
        JSON["val_tpl"] = "{{ value_json."+ String(MQTT_SUBTOPIC_CNT) +" }}";
        addDeviceDescription(JSON);
        publishJSON(JSON, topicCount, true, false);

        JSON["name"] = "WiFi Power Meter " + String(settings.systemID) + " Total Consumption";
        JSON["unique_id"] = "wifipowermeter-" + String(settings.systemID)+ "-total-consumption";
        JSON["ic"] = "mdi:counter";
        JSON["unit_of_meas"] = "kWh";
        JSON["dev_cla"] = "energy";
        JSON["stat_cla"] = "total_increasing";
        JSON["stat_t"] = devTopic;
        JSON["val_tpl"] = "{{ value_json."+ String(MQTT_SUBTOPIC_CONS) +" }}";
        addDeviceDescription(JSON);
        publishJSON(JSON, topicTotalCon, true, false);

        JSON["name"] = "WiFi Power Meter " + String(settings.systemID) + " Current Power Consumption";
        JSON["unique_id"] = "wifipowermeter-" + String(settings.systemID)+ "-current-power";
        JSON["ic"] = "mdi:lightning-bolt";
        JSON["unit_of_meas"] = "W";
        JSON["dev_cla"] = "power";
        JSON["stat_cla"] = "measurement";
        JSON["stat_t"] = devTopic;
        JSON["val_tpl"] = "{{ value_json."+ String(MQTT_SUBTOPIC_PWR) +" }}";
        addDeviceDescription(JSON);
        publishJSON(JSON, topicPower, true, false);

        JSON["name"] = "WiFi Power Meter " + String(settings.systemID) + " WiFi Signal Strength";
        JSON["unique_id"] = "wifipowermeter-" + String(settings.systemID)+ "-rssi";
        JSON["unit_of_meas"] = "dBm";
        JSON["dev_cla"] = "signal_strength";
        JSON["stat_t"] = devTopic;
        JSON["val_tpl"] = "{{ value_json."+ String(MQTT_SUBTOPIC_RSSI) +" }}";
        addDeviceDescription(JSON);
        publishJSON(JSON, topicRSSI, true, false);

        if (settings.enablePowerSavingMode) {
            JSON["name"] = "WiFi Power Meter " + String(settings.systemID) + " WiFi Power Saving Uptime";
            JSON["unique_id"] = "wifipowermeter-" + String(settings.systemID)+ "-wifi-powersaving-uptime";
            JSON["unit_of_meas"] = "s"; // seconds
            JSON["ic"] = "mdi:wifi-arrow-up-down";
            JSON["dev_cla"] = "duration";
            JSON["stat_t"] = devTopic;
            JSON["val_tpl"] = "{{ value_json."+ String(MQTT_SUBTOPIC_ONAIR) +" }}";
            addDeviceDescription(JSON);
            publishJSON(JSON, topicWifiOnAir, true, false);
        } else {
            JSON["name"] = "WiFi Power Meter " + String(settings.systemID) + " WiFi Reconnect Counter";
            JSON["unique_id"] = "wifipowermeter-" + String(settings.systemID)+ "-wifi-reconnect-counter";
            JSON["ic"] = "mdi:wifi-alert";
            JSON["stat_t"] = devTopic;
            JSON["val_tpl"] = "{{ value_json."+ String(MQTT_SUBTOPIC_WIFI) +" }}";
            addDeviceDescription(JSON);
            publishJSON(JSON, topicWifiCnt, true, false);
        }

        JSON["name"] = "WiFi Power Meter " + String(settings.systemID) + " Uptime";
        JSON["unique_id"] = "wifipowermeter-" + String(settings.systemID)+ "-uptime";
        JSON["unit_of_meas"] = "m"; // minutes
        JSON["ic"] = "mdi:clock-outline";
        JSON["dev_cla"] = "duration";
        JSON["stat_t"] = devTopic;
        JSON["val_tpl"] = "{{ value_json."+ String(MQTT_SUBTOPIC_RUNT) +" }}";
        addDeviceDescription(JSON);
        publishJSON(JSON, topicRuntime, true, false);
        discoveryPublished = true;

    } else if (strlen(devTopic) > 8) {
        // send empty (retained) message to delete sensor autoconfiguration
        Serial.printf("Removing Home Assistant MQTT discovery message for %s...\n", devTopic);

        mqtt->publish(topicCount, "", true);
        delay(50);
        mqtt->publish(topicTotalCon, "", true);
        delay(50);
        mqtt->publish(topicPower, "", true);
        delay(50);
        mqtt->publish(topicRSSI, "", true);
        delay(50);
        mqtt->publish(topicWifiCnt, "", true);
        delay(50);
        mqtt->publish(topicWifiOnAir, "", true);
        delay(50);
        mqtt->publish(topicRuntime, "", true);

        memset(devTopic, 0, sizeof(devTopic));
        discoveryPublished = false;
    }
}


// checks for remote commands (powersave mode, mqttinterval)
static void mqttCallback(char* topic, byte* payload, unsigned int length) {
    uint16_t valInt;
    char valStr[8];

    if (!length) // avoid loop on mqttUnsetTopic()
        return;

    // convert string payload to integer
    strncpy(valStr, (const char*)payload, length);
    valStr[length] = '\0';
    valInt = atoi(valStr);

    // enable/disable power saving mode
    if (strstr(topic, MQTT_SUBTOPIC_PSAVE) != NULL) {
        if (valInt != 0) {
            settings.enablePowerSavingMode = true;
            settings.calculatePowerMvgAvg = true;
            settings.powerAvgSecs = POWER_AVG_SECS_POWERSAVING;
            if (settings.mqttIntervalSecs < MQTT_INTERVAL_MIN_POWERSAVING)
                settings.mqttIntervalSecs = MQTT_INTERVAL_MIN_POWERSAVING;
        } else {
            settings.enablePowerSavingMode = false;
            settings.mqttIntervalSecs = MQTT_PUBLISH_INTERVAL_SEC;
            resetWifiOffset(); // unset ADC offset used when Wifi is offline
        }
        Serial.printf("MQTT %s: %sable power saving mode (MQTT publish interval %d seconds)\n",
            topic, settings.enablePowerSavingMode ? "en" : "dis", settings.mqttIntervalSecs);
        mqttUnsetTopic(MQTT_SUBTOPIC_PSAVE);

    // set mqtt publish interval
    } else if (strstr(topic, MQTT_SUBTOPIC_TXINT) != NULL) {
        if (settings.enablePowerSavingMode && valInt < MQTT_INTERVAL_MIN_POWERSAVING) {
            settings.mqttIntervalSecs = MQTT_INTERVAL_MIN_POWERSAVING;
        } else if (valInt < MQTT_INTERVAL_MIN) {
            settings.mqttIntervalSecs = MQTT_INTERVAL_MIN;
        } else if (valInt > MQTT_INTERVAL_MAX) {
            valInt = MQTT_INTERVAL_MAX;
        } else {
            settings.mqttIntervalSecs = valInt;
        }
        Serial.printf("MQTT command %s: set MQTT publish interval to %d seconds\n",
            topic, settings.mqttIntervalSecs);
        mqttUnsetTopic(MQTT_SUBTOPIC_TXINT);

    } else if (strstr(topic, MQTT_SUBTOPIC_RST) != NULL && valInt > 0) {
        Serial.printf("MQTT command %s: restart system\n", topic);
        mqttUnsetTopic(MQTT_SUBTOPIC_RST);
        restartSystem();
    }
}


static void mqttInit() {
    if (mqtt != NULL)
        return;

    if (settings.mqttSecure) {
        espClientSecure.setInsecure();
        // must reduce memory usage with Maximum Fragment Length Negotiation (supported by mosquitto)
        espClientSecure.probeMaxFragmentLength(settings.mqttBroker, settings.mqttBrokerPort, 1024);
        espClientSecure.setBufferSizes(1024, 1024);
        mqtt = new PubSubClient(espClientSecure);
    } else {
        mqtt = new PubSubClient(espClient);
    }
    mqtt->setServer(settings.mqttBroker, settings.mqttBrokerPort);
    mqtt->setBufferSize(672); // for home assistant MQTT device discovery
    mqtt->setSocketTimeout(2); // keep web ui responsive
    mqtt->setKeepAlive(settings.mqttIntervalSecs + 10);
    mqtt->setCallback(mqttCallback);
}


// subscribe to command topics
static void subCmdTopics() {
    static char topic[96];

    snprintf(topic, sizeof(topic)-1, "%s/%s/cmd/%s",
        settings.mqttBaseTopic, systemID().c_str(), MQTT_SUBTOPIC_PSAVE);
    mqtt->subscribe(topic);
    delay(50);

    snprintf(topic, sizeof(topic)-1, "%s/%s/cmd/%s",
        settings.mqttBaseTopic, systemID().c_str(), MQTT_SUBTOPIC_TXINT);
    mqtt->subscribe(topic);
    delay(50);

    snprintf(topic, sizeof(topic)-1, "%s/%s/cmd/%s",
        settings.mqttBaseTopic, systemID().c_str(), MQTT_SUBTOPIC_RST);
    mqtt->subscribe(topic);
    delay(50);
}


// connect to MQTT server (with changing id on every attempt)
// give up after three consecutive connection failures and
// retry after MQTT_CONN_RETRY_SECS seconds
static bool mqttConnect() {
    static char clientid[32];
    static uint32 mqttErrorMillis = 0;
    uint8_t mqttError = 0;

    if (mqttErrorMillis > 0 && tsDiff(mqttErrorMillis) < (MQTT_CONN_RETRY_SECS * 1000))
        return false;

    if (!WiFi.isConnected()) {
        Serial.printf("WiFi not available, cannot connect to MQTT broker %s\n", settings.mqttBroker);
        mqttErrorMillis = millis();
        return false;
    }

    mqttInit();
    if (mqtt->connected()) {
        publishHADiscoveryMessage(settings.enableHADiscovery);
        mqttErrorMillis = 0;
        return true;
    }

    while (!mqtt->connected() && mqttError < 3) {
        snprintf(clientid, sizeof(clientid), MQTT_CLIENT_ID, (int)random(0xfffff));
        Serial.printf("Connecting to MQTT broker %s as %s", settings.mqttBroker, clientid);
        if (settings.mqttEnableAuth)
            Serial.printf(" with username %s", settings.mqttUsername);
        Serial.printf(" on port %d%s...", settings.mqttBrokerPort, settings.mqttSecure ? " (TLS)" : "");
        if (settings.mqttEnableAuth && mqtt->connect(clientid, settings.mqttUsername, settings.mqttPassword)) {
            Serial.println(F("OK"));
            subCmdTopics();
            publishHADiscoveryMessage(settings.enableHADiscovery);
            mqttErrorMillis = 0;
            mqttError = 0;
            return true;
        } else if (!settings.mqttEnableAuth && mqtt->connect(clientid)) {
            Serial.println(F("OK"));
            subCmdTopics();
            publishHADiscoveryMessage(settings.enableHADiscovery);
            mqttErrorMillis = 0;
            mqttError = 0;
            return true;
        } else {
            mqttError++;
            Serial.printf("failed (error %d)\n", mqtt->state());
            delay(250);
        }
    }

    if (mqttError >= 3) {
        setMessage("mqttConnFailed", 3);
        Serial.printf("Retry MQTT connection in %d seconds...\n", MQTT_CONN_RETRY_SECS);
        mqttErrorMillis = millis();
        mqttError = 0;
        return false;
    }

    return true;
}


// publish empty message to unset given cmd topic (retained)
void mqttUnsetTopic(const char* topic) {
    char topicStr[128];

    if (mqttConnect()) {
        snprintf(topicStr, sizeof(topicStr), "%s/%s/cmd/%s",
            settings.mqttBaseTopic, systemID().c_str(), topic);
        if (mqtt->publish(topicStr, new byte[0], 0, true)) {
            Serial.printf("MQTT unset %s\n", topicStr);
        } else {
            Serial.printf("MQTT unset %s failed!\n", topicStr);
        }
    }
}


// publish data on multiple topics
static void publishDataSingle() {
    static char topicStr[128];
    uint8_t mqttError = 0;

    if (mqttConnect()) {
        snprintf(topicStr, sizeof(topicStr), "%s/%s/state/%s",
            settings.mqttBaseTopic, systemID().c_str(), MQTT_SUBTOPIC_CNT);
        if (mqtt->publish(topicStr, String(settings.counterTotal).c_str(), false))
            Serial.printf("MQTT %s %d\n", topicStr, settings.counterTotal);
        else {
            Serial.printf("MQTT %s failed!\n", topicStr);
            mqttError++;
        }
        delay(50);

        // need counter offset to publish total consumption (kwh)
        if (ferraris.consumption > 0) {
            snprintf(topicStr, sizeof(topicStr), "%s/%s/state/%s",
                settings.mqttBaseTopic, systemID().c_str(), MQTT_SUBTOPIC_CONS);
            if (mqtt->publish(topicStr, String(ferraris.consumption, 2).c_str(), false)) {
                Serial.printf("MQTT %s ", topicStr);
                Serial.println(ferraris.consumption, 2); // float!
            } else {
                Serial.printf("MQTT %s failed!\n", topicStr);
                mqttError++;
            }
            delay(50);
        }

        if (ferraris.power > -1) {
            snprintf(topicStr, sizeof(topicStr), "%s/%s/state/%s",
            settings.mqttBaseTopic, systemID().c_str(), MQTT_SUBTOPIC_PWR);
            if (mqtt->publish(topicStr, String(ferraris.power).c_str(), false))
                Serial.printf("MQTT %s %d\n", topicStr, ferraris.power);
            else {
                Serial.printf("MQTT %s failed!\n", topicStr);
                mqttError++;
            }
            delay(50);
        }

        snprintf(topicStr, sizeof(topicStr), "%s/%s/state/%s",
            settings.mqttBaseTopic, systemID().c_str(), MQTT_SUBTOPIC_TXINT);
        if (mqtt->publish(topicStr, String(settings.mqttIntervalSecs).c_str(), false))
            Serial.printf("MQTT %s %d\n", topicStr, settings.mqttIntervalSecs);
        else {
            Serial.printf("MQTT %s failed!\n", topicStr);
            mqttError++;
        }
        delay(50);

        snprintf(topicStr, sizeof(topicStr), "%s/%s/state/%s",
            settings.mqttBaseTopic, systemID().c_str(), MQTT_SUBTOPIC_RUNT);
        if (mqtt->publish(topicStr, getRuntime(true), true))
            Serial.printf("MQTT %s %s\n", topicStr, getRuntime(true));
        else {
            Serial.printf("MQTT %s failed!\n", topicStr);
            mqttError++;
        }
        delay(50);

        snprintf(topicStr, sizeof(topicStr), "%s/%s/state/%s",
            settings.mqttBaseTopic, systemID().c_str(), MQTT_SUBTOPIC_RSSI);
        if (mqtt->publish(topicStr, String(WiFi.RSSI()).c_str(), false))
            Serial.printf("MQTT %s %d\n", topicStr, WiFi.RSSI());
        else {
            Serial.printf("MQTT %s failed!\n", topicStr);
            mqttError++;
        }
        delay(50);

        snprintf(topicStr, sizeof(topicStr), "%s/%s/state/%s",
            settings.mqttBaseTopic, systemID().c_str(), MQTT_SUBTOPIC_PSAVE);
        if (mqtt->publish(topicStr, String(settings.enablePowerSavingMode ? 1 : 0).c_str(), false))
            Serial.printf("MQTT %s %d\n", topicStr, settings.enablePowerSavingMode ? 1 : 0);
        else {
            Serial.printf("MQTT %s failed!\n", topicStr);
            mqttError++;
        }
        delay(50);

        if (settings.enablePowerSavingMode) {
            snprintf(topicStr, sizeof(topicStr), "%s/%s/state/%s",
                settings.mqttBaseTopic, systemID().c_str(), MQTT_SUBTOPIC_ONAIR);
            if (mqtt->publish(topicStr, String(wifiOnlineTenthSecs/10).c_str(), false))
                Serial.printf("MQTT %s %d\n", topicStr, wifiOnlineTenthSecs/10);
            else {
                Serial.printf("MQTT %s failed!\n", topicStr);
                mqttError++;
            }
            delay(50);
        } else {
            snprintf(topicStr, sizeof(topicStr), "%s/%s/state/%s",
                settings.mqttBaseTopic, systemID().c_str(), MQTT_SUBTOPIC_WIFI);
            if (mqtt->publish(topicStr, String(wifiReconnectCounter).c_str(), false))
                Serial.printf("MQTT %s %d\n", topicStr, wifiReconnectCounter);
            else {
                Serial.printf("MQTT %s failed!\n", topicStr);
                mqttError++;
            }
            delay(50);
        }

        snprintf(topicStr, sizeof(topicStr), "%s/%s/state/version",
            settings.mqttBaseTopic, systemID().c_str());
        if (mqtt->publish(topicStr, String(FIRMWARE_VERSION).c_str(), false))
            Serial.printf("MQTT %s %d\n", topicStr, FIRMWARE_VERSION);
        else {
            Serial.printf("MQTT %s failed!\n", topicStr);
            mqttError++;
        }
        delay(50);

#ifdef DEBUG_HEAP
        snprintf(topicStr, sizeof(topicStr), "%s/%s/state/%s",
            settings.mqttBroker, systemID().c_str(), MQTT_SUBTOPIC_HEAP);
        if (mqtt->publish(topicStr, String(ESP.getFreeHeap()).c_str(), false))
            Serial.printf("%s %d\n", topicStr, ESP.getFreeHeap());
        else {
            Serial.printf("MQTT %s failed!\n", topicStr);
            mqttError++;
        }
#endif
    }
    if (!mqttError)
        setMessage("publishData", 3);
    else
        setMessage("publishFailed", 3);
}


// publish data on base topic as JSON
static void publishDataJSON() {
    StaticJsonDocument<192> JSON;
    static char topicStr[128];

    JSON.clear();
    if (mqttConnect()) {
        JSON[MQTT_SUBTOPIC_CNT] = settings.counterTotal;
        if (ferraris.consumption > 0)
            JSON[MQTT_SUBTOPIC_CONS] =  int(ferraris.consumption * 100) / 100.0;
        if (ferraris.power > -1)
            JSON[MQTT_SUBTOPIC_PWR] = ferraris.power;
        JSON[MQTT_SUBTOPIC_TXINT] = settings.mqttIntervalSecs;
        JSON[MQTT_SUBTOPIC_RUNT] = atoi(getRuntime(true));
        if (!settings.enablePowerSavingMode) {
            JSON[MQTT_SUBTOPIC_PSAVE] = 0;
            JSON[MQTT_SUBTOPIC_WIFI] = wifiReconnectCounter;
        } else {
            JSON[MQTT_SUBTOPIC_PSAVE] = 1;
            JSON[MQTT_SUBTOPIC_ONAIR] = wifiOnlineTenthSecs/10;
        }
        JSON[MQTT_SUBTOPIC_RSSI] = WiFi.RSSI();
        JSON["version"] = FIRMWARE_VERSION;
#ifdef DEBUG_HEAP
        JSON[MQTT_SUBTOPIC_HEAP] = ESP.getFreeHeap();
#endif
        snprintf(topicStr, sizeof(topicStr), "%s/%s/state",
            settings.mqttBaseTopic, systemID().c_str());
        if (publishJSON(JSON, topicStr, false, true))
            setMessage("publishData", 3);
        else
            setMessage("publishFailed", 3);
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


void mqttDisconnect(bool unsetHAdiscovery) {
    if (unsetHAdiscovery)
        publishHADiscoveryMessage(false);
    if (mqtt != NULL) {
        mqtt->disconnect();
        mqtt->~PubSubClient();
        mqtt = NULL;
    }
}


// check for remote commands
void mqttLoop() {
    if (mqttConnect())
        mqtt->loop();
}
