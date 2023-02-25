/***************************************************************************
  Copyright (c) 2019-2023 Lars Wessels

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
#include "wlan.h"


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


// determine average pulse reading in the past 'count' number of ADC readings
static uint16_t findPastAverage(uint16_t count) {
    uint32_t average = 0, index = count;

    for (uint16_t i = ferraris.index; i > 0; i--) {
        average += pulseReadings[i];
        if (--index == 0)
            break;
    }
    if (index > 0) {
        for (uint16_t i = ferraris.size-1; i > ferraris.index; i--) {
            average += pulseReadings[i];
            if (--index == 0)
                break;
        }
    }
    return ((average + (count / 2)) / count);
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
    ferraris.offsetNoWifi = 0;
}


// read an averaged value from the TCRT5000 IR sensor (5ms)
static int16_t readIRSensor() {
    int16_t pulseReading;  // 0-1023
    int16_t sumReadings = 0;

    for (uint8_t i = 0; i < 10; i++) { // average over 10 readings
        sumReadings += analogRead(A0); // about 100us
        delayMicroseconds(400);
    }
    pulseReading = int(sumReadings/10);

    if (settings.enableInflux)
        send2influx_udp(settings.counterTotal,
            (settings.pulseThreshold + ferraris.offsetNoWifi), pulseReading);
    return pulseReading;
}


// calculate valid threshold to detect the red marker
// on ferraris disk from analog sensor readings
static void calculateThreshold() {

    // sort array with analog readings
    qsort(pulseReadings, ferraris.size, sizeof(pulseReadings[0]), sortAsc);

    // min, max and slightly corrected spreadw of all readings
    ferraris.min = pulseReadings[0];
    ferraris.max = pulseReadings[ferraris.size - 1];
    ferraris.spread = pulseReadings[(int)(ferraris.size * 0.99)] - pulseReadings[(int)(ferraris.size * 0.01)];

    if (ferraris.spread >= settings.readingsSpreadMin) {
        // My Ferraris disk has a diameter of approx. 9cm => circumference about 28.3cm
        // Length of marker on the disk is more or less 1cm => fraction of circumference about 1/30 => 3%
        // Thus after at least(!) one full rotation of the ferraris disk all analog sensor
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
        if (pulseReadings[i] < (settings.pulseThreshold + ferraris.offsetNoWifi))
            belowTh++;
        else
            aboveTh++;
    }

    // round robin...
    i = ferraris.size;
    while (i - 1 > ferraris.index && belowTh <= belowThresholdTrigger &&
                aboveTh <= settings.aboveThresholdTrigger) {
        i -= 1;
        if (pulseReadings[i] < (settings.pulseThreshold + ferraris.offsetNoWifi))
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
            if (sumAvg > 0)
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


// since ADC readings seem to increase a little bit, if Wifi was
// switched off, calculate an offset for the pulseThreshold
static void setPulseThresholdOffset(bool reset) {
    static uint32_t wifiOffMillis = 0;
    static uint32_t wifiOnMillis = 0;
    static uint16_t noPulseLevelWifiOn = 0;

    if (reset) {
        wifiOnMillis = 0;
        wifiOffMillis = 0;
        ferraris.offsetNoWifi = 0;
        return;
    }

    // remember time when Wifi connection was first
    // available and when it was switched off
    if (!wifiOnMillis && (wifiStatus == 1))
        wifiOnMillis = millis();
    if (!wifiOffMillis && (wifiStatus == 0))
        wifiOffMillis = millis();

    // determine the average baseline pulse reading within the
    // last 30 sec. at least 60 sec. after system startup
    if (!noPulseLevelWifiOn && (wifiOnMillis > 0) && ((millis() - wifiOnMillis) > 60000)) {
        noPulseLevelWifiOn = findPastAverage(30000/settings.readingsIntervalMs);
        Serial.printf("Set average ADC no pulse level to %d\n", noPulseLevelWifiOn);
    }

    // determine the offset for pulse readings if Wifi is off based on
    // the last 20 sec. at least 30 sec. after Wifi was switched off
    if (!ferraris.offsetNoWifi && (wifiOffMillis > 0) && ((millis() - wifiOffMillis) > 30000)) {
        ferraris.offsetNoWifi = findPastAverage(20000/settings.readingsIntervalMs) - noPulseLevelWifiOn;
        Serial.printf("Set ADC offset for inactive Wifi to %d\n", ferraris.offsetNoWifi);
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
    int16_t currentPower;

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

    // only count a rotation if a valid threshold value has been set, since last
    // count at least pulseDebounceMs seconds have passed, the readings have
    // been above the threshold at least aboveThresholdTrigger consecutive times
    // and a rising edge was identified in recents readings
    if (settings.pulseThreshold > 0 && 
            (tsDiff(previousCountMillis) > settings.pulseDebounceMs) &&
            pulseReading >= (settings.pulseThreshold + ferraris.offsetNoWifi) &&
            ++aboveThreshold >= settings.aboveThresholdTrigger &&
            findRisingEdge()) {

        // if Wifi is off but ADC offset is not yet set,
        // ignore possibly false pulse counts
        if (wifiStatus == 0 && !ferraris.offsetNoWifi)
            return false;

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
        currentPower = calculateCurrentPower(0);
        if (settings.calculatePowerMvgAvg) {
            ferraris.power = calculateCurrentPower(settings.powerAvgSecs);
            Serial.printf("Red marker detected (%d rotations), averaged/current power consumption %d/%d W\n",
                settings.counterTotal, ferraris.power, currentPower);
        } else {
            ferraris.power = currentPower;
            Serial.printf("Red marker detected (%d rotations), current power consumption %d W\n",
                settings.counterTotal, ferraris.power);
        }

        return true;
    }

    // after a peak in consumption, this helps to bring
    // down the current (averaged) power reading more quickly
    if (settings.calculatePowerMvgAvg && pulseInterval.getAvg(1) > 0 &&
            (tsDiff(previousCountMillis)/100 > (pulseInterval.getAvg(cutDwnCnt+1)*(cutDwnCnt+1)))) {
        pulseInterval.reading(tsDiff(previousCountMillis)/100);
        cutDwnCnt++;
    } else if (!settings.calculatePowerMvgAvg && ferraris.power > 1000 &&
            (3600000/(ferraris.power * 75)) < (tsDiff(previousCountMillis)/1000 * 0.5)) {
        Serial.printf("Red marker not yet detected, lower current power to %d\n", int(ferraris.power * 0.7));
        ferraris.power = int(ferraris.power * 0.7);
    }

    // since switching off Wifi has an effect on ADC readings a (positiv)
    // offset has to be calculated and added to pulse threshold value
    if (settings.enablePowerSavingMode && !ferraris.offsetNoWifi)
        setPulseThresholdOffset(false);

    return false;
}


// trigger calibration of threshold value for red marker on ferraris disk
void calibrateFerraris() {
    Serial.println(F("Trying to identify threshold value for red marker..."));
    thresholdCalculation = true;
    settings.pulseThreshold = 0;
    resetReadings();
}


// if system switches from power saving mode back to online
// mode (Wifi always on) the ADC offset needs to be reset to 0
void resetWifiOffset() {
    Serial.println(F("Reset ADC offset"));
    setPulseThresholdOffset(true);
}
