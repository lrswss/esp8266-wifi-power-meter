/***************************************************************************
   ESP8266 Wifi Power Meter (Ferraris Counter)

   Hardware: Wemos D1 mini board + TCRT5000 IR sensor
   https://github.com/lrswss/esp8266-wifi-power-meter/

   (c) 2019-2022 Lars Wessels <software@bytebox.org>

   Permission is hereby granted, free of charge, to any person
   obtaining a copy of this software and associated documentation
   files (the "Software"), to deal in the Software without restriction,
   including without limitation the rights to use, copy, modify,
   merge, publish, distribute, sublicense, and/or sell copies of the
   Software, and to permit persons to whom the Software is furnished
   to do so, subject to the following conditions:

   The above copyright notice and this permission notice shall be
   included in all copies or substantial portions of the Software.

   THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
   EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
   MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
   IN NO EVENT SHALL THEAUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
   CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
   TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
   SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

***************************************************************************/

#include "config.h"
#include "mqtt.h"
#include "utils.h"
#include "wlan.h"
#include "ferraris.h"
#include "web.h"
#include "nvs.h"


void setup() {
    Serial.begin(115200);
    delay(1000);

    Serial.printf("\n\nStarting WifiPowerMeter v%d\n", FIRMWARE_VERSION);
    Serial.printf("Compiled on %s, %s\n\n",__DATE__, __TIME__);

    initNVS();
    if (!settings.enableMQTT)
        Serial.println(F("Publishing of meter readings via MQTT disabled"));
    if (!settings.enableInflux)
        Serial.println(F("Streaming of raw sensor readings to InfluxDB disabled"));

    // setup LED on Wemos D1 mini
    // Wemos D1 mini: LOW = on, HIGH = off
    pinMode(LED_BUILTIN, OUTPUT);
    digitalWrite(LED_BUILTIN, HIGH); 

    initWifi();
    initFerraris();
    startWebserver();
}


void loop() {
    static uint32_t previousMeasurementMillis = 0;
    static uint32_t prevLoopTimer = 0;
    static uint32_t busyTime = 0;
    static uint8_t wifiOfflineCounter = 0;

    // scan ferraris disk for red marker
    if (tsDiff(previousMeasurementMillis) > settings.readingsIntervalMs) {
        previousMeasurementMillis = millis();
        if (readFerraris()) {
            blinkLED(2, 200);
            if (settings.enableMQTT)
                mqttPublish();
        }
    }

    // run tasks once every second
    if (tsDiff(prevLoopTimer) >= 1000) {
        prevLoopTimer = millis();
        busyTime += 1;

        // regular MQTT publish interval (if enabled)
        // try to reconnect to WiFi
        if (settings.enableMQTT && !(busyTime % settings.mqttIntervalSecs))
            mqttPublish();

        // blink LED if WiFi is (still) not available
        // try to reconnect every 30 seconds
        if (!WiFi.isConnected()) {
            toggleLED();
            if (wifiOfflineCounter++ >= 30) {
                wifiOfflineCounter = 0;
                wifiReconnectCounter++;
                WiFi.reconnect();
            }
        } else {
            switchLED(false);
        }

        // frequently save counter readings and threshold to EEPROM
        if (!(busyTime % (settings.backupCycleMin * 60)))
            saveNVS();
    }

    httpServer.handleClient();
}
