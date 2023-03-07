/***************************************************************************
   ESP8266 Wifi Power Meter (Ferraris Counter)

   Hardware: Wemos D1 mini board + TCRT5000 IR sensor
   https://github.com/lrswss/esp8266-wifi-power-meter/

   (c) 2019-2023 Lars Wessels <software@bytebox.org>

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
    Serial.println(F("https://github.com/lrswss/esp8266-wifi-power-meter"));
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

    initFerraris();
    startWifi();
    startWebserver();
    getRuntime(false); // start timer
}


void loop() {
    static uint32_t previousMeasurementMillis = 0;
    static uint32_t prevLoopTimer = 0;
    static uint32_t busyTime = 0;

    // scan ferraris disk for red marker
    if (tsDiff(previousMeasurementMillis) > settings.readingsIntervalMs) {
        previousMeasurementMillis = millis();
        if (readFerraris()) {
            blinkLED(2, 200);
            if (settings.enableMQTT && wifiStatus == 1) {
                reconnectWifi();
                mqttPublish();
                if (settings.enablePowerSavingMode)
                    stopWifi(busyTime);
            }
        }
    }

    // run tasks once every second
    if (tsDiff(prevLoopTimer) >= 1000) {
        prevLoopTimer = millis();
        busyTime += 1;

        // regular MQTT publish interval (if enabled)
        // if power saving is enabled, start/stop Wifi before/after MQTT message
        if (settings.enableMQTT) {
            if (!(busyTime % settings.mqttIntervalSecs)) {
                if (settings.enablePowerSavingMode)
                    startWifi();
                mqttPublish();
            }
            // in power saving mode turn off Wifi 2 seconds after publishing data
            // needed to trigger mqttLoop() below before going offline again
            if (busyTime > 2 && !((busyTime-2) % settings.mqttIntervalSecs))
                stopWifi(settings.enablePowerSavingMode ? busyTime : 0);
        }

        // check Wifi uplink and try to reconnect (every 30 sec.) if
        // not in power saving mode; LED is on if Wifi uplink is down
        if (wifiStatus == 1) {
            reconnectWifi();

        // flash LED every 5 seconds if in power saving mode
        } else if (!(busyTime % 5)) {
            blinkLED(1, 50);
        }

        // frequently save counter readings and threshold to EEPROM
        if (!(busyTime % (settings.backupCycleMin * 60)))
            saveNVS(true);
    }

    if (wifiStatus == 1) {
        handleWebrequest();
        if (settings.enableMQTT)
            mqttLoop();
    }
}
