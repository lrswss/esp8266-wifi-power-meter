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
    unsigned long requestTimer = 0;

    // create request with curr2ent/voltage values according to influxdb line protocol
    // https://docs.influxdata.com/influxdb/v1.7/guides/writing_data/
    String content = "esp8266_stromzaehler,device=" + String(INFLUXDB_DEVICE_TAG)
                    + " counter=" + String(counter) + ",threshold=" + String(threshold) + ",pulse=" + String(pulse);

    // send udp packet
    Serial.print("UDP (:" + String(INFLUXDB_HOST) + ":" + String(INFLUXDB_UDP_PORT) + "): " + String(content));
    requestTimer = millis();
    udp.beginPacket(INFLUXDB_HOST, INFLUXDB_UDP_PORT);
    udp.print(content);
    udp.endPacket();
    Serial.println(" (" + String(millis() - requestTimer) + "ms)");
}