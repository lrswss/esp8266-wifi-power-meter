/***************************************************************************
  Copyright (c) 2019-2022 Lars Wessels

  This file a part of the "ESP8266 Wifi Power Meter" source code.
  https://github.com/lrswss/esp8266-wifi-power-meter

  Licensed under the MIT License. You may not use this file except in
  compliance with the License. You may obtain a copy of the License at

  https://opensource.org/licenses/MIT

***************************************************************************/

#include "config.h"
#include "nvs.h"

EEPROM_Rotate EEP;
settings_t settings;
settings_t defaultSettings = {
    0,
    0,
    0,
    TURNS_PER_KWH,
    BACKUP_CYCLE_MIN,
#ifdef CALCULATE_CURRENT_POWER
    true,
#else
    false,
#endif
#if POWER_AVG_SECS > 0
    true,
#else
    false,
#endif
    POWER_AVG_SECS,
    READINGS_BUFFER_SEC,
    READINGS_INTERVAL_MS,
    READINGS_SPREAD_MIN,
    ABOVE_THRESHOLD_TRIGGER,
    PULSE_DEBOUNCE_MS,
#ifdef MQTT_ENABLE
    true,
#else
    false,
#endif
    MQTT_BROKER_HOSTNAME,
    MQTT_BROKER_PORT,
    MQTT_BASE_TOPIC,
    MQTT_PUBLISH_INTERVAL_SEC,
#if defined(MQTT_USERNAME) && defined(MQTT_PASSWORD)
    true,
    MQTT_USERNAME,
    MQTT_PASSWORD,
#else
    false,
    "none",
    "none",
#endif
#ifdef MQTT_PUBLISH_JSON
    true,
#else
    false,
#endif
#ifdef INFLUXDB_ENABLE
    true,
#else
    false,
#endif
    "",
    0x77
};

static void readNVS() {
    settings_t settingsNVS;
    EEP.get(EEPROM_ADDR, settingsNVS);
    if (settingsNVS.magic == 0x77) {
        memcpy(&settings, &settingsNVS, sizeof(settings_t));
        Serial.println(F("Restored system settings from NVS"));
        Serial.printf("Counter(%d), Offset(", settings.counterTotal);
        Serial.print(settings.counterOffset / 10.0);
        Serial.printf("), Threshold(%d)\n", settings.pulseThreshold);
    } else {
        memcpy(&settings, &defaultSettings, sizeof(settings_t));
        Serial.println(F("Initialized NVS with default setttings"));
        saveNVS();
    }
    Serial.printf("Backup settings to NVS every %d minutes\n", settings.backupCycleMin);
}


void initNVS() {
    EEP.begin(4096);
    readNVS();
}


// restore default settings
void resetNVS() {
    Serial.println(F("Reset settings to default values"));
    memcpy(&settings, &defaultSettings, sizeof(settings_t));
    saveNVS();
}


// save current reading to pseudo eeprom
void saveNVS() {
    Serial.println(F("Save settings to NVS"));
    EEP.put(EEPROM_ADDR, settings);
    EEP.commit();
}

