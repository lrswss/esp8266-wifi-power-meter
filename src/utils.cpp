/***************************************************************************
  Copyright (c) 2019-2022 Lars Wessels

  This file a part of the "ESP8266 Wifi Power Meter" source code.
  https://github.com/lrswss/esp8266-wifi-power-meter

  Licensed under the MIT License. You may not use this file except in
  compliance with the License. You may obtain a copy of the License at

  https://opensource.org/licenses/MIT

***************************************************************************/

#include "utils.h"
#include "wlan.h"
#include "nvs.h"

WiFiManager wm;

// rollover safe comparison for given timestamp with millis()
int32_t tsDiff(uint32_t tsMillis) {
  int32_t diff = millis() - tsMillis;
  if (diff < 0)
    return abs(diff);
  else
    return diff;
}


// returns total runtime (day/hours/minutes) as a string
// has internal time keeping to cope with millis() rollover after 49 days
// should be called from time to time to update interal counter
char* getRuntime(bool noSpaces) {
    static uint32_t lastMillis = 0;
    static uint32_t seconds = 0;
    static char runtime[16];

    seconds += tsDiff(lastMillis) / 1000;
    lastMillis = millis();

    uint16_t days = seconds / 86400 ;
    uint8_t hours = (seconds % 86400) / 3600;
    uint8_t minutes = ((seconds % 86400) % 3600) / 60;
    if (noSpaces) {
        sprintf(runtime, "%dd%dh%dm", days, hours, minutes);
    } else {
        sprintf(runtime, "%dd %dh %dm", days, hours, minutes);
    }

    return runtime;
}


// returns hardware system id (last 3 bytes of mac address)
// used for MQTT topic
String systemID() {
    uint8_t mac[6];
    char sysid[7];

    WiFi.macAddress(mac);
    sprintf(sysid, "%02X%02X%02X", mac[3], mac[4], mac[5]);
    return String(sysid);
}


// save current counter value to eeprom and restart ESP
void restartSystem() {
    Serial.println(F("Restart..."));
    saveNVS();
    delay(1000);
    ESP.restart();
}


// blinking led
// Wemos D1 mini: LOW = on, HIGH = off
void blinkLED(uint8_t repeat, uint16_t pause) {
    for (uint8_t i = 0; i < repeat; i++) {
        digitalWrite(LED_BUILTIN, LOW);
        delay(pause);
        digitalWrite(LED_BUILTIN, HIGH);
        if (repeat > 1)
        delay(pause);
    }
}


// toggle state of LED on each call
void toggleLED() {
    static byte ledState = LOW;
    ledState = !ledState;
    digitalWrite(LED_BUILTIN, ledState);
}


// turn LED on or off
// Wemos D1 mini: LOW = on, HIGH = off
void switchLED(bool state) {
    digitalWrite(LED_BUILTIN, state ? LOW : HIGH);
}


