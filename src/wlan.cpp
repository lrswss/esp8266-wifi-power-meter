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
#include "utils.h"

WiFiManager wm;

// connect to local WiFi or automatically start access 
// point with WiFiManager if not yet configured
void initWifi() {
    String apname = String(WIFI_AP_SSID) + "-" + systemID();

    Serial.println(F("Start WiFiManager..."));
    Serial.printf("Check for SSID >> %s << if system does not connect to your WiFi network\n", apname.c_str());
    wm.setDebugOutput(false);
    wm.setMinimumSignalQuality(WIFI_MIN_RSSI);
    wm.setConfigPortalTimeout(WIFI_CONFIG_TIMEOUT_SECS);
    wm.setConnectTimeout(WIFI_CONNECT_TIMEOUT);
    switchLED(true);
    if (!wm.autoConnect(apname.c_str())) {
        Serial.println(F("Failed to connect to a WiFi network...restarting!"));
        blinkLED(20, 100);
        ESP.restart();
    }
    Serial.printf("Connected to SSID %s with RSSI %d dBm on IP ", WiFi.SSID().c_str(), WiFi.RSSI());
    Serial.println(WiFi.localIP());
    blinkLED(4, 100);
}