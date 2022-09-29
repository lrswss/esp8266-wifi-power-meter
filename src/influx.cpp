/***************************************************************************
  Copyright (c) 2019-2022 Lars Wessels

  This file a part of the "ESP8266 Wifi Power Meter" source code.
  https://github.com/lrswss/esp8266-wifi-power-meter

  Licensed under the MIT License. You may not use this file except in
  compliance with the License. You may obtain a copy of the License at

  https://opensource.org/licenses/MIT

***************************************************************************/

#include "config.h"
#include "influx.h"

WiFiUDP udp;

// For debugging purposes analog readings can be send to an InfluxDB via UDP
// Visualizing the data with Grafana might help to determine the right threshold value
void send2influx_udp(uint16_t counter, uint16_t threshold, uint16_t pulse) {
    static char measurement[128];
    uint32_t requestTimer = 0;

    // create udp packet containing raw values according to influxdb line protocol
    // https://docs.influxdata.com/influxdb/v2.4/reference/syntax/line-protocol/
    sprintf(measurement, "esp8266_power_meter,device=%s counter=%d,threshold=%d,pulse=%d\n",
                INFLUXDB_DEVICE_TAG, counter, threshold, pulse);

    // send udp packet
    Serial.printf("UDP (%s:%d): %s", INFLUXDB_HOST, INFLUXDB_UDP_PORT, measurement);
    requestTimer = millis();
    udp.beginPacket(INFLUXDB_HOST, INFLUXDB_UDP_PORT);
    udp.print(String(measurement));
    udp.endPacket();
    Serial.printf(" (%ld ms)", millis() - requestTimer);
}