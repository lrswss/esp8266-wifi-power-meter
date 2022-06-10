/***************************************************************************
  Copyright (c) 2019-2022 Lars Wessels

  This file a part of the "ESP8266 Wifi Power Meter" source code.
  https://github.com/lrswss/esp8266-wifi-power-meter

  Licensed under the MIT License. You may not use this file except in
  compliance with the License. You may obtain a copy of the License at

  https://opensource.org/licenses/MIT

***************************************************************************/

#include "config.h"
#include "mqtt.h"
#include "utils.h"
#include "ferraris.h"
#include "web.h"
#include "nvs.h"
#include "wlan.h"

// local webserver on port 80 with OTA-Option
ESP8266WebServer httpServer(80);

static char msgType[32];
static uint8_t msgTimeout;


// required for RESTful api
static void setCrossOrigin() {
    httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
    httpServer.sendHeader(F("Access-Control-Max-Age"), F("600"));
    httpServer.sendHeader(F("Access-Control-Allow-Methods"), F("GET,OPTIONS"));
    httpServer.sendHeader(F("Access-Control-Allow-Headers"), F("*"));
}


// response for REST preflight request
static void sendCORS() {
    setCrossOrigin();
    httpServer.sendHeader(F("Access-Control-Allow-Credentials"), F("false"));
    httpServer.send(204);
}


// passes updated value to web ui as JSON on AJAX call once a second
// can also be used for (remote) RESTful request
static void handleGetReadings() {
    StaticJsonDocument<384> JSON;
    static char reply[256];
    float totalConsumption = 0.0;

    JSON.clear();
    JSON["totalCounter"] = settings.counterTotal;
    totalConsumption = (settings.counterTotal / (settings.turnsPerKwh * 1.0)) 
        + settings.counterOffset / 100.0;
    JSON["totalConsumption"] = int(totalConsumption * 100) / 100.0;
    JSON["runtime"] = getRuntime(false);

    // only relevant for power meter's web ui
    if (httpServer.arg("local").length() >= 1) {
        JSON["pulseThreshold"] = settings.pulseThreshold;
        JSON["thresholdCalculation"] = thresholdCalculation ? 1 : 0;
        JSON["currentReadings"] = ferraris.index;
        JSON["totalReadings"] = ferraris.size;
        JSON["pulseMin"] = ferraris.min;
        JSON["pulseMax"] = ferraris.max;
        if (strlen(msgType) > 0) {
            JSON["msgType"] = msgType;
            JSON["msgTimeout"] = msgTimeout;
            memset(msgType, 0, sizeof(msgType));
            msgTimeout = 0;
        }
#ifdef DEBUG_HEAP
        JSON["freeheap"] = ESP.getFreeHeap();
#endif
    }
    memset(reply, 0, sizeof(reply));
    size_t s = serializeJson(JSON, reply);
    setCrossOrigin(); // required for remote REST queries
    httpServer.send(200, "text/plain", reply, s);
}


// display message for given time in web ui
void setMessage(const char *msg, uint8_t timeSecs) {
    strncpy(msgType, msg, sizeof(msgType));
    msgTimeout = timeSecs;
}


// configure url handler and start web server
void startWebserver() {

    // send main page
    httpServer.on("/", HTTP_GET, []() {
        String html = FPSTR(HEADER_html);
        html += FPSTR(MAIN_html);
        html += FPSTR(FOOTER_html);
        html.replace("__SYSTEMID__", systemID());
        html.replace("__FIRMWARE__", String(FIRMWARE_VERSION/100.0));
        html.replace("__BUILD__", String(__DATE__)+" "+String(__TIME__));
        Serial.println(F("Show main page"));
        httpServer.send(200, "text/html", html);
    });    

    // handler for AJAX requests
    httpServer.on("/readings", HTTP_GET, handleGetReadings);
    httpServer.on("/readings", HTTP_OPTIONS, sendCORS);

    // restart ESP8266
    httpServer.on("/restart", HTTP_GET, []() {
        httpServer.send(200, "text/plain", "OK", 2);
        restartSystem();
    });

    // save default settings to NVS
    httpServer.on("/reset", HTTP_GET, []() {
        httpServer.send(200, "text/plain", "OK", 2);
        WiFi.disconnect(true);
        resetNVS();
        ESP.eraseConfig();
        ESP.restart();
    });

    // trigger calculation of new threshold value
    httpServer.on("/calcThreshold", HTTP_GET, []() {
        httpServer.send(200, "text/plain", "OK", 2);
        calibrateFerraris();
    });

    // save measured threshold value to NVS
    httpServer.on("/saveThreshold", HTTP_GET, []() {
        saveNVS();
        Serial.printf("Saved %d as new threshold for marker detection\n", settings.pulseThreshold);
        httpServer.send(200, "text/plain", "OK", 2);
    });

    // reset all counters to zero
    httpServer.on("/resetCounter", HTTP_GET, []() {
        settings.counterTotal = 0;
        settings.counterOffset = 0;
        saveNVS();
        Serial.println(F("Reset counter and offset"));
        httpServer.send(200, "text/plain", "OK", 2);
    });

    // show upload form for firmware update
    httpServer.on("/update", HTTP_GET, []() {
        String html = FPSTR(HEADER_html);
        html += FPSTR(UPDATE_html);
        html += FPSTR(FOOTER_html);
        html.replace("__SYSTEMID__", systemID());
        html.replace("__FIRMWARE__", String(FIRMWARE_VERSION/100.0));
        html.replace("__BUILD__", String(__DATE__)+" "+String(__TIME__));
        Serial.println(F("Show firmware upload form"));
        httpServer.send(200, "text/html", html);
    });  

    // show main configuration
    httpServer.on("/config", HTTP_GET, []() {
        String html = FPSTR(HEADER_html);
        html += FPSTR(CONFIG_html);
        html += FPSTR(FOOTER_html);
        
        html.replace("__TURNS_KWH__", String(settings.turnsPerKwh));
        html.replace("__CONSUMPTION_KWH__", 
            String((settings.counterTotal / (settings.turnsPerKwh * 1.0)) + settings.counterOffset / 100.0));
        html.replace("__BACKUP_CYCLE__", String(settings.backupCycleMin));

        if (settings.enableMQTT) 
            html.replace("__MQTT__", "checked");
        else
            html.replace("__MQTT__", "");
        html.replace("__MQTT_BROKER__", String(settings.mqttBroker));
        html.replace("__MQTT_BASE_TOPIC__", String(settings.mqttBaseTopic));
        html.replace("__MQTT_INTERVAL__", String(settings.mqttIntervalSecs));
        if (settings.mqttEnableAuth) 
            html.replace("__MQTT_AUTH__", "checked");
        else
            html.replace("__MQTT_AUTH__", "");        
        html.replace("__MQTT_USERNAME__", String(settings.mqttUsername));
        html.replace("__MQTT_PASSWORD__", String(settings.mqttPassword));

        html.replace("__SYSTEMID__", systemID());
        html.replace("__FIRMWARE__", String(FIRMWARE_VERSION/100.0));
        html.replace("__BUILD__", String(__DATE__)+" "+String(__TIME__));
        Serial.println(F("Show main configuration"));
        httpServer.send(200, "text/html", html);
    });   

    // save general settings
    httpServer.on("/config", HTTP_POST, []() {
        if (httpServer.arg("kwh_turns").toInt() >= 50 && httpServer.arg("kwh_turns").toInt() <= 500)
            settings.turnsPerKwh = httpServer.arg("kwh_turns").toInt();
        if (httpServer.arg("consumption_kwh").toFloat() >= 10 && httpServer.arg("consumption_kwh").toFloat() <= 999999)
            settings.counterOffset = (httpServer.arg("consumption_kwh").toFloat() * 100) - 
                lround(settings.counterTotal * 100 / settings.turnsPerKwh);            
        if (httpServer.arg("backup_cycle").toInt() >= 60 && httpServer.arg("backup_cycle").toInt() <= 180)
            settings.backupCycleMin = httpServer.arg("backup_cycle").toInt();  

        if (httpServer.arg("mqtt") == "on") {
            settings.enableMQTT = true;
            if (httpServer.arg("mqttbroker").length() >= 4 && httpServer.arg("mqttbroker").length() <= 63)
                strncpy(settings.mqttBroker, httpServer.arg("mqttbroker").c_str(), 63);
            if (httpServer.arg("mqttbasetopic").length() >= 4 && httpServer.arg("mqttbasetopic").length() <= 63)
                strncpy(settings.mqttBaseTopic, httpServer.arg("mqttbasetopic").c_str(), 63);
            if (httpServer.arg("mqttinterval").toInt() >= 10 && httpServer.arg("mqttinterval").toInt() <= 900)
                settings.mqttIntervalSecs = httpServer.arg("mqttinterval").toInt();
            if (httpServer.arg("mqttauth") == "on") {
                settings.mqttEnableAuth = true;
                if (httpServer.arg("mqttuser").length() >= 4 && httpServer.arg("mqttuser").length() <= 31)
                    strncpy(settings.mqttUsername, httpServer.arg("mqttuser").c_str(), 31);
                if (httpServer.arg("mqttpassword").length() >= 4 && httpServer.arg("mqttpassword").length() <= 31)
                    strncpy(settings.mqttPassword, httpServer.arg("mqttpassword").c_str(), 31);
            } else {
                settings.mqttEnableAuth = false;
            }
        } else {
            settings.enableMQTT = false;
        }

        saveNVS();
        httpServer.sendHeader("Location", "/config?saved", true);
        httpServer.send(302, "text/plain", "");
        Serial.println(F("Updated main configuration"));
    });

    // show expert settings page
    httpServer.on("/expert", HTTP_GET, []() {
        String html = FPSTR(HEADER_html);
        html += FPSTR(EXPERT_html);
        html += FPSTR(FOOTER_html);

        html.replace("__IMPULS_THRESHOLD__", String(settings.pulseThreshold));
        html.replace("__READINGS_SPREAD__", String(settings.readingsSpreadMin));
        html.replace("__READINGS_INTERVAL__", String(settings.readingsIntervalMs));
        html.replace("__READINGS_BUFFER__", String(settings.readingsBufferSec));
        html.replace("__THRESHOLD_TRIGGER__", String(settings.aboveThresholdTrigger));
        html.replace("__DEBOUNCE_TIME__", String(settings.pulseDebounceMs));
        if (settings.enableInflux) 
            html.replace("__INFLUXDB__", "checked");
        else
            html.replace("__INFLUXDB__", "");

        html.replace("__SYSTEMID__", systemID());
        html.replace("__FIRMWARE__", String(FIRMWARE_VERSION/100.0));
        html.replace("__BUILD__", String(__DATE__)+" "+String(__TIME__));
        Serial.println(F("Show expert settings"));
        httpServer.send(200, "text/html", html);
    });

    // save general settings
    httpServer.on("/expert", HTTP_POST, []() {
        if (httpServer.arg("pulse_threshold").toInt() >= 10 && httpServer.arg("pulse_threshold").toInt() <= 1023)
            settings.pulseThreshold = httpServer.arg("pulse_threshold").toInt();
        if (httpServer.arg("readings_spread").toInt() >= 4 && httpServer.arg("readings_spread").toInt() <= 40)
            settings.readingsSpreadMin = httpServer.arg("readings_spread").toInt();
        if (httpServer.arg("readings_interval").toInt() >= 50 && httpServer.arg("readings_interval").toInt() <= 200)
            settings.readingsIntervalMs = httpServer.arg("readings_interval").toInt();
        if (httpServer.arg("readings_buffer").toInt() >= 30 && httpServer.arg("readings_buffer").toInt() <= 120)
            settings.readingsBufferSec = httpServer.arg("readings_buffer").toInt();
        if (httpServer.arg("threshold_trigger").toInt() >= 3 && httpServer.arg("threshold_trigger").toInt() <= 10)
            settings.aboveThresholdTrigger = httpServer.arg("threshold_trigger").toInt();
        if (httpServer.arg("debounce_time").toInt() >= 500 && httpServer.arg("debounce_time").toInt() <= 3000)
            settings.pulseDebounceMs = httpServer.arg("debounce_time").toInt();
        if (httpServer.arg("influxdb") == "on")
            settings.enableInflux = true;
        else
            settings.enableInflux = false;

        saveNVS();
        httpServer.sendHeader("Location", "/expert?saved", true);
        httpServer.send(302, "text/plain", "");
        Serial.println(F("Update expert configuration"));
    });

    // handle firmware upload
    httpServer.on("/update", HTTP_POST, []() {
        String html = FPSTR(HEADER_html);
        if (Update.hasError()) {
            html += FPSTR(UPDATE_ERR_html);
            Serial.println(("OTA failed"));
            blinkLED(4, 50);
        } else {
            html += FPSTR(UPDATE_OK_html);
            Serial.println(("OTA successful"));
            blinkLED(2, 250);
        }
        html += FPSTR(FOOTER_html);
        html.replace("__FIRMWARE__", String(FIRMWARE_VERSION/100.0));
        html.replace("__BUILD__", String(__DATE__)+" "+String(__TIME__));
        httpServer.send(200, "text/html", html);
    }, []() {
        HTTPUpload& upload = httpServer.upload();
        if (upload.status == UPLOAD_FILE_START) {
            Serial.println(F("Starting OTA update..."));
            uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
            if (!Update.begin(maxSketchSpace)) {
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (!Update.end(true)) {
                Update.printError(Serial);
            }
        }
    });

    httpServer.begin();
    Serial.println(F("Webserver started"));
}