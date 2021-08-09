/*
   ESP8266 Wifi Power Meter (Ferraris Counter) v1.2.1

   Hardware: Wemos D1 mini board + TCRT5000 IR sensor
   https://github.com/lrswss/esp8266-wifi-power-meter/

   (c) 2019-2021 Lars Wessels <software@bytebox.org>

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
*/

#include <SPI.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiUDP.h>
#include <ArduinoJson.h>
#include <EEPROM_Rotate.h>

// this value has to be set according
// to the specs of your ferraris meter
#define TURNS_PER_KWH 75

// uncomment for german ui, defaults to english
#define LANGUAGE_EN

#define WIFI_SSID  "<YOUR_SSID>"
#define WIFI_PASS  "<YOUR_WIFI_PASSWORD>"

// uncomment to disable MQTT
#define MQTT_ENABLE

#define MQTT_HOST_IP        "<MQTT_SERVER_IP"
#define MQTT_CLIENT_ID      "PowerMeter_%x"
#define MQTT_BASE_TOPIC     "powermeter"
#define MQTT_SUBTOPIC_CNT   "counter"
#define MQTT_SUBTOPIC_CONS  "consumption"
#define MQTT_SUBTOPIC_RUNT  "runtime"
#define MQTT_SUBTOPIC_RSSI  "rssi"
#define MQTT_SUBTOPIC_HEAP  "freeheap"

// if defined the data is published as JSON on MQTT_BASE_TOPIC
// comment out to post each value on an individual topic
#define MQTT_PUBLISH_JSON

// Uncomment INFLUXDB_ENABLE for debugging purposes:
// Analog readings from the IR sensor are send to an InfluxDB
// which has to configured to accept data on a dedicated UDP port
//#define INFLUXDB_ENABLE
#define INFLUXDB_HOST "<INFLUXDB_SERVER_IP>"
#define INFLUXDB_UDP_PORT 8089
#define INFLUXDB_DEVICE_TAG "TCRT5000"

#define SKETCH_NAME "WifiPowerMeter"
#define SKETCH_VERSION "1.2.1"

// the following settings should not be changed
#define READINGS_TOTAL_SEC 90
#define READINGS_INTERVAL_MS 100
#define READINGS_SPREAD_MIN 4
#define ABOVE_THRESHOLD_TRIGGER 3
#define IMPULSE_DEBOUNCE_SEC 3
#define BACKUP_CYCLE_MIN 60
#define EEPROM_ADDR 10
//#define DEBUG_HEAP

#ifdef LANGUAGE_EN
#include "index_en.h" // HTML-Page (english)
#else
#include "index_de.h" // HTML-Page (german)
#endif

// Setup MQTT client
#ifdef MQTT_ENABLE
WiFiClient espClient;
PubSubClient mqtt(espClient);
#endif

// local webserver on port 80 with OTA-Option
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

// counter values are frequently stored in a
// rotating pseudo EEPROM (actually ESP8266 flash)
struct eeprom {
  uint32_t counterTotal = 0;
  uint32_t counterOffset = 0; // has to be set manually (see README)
  uint16_t impulseThreshold = 0;
} settings;
EEPROM_Rotate EEP;

#ifdef INFLUXDB_ENABLE
WiFiUDP udp;
#endif

uint32_t restartSystem = 0;
bool readThreshold = false;
bool resetThreshold = false;
bool savedThreshold = true;
uint8_t expireMessage = 0;
char message[128];

uint16_t impulseReadings[(int)(READINGS_TOTAL_SEC * 1000 / READINGS_INTERVAL_MS)];
uint16_t readingsSize = sizeof(impulseReadings) / sizeof(impulseReadings[0]);
uint16_t impulseIndex = 0;
uint16_t impulseSpread = 0;
uint16_t impulseMax = 0;
uint16_t impulseMin = 0;


// standard event handler for webserver
void handleRoot() {
  httpServer.send_P(200, "text/html", MAIN_page, sizeof(MAIN_page));
}


// passes updated value to web ui as JSON on AJAX call once a second
// can also be used for (remote) RESTful request
void handleGetReadings() {
  StaticJsonDocument<384> JSON;
  static char reply[360], buf[32];
  float totalConsumption = 0.0;

  if (!settings.impulseThreshold && !readThreshold) {
#ifdef LANGUAGE_EN
    setMessage("Invalid threshold value!", 5);
#else
    setMessage("Ungültiger Schwellwert!", 5);
#endif
  }

  memset(reply, 0, sizeof(reply));
  JSON.clear();
  JSON["totalCounter"] = settings.counterTotal;
  totalConsumption = settings.counterTotal / (TURNS_PER_KWH * 1.0);
  if (settings.counterOffset > 0) {
    totalConsumption += settings.counterOffset / 10.0;
  }
  JSON["totalConsumption"] = int(totalConsumption * 100) / 100.0;
  JSON["runtime"] = getRuntime(false);

  // only relevant for power meter's web ui
  if (httpServer.arg("local").length() >= 1) {
    JSON["impulseThreshold"] = settings.impulseThreshold;
    JSON["readingThreshold"] = readThreshold;
    JSON["currentReadings"] = impulseIndex;
    JSON["totalReadings"] = readingsSize;
    JSON["impulseMin"] = impulseMin;
    JSON["impulseMax"] = impulseMax;
#ifdef INFLUXDB_ENABLE
    JSON["influxEnabled"] = 1;
#else
    JSON["influxEnabled"] = 0;
#endif
    memset(buf, 0, sizeof(buf));
    sprintf(buf, "build on %s at %s", __DATE__, __TIME__);
    JSON["build"] = buf;

    memset(buf, 0, sizeof(buf));
    sprintf(buf, "%s %s", SKETCH_NAME, SKETCH_VERSION);
    JSON["version"] = buf;

    JSON["message"] = message;
    if (!expireMessage) { // keep old message for a while
      memset(message, 0, sizeof(message));
    } else {
      expireMessage--;
    }
#ifdef DEBUG_HEAP
    JSON["freeheap"] = ESP.getFreeHeap();
#endif
  }

  size_t s = serializeJson(JSON, reply);
  setCrossOrigin(); // required for remote REST queries
  httpServer.send_P(200, "text/plain", reply, s);
}


// will restart ESP8266
void handleRestartSystem() {
  restartSystem = millis() + 5000;
#ifdef LANGUAGE_EN
  setMessage("System will restart shortly...", 5);
#else
  setMessage("System wird gleich neu gestartet...", 5);
#endif
  httpServer.send_P(200, "text/plain", "OK", 2);
}


// trigger calculation of new threshold value
void handleCalcThreshold() {
  resetThreshold = true;
  readThreshold = true;
  savedThreshold = false;
  httpServer.send_P(200, "text/plain", "OK", 2);
}


// save new impulse threshold value
void handleSaveThreshold() {
  saveSettingsEEPROM();
  savedThreshold = true;
  impulseIndex = 0;
#ifdef LANGUAGE_EN
  setMessage("New threshold value saved!", 3);
#else
  setMessage("Schwellwert gespeichert!", 3);
#endif
  httpServer.send_P(200, "text/plain", "OK", 2);
}


// will reset the revolution counter to zero
void handleResetCounter() {
  saveSettingsEEPROM();
  settings.counterTotal = 0;
  settings.counterOffset = 0;
#ifdef LANGUAGE_EN
  setMessage("Counter reset successful!", 3);
#else
  setMessage("Zähler zurückgesetzt!", 3);
#endif
  httpServer.send_P(200, "text/plain", "OK", 2);
}


// manually set a counterOffset to adjust the reading for total consumption
// on the embedded web page by sending a request with the current electricity
// meter reading, e.g. http://<ip>/setCounter?value=18231.1
void handleSetCounter() {
  uint32_t counterKWh;

  if (!httpServer.hasArg("value")) {
#ifdef LANGUAGE_EN
    setMessage("Failed to set counter offset!", 3);
#else
    setMessage("Zählerstandsanpassung fehlgeschlagen!", 3);
#endif
  } else {
    Serial.print("setCounter: ");
    Serial.println(httpServer.arg("value"));
    counterKWh = int(httpServer.arg("value").toFloat() * 10);
    settings.counterOffset = counterKWh - lround(settings.counterTotal * 10 / TURNS_PER_KWH);
    saveSettingsEEPROM();
#ifdef LANGUAGE_EN
    setMessage("Counter offset set successfully!", 3);
#else
    setMessage("Zählerstandsanpassung gespeichert!", 3);
#endif
  }
  httpServer.sendHeader("Location", "/", true);
  httpServer.send(302, "text/plain", "");
}


// manually adjust the impulse threshold value
// http://<ip>/setThreshold?value=60
void handleSetThreshold() {

  if (!httpServer.hasArg("value")) {
#ifdef LANGUAGE_EN
    setMessage("Failed to set impulse threshold!", 3);
#else
    setMessage("Schwellwertanpassung fehlgeschlagen!", 3);
#endif
  } else {
    Serial.print("setThreshold: ");
    Serial.println(httpServer.arg("value"));
    settings.impulseThreshold = httpServer.arg("value").toInt();
#ifdef LANGUAGE_EN
    setMessage("Impulse threshold set successfully!", 3);
#else
    setMessage("Schwellwert gespeichert!", 3);
#endif
  }
  httpServer.sendHeader("Location", "/", true);
  httpServer.send(302, "text/plain", "");
}


// required for RESTful api
void setCrossOrigin() {
  httpServer.sendHeader(F("Access-Control-Allow-Origin"), F("*"));
  httpServer.sendHeader(F("Access-Control-Max-Age"), F("600"));
  httpServer.sendHeader(F("Access-Control-Allow-Methods"), F("GET,OPTIONS"));
  httpServer.sendHeader(F("Access-Control-Allow-Headers"), F("*"));
}


// response for REST preflight request
void sendCORS() {
  setCrossOrigin();
  httpServer.sendHeader(F("Access-Control-Allow-Credentials"), F("false"));
  httpServer.send(204);
}


// schedule a message for web ui
void setMessage(const char *msg, int expireSeconds) {
  memset(message, 0, sizeof(message));
  strcpy(message, msg);
  expireMessage = expireSeconds;
}


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
  memset(runtime, 0, sizeof(runtime));
  if (noSpaces) {
    sprintf(runtime, "%dd%dh%dm", days, hours, minutes);
  } else {
    sprintf(runtime, "%dd %dh %dm", days, hours, minutes);
  }

  return runtime;
}


// connect to local wifi network (dhcp) with a 10 second timeout
// after five consecutive connection failures restart ESP
void connectWifi() {
  static uint8_t wifi_error = 0;
  uint8_t wifi_timeout = 10;
  char rssi[16];

  WiFi.mode(WIFI_STA);
#ifdef LANGUAGE_EN
  Serial.print(F("Connecting to SSID "));
#else
  Serial.print(F("Verbinde mit WLAN "));
#endif
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  while (WiFi.status() != WL_CONNECTED && wifi_timeout > 0) {
    Serial.print(F("."));
    delay(500);
    wifi_timeout--;
  }
  if (wifi_timeout > 0) {
    wifi_error = 0;
    Serial.println(F("OK."));
    Serial.print(F("IP address: "));
    Serial.println(WiFi.localIP());
    sprintf(rssi, "RSSI: %ld dBm", WiFi.RSSI());
    Serial.println(rssi);
  } else {
    Serial.println(F("FAILED!"));
    wifi_error++;
  }
  if (wifi_error >= 5) {
    saveSettingsEEPROM();
    delay(100);
#ifdef LANGUAGE_EN
    Serial.println(F("Continuous Wifi connection errors...rebooting!"));
#else
    Serial.println(F("Wiederholte WLAN-Verbindungsfehler...Neustart!"));
#endif
    ESP.restart();
  }
  Serial.println();
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


// connect to MQTT server (with changing id on every attempt)
#ifdef MQTT_ENABLE
bool mqttConnect() {
  static char clientid[32];
  uint8_t mqtt_error = 0;

  // restart wifi connection if down
  if (WiFi.status() != WL_CONNECTED)
    connectWifi();

  while (!mqtt.connected() && mqtt_error < 3) {
    snprintf(clientid, sizeof(clientid), MQTT_CLIENT_ID, random(0xffff));

#ifdef LANGUAGE_EN
    Serial.print(F("Connecting to MQTT Broker "));
    Serial.print(MQTT_HOST_IP); Serial.print(F(" as "));
#else
    Serial.print(F("Verbinde mit MQTT Broker "));
    Serial.print(MQTT_HOST_IP); Serial.print(F(" als "));
#endif
    Serial.print(clientid); Serial.print(F("..."));

    if (mqtt.connect(clientid)) {
      Serial.println(F("OK."));
      mqtt_error = 0;
      return true;
    } else {
      mqtt_error++;
#ifdef LANGUAGE_EN
      Serial.print(F("failed("));
      Serial.print(mqtt.state());
      Serial.println(F("), try again in 3 seconds..."));
#else
      Serial.print(F("Fehler("));
      Serial.print(mqtt.state());
      Serial.println(F("), erneuter Versuch in 3 Sekunden..."));
#endif
      delay(3000);
    }
  }

  if (mqtt_error >= 3) {
#ifdef LANGUAGE_EN
    setMessage("Failed to publish data via MQTT!", 3);
#else
    setMessage("Publizieren der Messwerte fehlgeschlagen!", 3);
#endif
    mqtt_error = 0;
    return false;
  }
}


// publish data on multiple topics
#ifndef MQTT_PUBLISH_JSON
void publishDataSingle() {
  static char topicStr[96];
  float totalConsumption = 0.0;

  if (mqttConnect()) {
    memset(topicStr, 0, sizeof(topicStr));
    sprintf(topicStr, "%s/%s/%s", MQTT_BASE_TOPIC, systemID().c_str(), MQTT_SUBTOPIC_CNT);
    Serial.print(topicStr);
    Serial.print(F(" "));
    Serial.println(settings.counterTotal);
    mqtt.publish(topicStr, String(settings.counterTotal).c_str(), true);
    delay(50);

    // publish total consumption if counter offset values is set
    if (settings.counterOffset > 0) {
      memset(topicStr, 0, sizeof(topicStr));
      sprintf(topicStr, "%s/%s/%s", MQTT_BASE_TOPIC, systemID().c_str(), MQTT_SUBTOPIC_CONS);
      Serial.print(topicStr);
      Serial.print(F(" "));
      totalConsumption = int((settings.counterTotal / (TURNS_PER_KWH * 1.0) + settings.counterOffset / 10.0) * 100) / 100.0;
      Serial.println(totalConsumption);
      mqtt.publish(topicStr, String(totalConsumption).c_str(), true);
      delay(50);
    }

    memset(topicStr, 0, sizeof(topicStr));
    sprintf(topicStr, "%s/%s/%s", MQTT_BASE_TOPIC, systemID().c_str(), MQTT_SUBTOPIC_RUNT);
    Serial.print(topicStr);
    Serial.print(F(" "));
    Serial.println(getRuntime(true));
    mqtt.publish(topicStr, getRuntime(true), true);
    delay(50);

    memset(topicStr, 0, sizeof(topicStr));
    sprintf(topicStr, "%s/%s/%s", MQTT_BASE_TOPIC, systemID().c_str(), MQTT_SUBTOPIC_RSSI);
    Serial.print(topicStr);
    Serial.print(F(" "));
    Serial.println(WiFi.RSSI());
    mqtt.publish(topicStr, String(WiFi.RSSI()).c_str(), true);
    delay(50);

#ifdef DEBUG_HEAP
    memset(topicStr, 0, sizeof(topicStr));
    sprintf(topicStr, "%s/%s/%s", MQTT_BASE_TOPIC, systemID().c_str(), MQTT_SUBTOPIC_HEAP);
    Serial.print(topicStr);
    Serial.print(F(" "));
    Serial.println(ESP.getFreeHeap());
    mqtt.publish(topicStr, String(ESP.getFreeHeap()).c_str(), true);
#endif
  }
}
#endif


// publish all data on base topic as JSON
#ifdef MQTT_PUBLISH_JSON
void publishDataJSON() {
  StaticJsonDocument<128> JSON;
  static char topicStr[96];
  char buf[128];

  if (mqttConnect()) {
    JSON[MQTT_SUBTOPIC_CNT] = settings.counterTotal;
    if (settings.counterOffset > 0) {
      JSON[MQTT_SUBTOPIC_CONS] = int((settings.counterTotal / (TURNS_PER_KWH * 1.0) + settings.counterOffset / 10.0) * 100) / 100.0;
    }
    JSON[MQTT_SUBTOPIC_RUNT] = getRuntime(true);
    JSON[MQTT_SUBTOPIC_RSSI] = WiFi.RSSI();
#ifdef DEBUG_HEAP
    JSON[MQTT_SUBTOPIC_HEAP] = ESP.getFreeHeap();
#endif
    size_t s = serializeJson(JSON, buf);
    sprintf(topicStr, "%s/%s", MQTT_BASE_TOPIC, systemID().c_str());
    Serial.print(topicStr);
    Serial.print(F(" "));
    Serial.println(buf);
    mqtt.publish(topicStr, buf, s);
  }
}
#endif
#endif // MQTT_ENABLE


// get last reading from pseudo eeprom
void readSettingsEEPROM() {
  EEP.get(EEPROM_ADDR, settings);
}


// save current reading to pseudo eeprom
void saveSettingsEEPROM() {
  EEP.put(EEPROM_ADDR, settings);
  EEP.commit();
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


// helper function for qsort() to sort the array with analog readings
int sortAsc(const void *val1, const void *val2) {
  uint16_t a = *((uint16_t *)val1);
  uint16_t b = *((uint16_t *)val2);
  return a - b;
}


// calculate a suitable threshold to detect the red marker
// on ferraris disk in analog sensor readings
void calcThreshold() {

  // sort array with analog readings
  qsort(impulseReadings, readingsSize, sizeof(impulseReadings[0]), sortAsc);

  // min, max and slightly corrected spread of all readings
  impulseMin = impulseReadings[0];
  impulseMax = impulseReadings[readingsSize - 1];
  impulseSpread = impulseReadings[(int)(readingsSize * 0.99)] - impulseReadings[(int)(readingsSize * 0.01)];

  if (impulseSpread >= READINGS_SPREAD_MIN) {
    // My Ferraris disk has a diameter of approx. 9cm => circumference about 28.3cm
    // Length of marker on the disk is more or less 1cm => fraction of circumference about 1/30 => 3%
    // Thus after at least(!) one full revolution of the ferraris disk all analog sensor
    // readings above the 97% percentile should qualify as a suitable threshold values
    settings.impulseThreshold = impulseReadings[(int)(readingsSize * 0.97)];
#ifdef LANGUAGE_EN
    setMessage("Threshold calculation successful!", 5);
    Serial.println(F("Calculation of new threshold succeeded..."));
    Serial.print(F("Threshold: ")); Serial.println(settings.impulseThreshold);
#else
    setMessage("Schwellwertberechnung erfolgreich!", 5);
    Serial.println(F("Neuen Schwellwert erfolgreich berechnet..."));
    Serial.print(F("Schwellwert: ")); Serial.println(settings.impulseThreshold);
#endif
  } else {
    settings.impulseThreshold = 0;
#ifdef LANGUAGE_EN
    setMessage("Readings not sufficient!", 5);
    Serial.println(F("Spread of readings not sufficient for threshold calculation..."));
#else
    setMessage("Messwerte nicht ausreichend!", 5);
    Serial.println(F("Spannweite nicht ausreichend für Schwellwertberechnung..."));
#endif
  }
  Serial.print(F("Minimin: ")); Serial.println(impulseMin);
  Serial.print(F("Maximum: ")); Serial.println(impulseMax);

  // reset arry with analog readingss
  memset(impulseReadings, 0, sizeof(impulseReadings));
}


// try to find a rising edge in previous analog readings
bool findRisingEdge() {
  uint16_t i, belowThresholdTrigger;
  uint8_t s = 1;
  uint16_t belowTh = 0;
  uint16_t aboveTh = 0;

  // reading interval min. 100ms
  if (100 / READINGS_INTERVAL_MS > 1) {
    s = 100 / READINGS_INTERVAL_MS;
  }

  // min. number of impulses below threshold required to detect a rising edge
  belowThresholdTrigger = (IMPULSE_DEBOUNCE_SEC * 1000 / 2) / READINGS_INTERVAL_MS;

  i = impulseIndex > 0 ? impulseIndex : readingsSize - 1;
  while (i - s >= 0 && belowTh <= belowThresholdTrigger && aboveTh <= ABOVE_THRESHOLD_TRIGGER) {
    i -= s;
    if (impulseReadings[i] < settings.impulseThreshold) {
      belowTh++;
    } else {
      aboveTh++;
    }
  }

  // round robin...
  i = readingsSize;
  while (i - s > impulseIndex && belowTh <= belowThresholdTrigger && aboveTh <= ABOVE_THRESHOLD_TRIGGER) {
    i -= s;
    if (impulseReadings[i] < settings.impulseThreshold) {
      belowTh++;
    } else {
      aboveTh++;
    }
  }

  return (aboveTh >= ABOVE_THRESHOLD_TRIGGER && belowTh >= belowThresholdTrigger);
}


// For debugging purposes analog readings can be send to an InfluxDB via UDP
// Visualizing the data with Grafana might help to determine the right threshold value
#ifdef INFLUXDB_ENABLE
void send2influx_udp(uint16_t counter, uint16_t threshold, uint16_t impulse) {
  unsigned long requestTimer = 0;

  // create request with curr2ent/voltage values according to influxdb line protocol
  // https://docs.influxdata.com/influxdb/v1.7/guides/writing_data/
  String content = "esp8266_stromzaehler,device=" + String(INFLUXDB_DEVICE_TAG)
                   + " counter=" + String(counter) + ",threshold=" + String(threshold) + ",impulse=" + String(impulse);

  // send udp packet
  Serial.print("UDP (:" + String(INFLUXDB_HOST) + ":" + String(INFLUXDB_UDP_PORT) + "): " + String(content));
  requestTimer = millis();
  udp.beginPacket(INFLUXDB_HOST, INFLUXDB_UDP_PORT);
  udp.print(content);
  udp.endPacket();
  Serial.println(" (" + String(millis() - requestTimer) + "ms)");
}
#endif


void setup() {
  Serial.begin(115200);
  delay(500);

  Serial.println();
  Serial.print(F("Starting Sketch "));
  Serial.print(SKETCH_NAME);
  Serial.print(F(" "));
  Serial.println(SKETCH_VERSION);
  Serial.print(F("Compiled on "));
  Serial.print(__DATE__);
  Serial.print(F(", "));
  Serial.println(__TIME__);
#ifndef MQTT_ENABLE
  Serial.println(F("MQTT disabled."));
#endif
#ifndef INFLUXDB_ENABLE
  Serial.println(F("InfluxDB disabled."));
#endif
  Serial.println();

  EEP.begin(4096);
  readSettingsEEPROM();
  if (settings.counterTotal >= UINT32_MAX - 1 || settings.counterOffset >= UINT32_MAX - 1 || settings.impulseThreshold >= UINT16_MAX - 1) {
#ifdef LANGUAGE_EN
    Serial.println(F("Resetting invalid values in EEPROM..."));
#else
    Serial.println(F("Ungültige Werte im EEPROM zurückgesetzt..."));
#endif
    if (settings.counterTotal >= UINT32_MAX - 1)
      settings.counterTotal = 0;
    if (settings.counterOffset >= UINT32_MAX - 1)
      settings.counterOffset = 0;
    if (settings.impulseThreshold >= UINT16_MAX - 1)
      settings.impulseThreshold = 0;
    saveSettingsEEPROM();
  } else {
#ifdef LANGUAGE_EN
    Serial.print(F("Counter reading from EEPROM: "));
    Serial.println(settings.counterTotal);
    Serial.print(F("Counter offset stored in EEPROM: "));
    Serial.println(settings.counterOffset / 10.0);
    Serial.print(F("Threshold value read from EEPROM: "));
    Serial.println(settings.impulseThreshold);
    Serial.print(F("EEPROM backup every "));
    Serial.print(BACKUP_CYCLE_MIN);
    Serial.println(F(" minutes."));
#else
    Serial.print(F("Zählerstand im EEPROM: "));
    Serial.println(settings.counterTotal);
    Serial.print(F("Zählerstandsanpassung im EEPROM: "));
    Serial.println(settings.counterOffset / 10.0);
    Serial.print(F("Schwellwert im EEPROM: "));
    Serial.println(settings.impulseThreshold);
    Serial.print(F("EEPROM-Sicherung alle "));
    Serial.print(BACKUP_CYCLE_MIN);
    Serial.println(F(" Minuten."));
#endif
  }
  Serial.println();

  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // Wemos D1 mini: LOW = on, HIGH = off
  pinMode(A0, INPUT);

  connectWifi();
#ifdef MQTT_ENABLE
  mqtt.setServer(MQTT_HOST_IP, 1883);
#endif

  // setup local webserver with all url handlers
  httpUpdater.setup(&httpServer);
  httpServer.begin();
  httpServer.on("/", handleRoot);
  httpServer.on("/calcThreshold", handleCalcThreshold);
  httpServer.on("/saveThreshold", handleSaveThreshold);
  httpServer.on("/resetCounter", handleResetCounter);
  httpServer.on("/readings", handleGetReadings);
  httpServer.on("/readings", HTTP_OPTIONS, sendCORS);
  httpServer.on("/restart", handleRestartSystem);
  httpServer.on("/setCounter", handleSetCounter);
  httpServer.on("/setThreshold", handleSetThreshold);
}


void loop() {
  static uint32_t previousMeasurementMillis = 0;
  static uint32_t previousCountMillis = 0;
  static uint32_t previousSaveMillis = 0;
  static uint8_t aboveThreshold = 0;
  uint32_t currentMillis = millis();
  uint16_t impulseReading = 0;

  // handle http requests
  httpServer.handleClient();

  // determine new threshold value
  if (resetThreshold) {
    resetThreshold = false;
#ifdef LANGUAGE_EN
    setMessage("Started threshold calculation...", 10);
    Serial.println(F("Starting threshold calculation..."));
#else
    setMessage("Schwellwertmessung gestartet...", 10);
    Serial.println(F("Schwellwertmessung gestartet..."));
#endif
    memset(impulseReadings, 0, sizeof(impulseReadings));
    impulseIndex = 0;
    settings.impulseThreshold = 0;
    impulseSpread = 0;
    impulseMax = 0;
    impulseMin = 0;
  }

  // get analog reading from IR sensor
  if (tsDiff(previousMeasurementMillis) > READINGS_INTERVAL_MS) {
    previousMeasurementMillis = currentMillis;
    impulseReading = analogRead(A0);

#ifdef INFLUXDB_ENABLE
    send2influx_udp(settings.counterTotal, settings.impulseThreshold, impulseReading);
#endif

    // Threshold calculation: fill array with analog readings and then
    // try to derive a threshold value to detect a single revolution.
    // While pushing readings to the array for READINGS_TOTAL_SEC seconds
    // make sure that the ferraris disk is spinning fast enough: the
    // red marker should pass by the IR sensor at least twice. You
    // might want to turn on your oven to help speed up things... ;-)
    if (readThreshold) {
      Serial.print(impulseIndex + 1);
#ifdef LANGUAGE_EN
      Serial.print(F(" of "));
      Serial.print(readingsSize);
      Serial.println(F(" readings saved..."));
#else
      Serial.print(F(" von "));
      Serial.print(readingsSize);
      Serial.println(F(" Impulsen eingelesen..."));
#endif

      impulseReadings[impulseIndex++] = impulseReading;
      if (impulseIndex >= readingsSize) {
        readThreshold = false;
        calcThreshold(); // calculate threshold and reset array with readings
      }

    } else if (savedThreshold) {
      // Save readings in a round-robin array to be able to
      // search for a rising edge in recent readings
      impulseReadings[impulseIndex++] = impulseReading;
      if (impulseIndex >= readingsSize) {
        impulseIndex = 0;
      }
    }

    // Only count revolutions if a valid threshold value has been set, since last
    // count at least COUNTER_DEBOUNCE_SEC seconds have passed, the readings have
    // been above the threshold at least COUNTER_ABOVE_THRESHOLD consecutive times
    // and a rising edge was identified in recents readings
    if (settings.impulseThreshold > 0 && (tsDiff(previousCountMillis) > IMPULSE_DEBOUNCE_SEC * 1000) &&
        impulseReading >= settings.impulseThreshold && ++aboveThreshold >= ABOVE_THRESHOLD_TRIGGER && findRisingEdge()) {

      previousCountMillis = millis();
      aboveThreshold = 0;
      blinkLED(4, 50);
      settings.counterTotal++;

#ifdef MQTT_ENABLE
#ifdef MQTT_PUBLISH_JSON
      publishDataJSON();
#else
      publishDataSingle();
#endif
#endif
    }
  }

  // frequently save counter readings and threshold to EEPROM
  if (tsDiff(previousSaveMillis) > BACKUP_CYCLE_MIN * 1000 * 60) {
#ifdef LANGUAGE_EN
    Serial.println("Performing periodic EEPROM backup.");
#else
    Serial.println("Regelmäßige Datensicherung im EEPROM.");
#endif
    previousSaveMillis = millis();
    saveSettingsEEPROM();
  }

  // restart system after request through web ui
  if (restartSystem > 0 && restartSystem < currentMillis) {
    saveSettingsEEPROM();
    delay(100);
    ESP.restart();
  }
}
