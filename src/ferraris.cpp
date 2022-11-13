/***************************************************************************
  Copyright (c) 2019-2022 Lars Wessels

  This file a part of the "ESP8266 Wifi Power Meter" source code.
  https://github.com/lrswss/esp8266-wifi-power-meter

  Licensed under the MIT License. You may not use this file except in
  compliance with the License. You may obtain a copy of the License at

  https://opensource.org/licenses/MIT

***************************************************************************/

#include "config.h"
#include "ferraris.h"
#include "web.h"
#include "utils.h"
#include "influx.h"
#include "nvs.h"


static int16_t *pulseReadings;
static movingAvg pulseInterval(PULSE_HISTORY_SIZE);
bool thresholdCalculation = false;
ferrarisReadings_t ferraris;


// helper function for qsort() to sort the array with analog readings
static int sortAsc(const void *val1, const void *val2) {
    uint16_t a = *((uint16_t *)val1);
    uint16_t b = *((uint16_t *)val2);
    return a - b;
}


// reset sensor readings
static void resetReadings() {
    memset(pulseReadings, 0, ferraris.size * sizeof(int16_t));
    ferraris.consumption = (settings.counterTotal / (settings.turnsPerKwh * 1.0))
        + (settings.counterOffset / 100.0);
    ferraris.power = settings.calculateCurrentPower ? -1 : -2;
    ferraris.index = 0;
    ferraris.spread = 0;
    ferraris.max = 0;
    ferraris.min = 0;
}


// read an averaged value from the TCRT5000 IR sensor
static int16_t readIRSensor() {
    int16_t pulseReading;  // 0-1024
    int16_t sumReadings = 0;

    // average over 10 readings
    for (uint8_t i = 0; i < 10; i++) { 
        sumReadings += analogRead(A0);
        delayMicroseconds(200);
    }
    pulseReading = int(sumReadings/10);

    if (settings.enableInflux)
        send2influx_udp(settings.counterTotal, settings.pulseThreshold, pulseReading);
    return pulseReading;
}


// calculate valid threshold to detect the red marker
// on ferraris disk from analog sensor readings
static void calculateThreshold() {

    // sort array with analog readings
    qsort(pulseReadings, ferraris.size, sizeof(pulseReadings[0]), sortAsc);

    // min, max and slightly corrected spread of all readings
    ferraris.min = pulseReadings[0];
    ferraris.max = pulseReadings[ferraris.size - 1];
    ferraris.spread = pulseReadings[(int)(ferraris.size * 0.99)] - pulseReadings[(int)(ferraris.size * 0.01)];

    if (ferraris.spread >= settings.readingsSpreadMin) {
        // My Ferraris disk has a diameter of approx. 9cm => circumference about 28.3cm
        // Length of marker on the disk is more or less 1cm => fraction of circumference about 1/30 => 3%
        // Thus after at least(!) one full revolution of the ferraris disk all analog sensor
        // readings above the 97% percentile should qualify as a suitable threshold values
        settings.pulseThreshold = pulseReadings[(int)(ferraris.size * 0.98)];
        Serial.println(F("Calculation of new threshold for red marker succeeded."));
        Serial.printf("Threshold (%d), ", settings.pulseThreshold);
        setMessage("thresholdFound", 5);
    } else {
        settings.pulseThreshold = 0;
        Serial.println(F("Spread of sensor readings not sufficient for threshold calculation!"));
        setMessage("thresholdFailed", 5);
    }
    Serial.printf("Minimum(%d), Maximum(%d)\n", ferraris.min, ferraris.max);
}


// try to find a rising edge in previous analog readings
static bool findRisingEdge() {
    uint16_t i, belowThresholdTrigger;
    uint16_t belowTh = 0;
    uint16_t aboveTh = 0;

    // min. number of pulses below threshold required to detect a rising edge
    belowThresholdTrigger = (settings.pulseDebounceMs / 2) / settings.readingsIntervalMs;

    i = ferraris.index > 0 ? ferraris.index : ferraris.size - 1;
    while (i - 1 >= 0 && belowTh <= belowThresholdTrigger &&
                aboveTh <= settings.aboveThresholdTrigger) {
        i -= 1;
        if (pulseReadings[i] < settings.pulseThreshold)
            belowTh++;
        else
            aboveTh++;
    }

    // round robin...
    i = ferraris.size;
    while (i - 1 > ferraris.index && belowTh <= belowThresholdTrigger &&
                aboveTh <= settings.aboveThresholdTrigger) {
        i -= 1;
        if (pulseReadings[i] < settings.pulseThreshold)
            belowTh++;
        else
            aboveTh++;
    }

    return (aboveTh >= settings.aboveThresholdTrigger && belowTh >= belowThresholdTrigger);
}


// Calcultate current power consumption based on moving average
// over given number of seconds. If secs is zero, calculate power
// only based on the most recent pulse interval of the red marker.
// When the ferraris disk rotates rather slowly on low power
// consumption this value can only be updated about once every
// 1-2 minutes on a meter with 75 kwh/turn.
static int16_t calculateCurrentPower(uint16_t secs) {
    uint32_t sumAvg = 0;
    uint8_t pulses = 0;

    if (secs > 0) {
        for (uint8_t i = 1; i <= pulseInterval.getCount(); i++) {
            sumAvg += pulseInterval.getAvg(i);  // tenth of sec
            if (sumAvg)
                pulses++;
            if (sumAvg > (secs * 100))
                break;
        }
    }

    if (!settings.calculateCurrentPower) {
        return -2;
    } else if (pulseInterval.getCount() > 0) {
        pulses = (secs == 0) ? 1 : pulses;  // only consider last pulse interval (no moving avg)
        return int(3600000 / (settings.turnsPerKwh * (pulseInterval.getAvg(pulses)/10.0)));
    } else {
        return -1;
    }
}


// setup pin for IR sensor and allocate array for pulse readings
void initFerraris() {
    ferraris.size = (int)(settings.readingsBufferSec * 1000 / settings.readingsIntervalMs);
    pulseReadings = (int16_t*)malloc(ferraris.size * sizeof(int16_t));  
    if (pulseReadings == NULL) {
        Serial.println(F("malloc() failed"));
        while (true) { 
            toggleLED();
            delay(100);
        }
    }
    pinMode(A0, INPUT);
    pulseInterval.begin();
    resetReadings();
}


// scan ferraris disk for red marker and calibrate threshold
// returns true if system is calibrated and red marker was identified
bool readFerraris() {
    static uint32_t previousCountMillis = 0;
    static uint8_t cutDwnCnt = 0;
    static uint8_t aboveThreshold = 0;
    uint16_t pulseReading;

    pulseReading = readIRSensor();
    pulseReadings[ferraris.index++] = pulseReading;

    // calibration is triggered in web ui
    if (thresholdCalculation) {
        toggleLED();
        // fill up array with analog sensors readings then
        // try to find valid threshold value for red marker
        if (ferraris.index >= ferraris.size) {
            switchLED(false);
            thresholdCalculation = false;
            calculateThreshold();
        }
        return false;

    } else {
        // save readings in a round-robin array to be able 
        // to search for a rising edge in recent readings
        if (ferraris.index >= ferraris.size)
            ferraris.index = 0;
    }

    // only count revolutions if a valid threshold value has been set, since last
    // count at least pulseDebounceMs seconds have passed, the readings have
    // been above the threshold at least aboveThresholdTrigger consecutive times
    // and a rising edge was identified in recents readings
    if (settings.pulseThreshold > 0 && 
            (tsDiff(previousCountMillis) > settings.pulseDebounceMs) &&
            pulseReading >= settings.pulseThreshold && 
            ++aboveThreshold >= settings.aboveThresholdTrigger && 
            findRisingEdge()) {

        // keep history of recents pulse intervals (saved as tenth of second)
        // for optional moving average, see calculateCurrentPower() below
        if (previousCountMillis > 0)
            pulseInterval.reading(tsDiff(previousCountMillis) / 100);

        settings.counterTotal++;
        previousCountMillis = millis();
        aboveThreshold = 0;
        cutDwnCnt = 0;

        // (re)calculate total consumption (kwh) and current power consumption (watt)
        ferraris.consumption = (settings.counterTotal / (settings.turnsPerKwh * 1.0))
                + (settings.counterOffset / 100.0);
        ferraris.power = calculateCurrentPower(settings.calculatePowerMvgAvg ? settings.powerAvgSecs : 0);

        return true;
    }

    // after a peak in consumption, this helps to bring the reading
    // for the most recent pulse interval back down more quickly
    if (pulseInterval.getAvg(1) > 0 &&
            (tsDiff(previousCountMillis)/100 > (pulseInterval.getAvg(cutDwnCnt+1)*(cutDwnCnt+1)))) {
        pulseInterval.reading(tsDiff(previousCountMillis)/100);
        cutDwnCnt++;
    }

    return false;
}


// trigger calibration of threshold value for red marker on ferraris disk
void calibrateFerraris() {
    Serial.println(F("Trying to identify threshold value for red marker..."));
    thresholdCalculation = true;
    settings.pulseThreshold = 0;
    resetReadings();
}
