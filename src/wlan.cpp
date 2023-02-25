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
#include "utils.h"
#include "web.h"
#include "nvs.h"

uint32_t wifiOnlineTenthSecs = 0;
uint16_t wifiReconnectCounter = 0;
int8_t wifiStatus = -1;

static WiFiManager wm;
static String apname = String(WIFI_AP_SSID) + "-" + systemID();
static uint32_t wifiStartMillis;


// start access point to configure WiFi or
// restart WiFi is recently disabled in power saving mode
void startWifi() {
    static uint8_t connectionFailed = 0;

    // WiFi is configured/online
    if (wifiStatus == 1) {
        return;

    } else if (wifiStatus == -1) { // first Wifi connection attempt after power up
        Serial.println(F("\nStart WiFiManager..."));
        Serial.printf("Check for SSID >> %s << if system does not connect to your WiFi network\n", apname.c_str());
        wm.setDebugOutput(false);
        wm.setMinimumSignalQuality(WIFI_MIN_RSSI);
        wm.setConfigPortalTimeout(WIFI_CONFIG_TIMEOUT_SECS);
        wm.setConnectTimeout(WIFI_CONNECT_TIMEOUT);

    } else {
        // only triggered on WiFi restart in power saving mode (wifiStatus = 0)
        wifiStartMillis = millis();
        Serial.printf("Restart WiFi uplink to %s...\n", WiFi.SSID().c_str());
        WiFi.forceSleepWake();
        WiFi.mode(WIFI_STA);
        wm.setConfigPortalTimeout(10); // use shorter timeouts to avoid red marker misses
        wm.setConnectTimeout(20);
    }

    switchLED(true);
    if (!wm.autoConnect(apname.c_str())) {
        // just return if previously working Wifi doesn't repeatly fail
        if (wifiStatus == 1 && ++connectionFailed <= 5)
            return;
        else if (wifiStatus == 0 && ++connectionFailed <= 2)
            return;
        Serial.println(F("Failed to connect to a WiFi network...restarting!"));

        blinkLED(20, 100);
        saveNVS(true);
        ESP.restart();
    }
    Serial.printf("Connected to SSID %s with RSSI %d dBm on IP ", WiFi.SSID().c_str(), WiFi.RSSI());
    Serial.println(WiFi.localIP());
    blinkLED(4, 100);
    connectionFailed = 0;
    wifiStatus = 1;
}


// reconnect to Wifi if at least 30 secs have past since last retry
// turn on system LED if Wifi uplink is down
void reconnectWifi() {
    static uint32_t lastReconnect = 0;

    // no need to reconnect...
    if (WiFi.isConnected())
        return;

    switchLED(true);
    if ((millis() - lastReconnect) > 30000) {
        lastReconnect = millis();

        // count and publish Wifi reconnect tries
        // to help identify an unstable uplink
        wifiReconnectCounter++;

        Serial.printf("Trying to reconnect to SSID %s...", WiFi.SSID().c_str());
        if (WiFi.reconnect()) {
            Serial.println(F("OK"));
            switchLED(false);
        } else {
            Serial.println(F("failed!"));
        }
    }
}


// turn off Wifi in power saving mode
// otherwise reset Wifi timeout counter
void stopWifi(uint32_t currTime) {
    static uint32_t initTime = 0;
    uint32_t wifiOnlineMs = 0;

    if (currTime == 0) {
        initTime = 0; // reset Wifi timeout
        return;
    } else if (!initTime) {
        // start Wifi timeout timer
        initTime = currTime;
    }

    // switch off webserver 5 minutes after power saving mode
    // was enabled or if active after system reset/restart
    if (wifiStatus == 1 && (currTime - initTime) > 300) {
        wifiOnlineMs = millis() - wifiStartMillis;
        if (wifiOnlineMs < 60000)
            Serial.printf("Stopping WiFi to save power (after %d ms)\n", wifiOnlineMs);
        else
            Serial.println(F("Stopping WiFi to save power"));
        WiFi.mode(WIFI_OFF);
        WiFi.forceSleepBegin();
        if (wifiStartMillis > 0)
            wifiOnlineTenthSecs += wifiOnlineMs/100;
        wifiStatus = 0;
    }
}
