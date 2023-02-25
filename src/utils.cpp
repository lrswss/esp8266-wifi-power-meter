/***************************************************************************
  Copyright (c) 2019-2023 Lars Wessels

  This file a part of the "ESP8266 Wifi Power Meter" source code.
  https://github.com/lrswss/esp8266-wifi-power-meter

  Licensed under the MIT License. You may not use this file except in
  compliance with the License. You may obtain a copy of the License at

  https://opensource.org/licenses/MIT

***************************************************************************/

#include "utils.h"
#include "wlan.h"
#include "nvs.h"
#include "mqtt.h"


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
// if minutesOnly is set return local runtime in minutes (for display in HA)
char* getRuntime(bool minutesOnly) {
    static uint32_t lastMillis = 0;
    static uint32_t seconds = 0;
    static char runtime[16];

    seconds += tsDiff(lastMillis) / 1000;
    lastMillis = millis();

    if (minutesOnly) {
        sprintf(runtime, "%d", seconds/60);
    } else {
        uint16_t days = seconds / 86400 ;
        uint8_t hours = (seconds % 86400) / 3600;
        uint8_t minutes = ((seconds % 86400) % 3600) / 60;
        sprintf(runtime, "%dd %dh %dm", days, hours, minutes);
    }

    return runtime;
}


// returns hardware system id (ESP's chip id)
// or value set in web ui as 'power meter id'
// used for MQTT topic
String systemID() {
    if (strlen(settings.systemID) < 1) {
        snprintf(settings.systemID, sizeof(settings.systemID)-1, "%06X",ESP.getChipId());
    }
    return String(settings.systemID);
}


// save current counter value to eeprom and restart ESP
void restartSystem() {
    mqttDisconnect(true);
    saveNVS(false);
    Serial.println(F("Restart..."));
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


