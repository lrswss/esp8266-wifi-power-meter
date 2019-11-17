/*
 * ESP8266 Wifi Power Meter (Ferraris Counter)
 * 
 * Hardware: Wemos D1 mini board + TCRT5000 IR sensor
.* https://github.com/lrswss/esp8266-wifi-power-meter/ 
 *
 * 
 * (c) 2019 Lars Wessels <software@bytebox.org>
 * 
 * Permission is hereby granted, free of charge, to any person 
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, 
 * merge, publish, distribute, sublicense, and/or sell copies of the
 * Software, and to permit persons to whom the Software is furnished 
 * to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THEAUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 * 
 */

#include <SPI.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPUpdateServer.h>
#include <WiFiUDP.h>
#include <EEPROM_Rotate.h>

// This value has to be setup according to the 
// specs of your ferraris power meter
#define TURNS_PER_KWH 75

// uncomment for an english ui, defaults to german
//#define LANGUAGE_EN

#define WIFI_SSID  "<YOUR_SSID>"
#define WIFI_PASS  "<YOUR_WIFI_PASSWORD>"

#define MQTT_HOST_IP    "<MQTT_SERVER_IP"
#define MQTT_CLIENT_ID  "PowerMeter_%x"
#define MQTT_TOPIC_CNT  "powermeter/counter"
#define MQTT_TOPIC_RSSI "powermeter/rssi"

// Uncomment INFLUXDB_HOST for debugging purposes:
// Analog readings from the IR sensor are send to an InfluxDB
// which has to configured to accept data on a dedicated UDP port
//#define INFLUXDB_HOST "192.168.10.29"
#define INFLUXDB_UDP_PORT 8089
#define INFLUXDB_DEVICE_TAG "TCRT5000"


#define SKETCH_NAME "WifiPowerMeter"
#define SKETCH_VERSION "1.0"

#define READINGS_TOTAL_SEC 90
#define READINGS_INTERVAL_MS 100
#define READINGS_SPREAD_MIN 4
#define ABOVE_THRESHOLD_TRIGGER 3
#define IMPULSE_DEBOUNCE_SEC 5
#define BACKUP_CYCLE_MIN 120
#define EEPROM_ADDR 10

#ifdef LANGUAGE_EN
#include "index_en.h" // HTML-Page (englisch)
#else
#include "index_de.h" // HTML-Page (german)
#endif

// Setup MQTT client
WiFiClient espClient;
PubSubClient mqtt(espClient);

// local webserver on port 80 with OTA-Option
ESP8266WebServer httpServer(80);
ESP8266HTTPUpdateServer httpUpdater;

// counter values are frequently stored 
// in rotating pseudo EEPROM of ESP8622 chip
uint32_t counterTotal = 0;
uint16_t impulseThreshold = 0;
EEPROM_Rotate EEP;

#ifdef INFLUXDB_HOST
WiFiUDP udp;
#endif

uint32_t restartSystem = 0;
bool readThreshold = false;
bool resetThreshold = false;
bool savedThreshold = true;
uint8_t expireMessage = 0;
char message[128];

uint16_t impulseReadings[(int)(READINGS_TOTAL_SEC*1000/READINGS_INTERVAL_MS)];
uint16_t readingsSize = sizeof(impulseReadings)/sizeof(impulseReadings[0]);
uint16_t impulseIndex = 0;
uint16_t impulseSpread = 0;
uint16_t impulseMax = 0;
uint16_t impulseMin = 0;


// standard event handler for webserver
void handleRoot() {
  httpServer.send_P(200, "text/html", MAIN_page, sizeof(MAIN_page));
}


// Messwerte per AJAX ausliefern
void handleGetReadings() { 
  char reply[128];
  char s[9];

  if (!impulseThreshold && !readThreshold) {  
#ifdef LANGUAGE_EN
    setMessage("Invalid threshold value!", 5);
#else    
    setMessage("Ungültiger Schwellwert!", 5);
#endif    
  }
  
  memset(reply, 0, sizeof(reply));
  strcpy(reply, itoa(counterTotal, s, 10)); strcat(reply, ",");
  dtostrf(counterTotal/(TURNS_PER_KWH*1.0), 8, 2, s);
  strcat(reply, s); strcat(reply, ",");
  strcat(reply, itoa(impulseThreshold, s, 10)); strcat(reply, ",");
  strcat(reply, getRuntime()); strcat(reply, ",");
  strcat(reply, __DATE__); strcat(reply, " "); strcat(reply, __TIME__); strcat(reply, ",");
  strcat(reply, itoa((readThreshold ? 1 : 0), s, 10)); strcat(reply, ",");
  strcat(reply, itoa(impulseIndex, s, 10)); strcat(reply, ",");  
  strcat(reply, itoa(readingsSize, s, 10)); strcat(reply, ",");
  strcat(reply, itoa(impulseMin, s, 10)); strcat(reply, ",");
  strcat(reply, itoa(impulseMax, s, 10)); strcat(reply, ",");
#ifdef INFLUXDB_HOST
  strcat(reply, "1"); strcat(reply, ",");
#else
  strcat(reply, "0"); strcat(reply, ",");
#endif
  httpServer.send_P(200, "text/plain", reply, sizeof(reply));
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


// initate calculation of new threshold value
void handleReadThreshold() {
  resetThreshold = true;
  readThreshold = true;
  savedThreshold = false;
  httpServer.send_P(200, "text/plain", "OK", 2);
}


// save new threshold
void handleSaveThreshold() {
  saveSettingsEEPROM();
  savedThreshold = true;
  impulseIndex = 0;
#ifdef LANGUAGE_EN
  setMessage("New threshold saved!", 3);
#else
  setMessage("Schwellwert gespeichert!", 3);
#endif  
  httpServer.send_P(200, "text/plain", "OK", 2);
}


// will reset resolutions counter to zero
void handleResetCounter() {
  counterTotal = 0;
#ifdef LANGUAGE_EN
  setMessage("Counter reset successful!", 3);
#else
  setMessage("Zähler zurückgesetzt!", 3);
#endif  
  httpServer.send_P(200, "text/plain", "OK", 2);
}


// send current message to web ui
void handleGetMessage() {
  httpServer.send_P(200, "text/plain", message, sizeof(message)); 
  if (!expireMessage) { // keep old message for a while
    memset(message, 0, sizeof(message));
  } else {
    expireMessage--;
  }
}


// schedule a message for web ui
void setMessage(const char *msg, int expireSeconds) {
  memset(message, 0, sizeof(message));
  strcpy(message, msg);
  expireMessage = expireSeconds;
}


// returns total runtime (day/hours/minuntes) as a string
char* getRuntime() {
  long timeNow = millis();
  static char runtime[16];
 
  int days = timeNow / 86400000 ;
  int hours = (timeNow % 86400000) / 3600000;
  int minutes = ((timeNow % 86400000) % 3600000) / 60000;
  int seconds = (((timeNow % 86400000) % 3600000) % 60000) / 10000;
  memset(runtime, 0, sizeof(runtime));
  sprintf(runtime, "%dd %dh %dm", days, hours, minutes);
  
  return runtime;
}


// connect to local wifi network (dhcp)
void connectWifi() {
  char rssi[16];
  
  WiFi.mode(WIFI_STA);
  Serial.print(F("Connecting to SSID "));
  Serial.print(WIFI_SSID);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(F("."));
    delay(500);
  }
  
  Serial.println(F("OK."));
  Serial.print(F("IP address: "));
  Serial.println(WiFi.localIP());
  sprintf(rssi, "RSSI: %ld dBm", WiFi.RSSI());
  Serial.println(rssi);
}


// connect to MQTT server (with changing id on every attempt)
// and publish date (counterTotal and Wifi RSSI)
void publishData() {
  static char clientid[32];
  uint8_t mqtt_error = 0;

  // abort if Wifi is not available
  if (WiFi.status() != WL_CONNECTED)
    return;

  while (!mqtt.connected() && mqtt_error < 3) {
    snprintf(clientid, sizeof(clientid), MQTT_CLIENT_ID, random(0xffff));

    Serial.print(F("Connecting to MQTT Broker ")); 
    Serial.print(MQTT_HOST_IP); Serial.print(F(" as client "));
    Serial.print(clientid); Serial.print(F("..."));

    if (mqtt.connect(clientid)) {
      Serial.println(F("OK."));
      mqtt_error = 0;

      Serial.print(F("Publishing ")); 
      Serial.print(MQTT_TOPIC_CNT);
      Serial.print(F(" "));
      Serial.println(counterTotal);
      mqtt.publish(MQTT_TOPIC_CNT, String(counterTotal).c_str(), true);
      delay(50);

      Serial.print(F("Publishing "));
      Serial.print(MQTT_TOPIC_RSSI);
      Serial.print(F(" "));
      Serial.println(WiFi.RSSI());
      mqtt.publish(MQTT_TOPIC_RSSI, String(WiFi.RSSI()).c_str(), true);         

    } else {
      mqtt_error++;
      Serial.print(F("failed("));
      Serial.print(mqtt.state());
      Serial.println(F("), try again in 3 seconds..."));   
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
  }
}


// get last reading from pseudo eeprom
void readSettingsEEPROM() {
  
  // counter uint32_t (4 bytes)
  byte b4 = EEP.read(EEPROM_ADDR);
  byte b3 = EEP.read(EEPROM_ADDR + 1);
  byte b2 = EEP.read(EEPROM_ADDR + 2);
  byte b1 = EEP.read(EEPROM_ADDR + 3);
  counterTotal = b1; counterTotal <<= 8;
  counterTotal |= b2; counterTotal <<= 8;
  counterTotal |= b3; counterTotal <<= 8;
  counterTotal |= b4;
  
  // threshold uint16_t (2 bytes)
  b2 = EEP.read(EEPROM_ADDR + 4);
  b1 = EEP.read(EEPROM_ADDR + 5);
  impulseThreshold = b1; impulseThreshold <<= 8;
  impulseThreshold |= b2;  
}


// save current reading to pseudo eeprom
void saveSettingsEEPROM() {

    uint32_t _counter = counterTotal;
    byte b4 = _counter; _counter >>= 8;
    byte b3 = _counter; _counter >>= 8;
    byte b2 = _counter; _counter >>= 8;
    byte b1 = _counter;
    EEP.write(EEPROM_ADDR, b4);
    EEP.write(EEPROM_ADDR + 1, b3);
    EEP.write(EEPROM_ADDR + 2, b2);
    EEP.write(EEPROM_ADDR + 3, b1);

    uint16_t _impulseThreshold = impulseThreshold;
    b2 = _impulseThreshold; _impulseThreshold >>= 8;
    b1 = _impulseThreshold;
    EEP.write(EEPROM_ADDR + 4, b2);
    EEP.write(EEPROM_ADDR + 5, b1);
    
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


// calculate a suitable threshold for marker detection from analog sensor readings
void calcThreshold() {
  
  // sort array with analog readings
  qsort(impulseReadings, readingsSize, sizeof(impulseReadings[0]), sortAsc);

  // min, max, median and slightly corrected spread of all readings
  impulseMin = impulseReadings[0];
  impulseMax = impulseReadings[readingsSize-1];
  impulseSpread = impulseReadings[(int)(readingsSize*0.99)]-impulseReadings[(int)(readingsSize*0.01)];
  
  if (impulseSpread >= READINGS_SPREAD_MIN) {
    // My Ferraris disk has a diameter of approx. 9cm => circumference about 28.3cm
    // Length of marker on the disk is more or less 1cm => fraction of circumference about 1/20 => 3%
    // Thus on at least(!) one full revolution of the ferraris disk all analog sensor
    // readings above the 97% percentile should qualify as a suitable threshold value
    impulseThreshold = impulseReadings[(int)(readingsSize*0.97)];
#ifdef LANGUAGE_EN
    setMessage("Threshold calculation successful!", 3);
    Serial.println(F("Calculation of new threshold succeeded..."));
    Serial.print(F("Threshold: ")); Serial.println(impulseThreshold);
#else    
    setMessage("Schwellwertberechnung erfolgreich!", 3);
    Serial.println(F("Neuen Schwellwert erfolgreich berechnet..."));
    Serial.print(F("Schwellwert: ")); Serial.println(impulseThreshold);
#endif   
  } else {
    impulseThreshold = 0;
#ifdef LANGUAGE_EN
    setMessage("Readings not sufficient!", 3);
    Serial.println(F("Spread of readings not sufficient for threshold calculation...")); 
#else
    setMessage("Messwerte nicht ausreichend!", 3);
    Serial.println(F("Spannweite nicht ausreichend für Schwellwertberechnung...")); 
#endif 
  }
  Serial.print(F("Minimin: ")); Serial.println(impulseMin);
  Serial.print(F("Maximum: ")); Serial.println(impulseMax);

  // reset arry with analog readingss
  memset(impulseReadings, 0, sizeof(impulseReadings));
}


// try to find a rise edge in previous analog readings
bool findRisingEdge() {
  uint16_t i, belowThresholdTrigger;
  uint8_t s = 1;
  uint16_t belowTh = 0;
  uint16_t aboveTh = 0;

  // reading interval min. 100ms
  if (100/READINGS_INTERVAL_MS > 1) {
    s = 100/READINGS_INTERVAL_MS;
  }

  // min. number of impulses below threshold required to detect a rising edge
  belowThresholdTrigger = (IMPULSE_DEBOUNCE_SEC*1000/2)/READINGS_INTERVAL_MS;
  
  i = impulseIndex > 0 ? impulseIndex : readingsSize-1;
  while (i-s >= 0 && belowTh <= belowThresholdTrigger && aboveTh <= ABOVE_THRESHOLD_TRIGGER) {
    i -= s;
    if (impulseReadings[i] < impulseThreshold) {
      belowTh++; 
    } else {
      aboveTh++;
    }
  }

  // round robin...
  i = readingsSize;
  while (i-s > impulseIndex && belowTh <= belowThresholdTrigger && aboveTh <= ABOVE_THRESHOLD_TRIGGER) {
    i -= s;
    if (impulseReadings[i] < impulseThreshold) {
      belowTh++;
    } else {
      aboveTh++;
    }
  }

  return (aboveTh >= ABOVE_THRESHOLD_TRIGGER && belowTh >= belowThresholdTrigger);
}


// For debugging purpose analog readings can be send to an InfluxDB via UDP
// Visualizing the data with Grafana might help to determin the right threshold value
#ifdef INFLUXDB_HOST
void send2influx_udp(uint16_t counter, uint16_t threshold, uint16_t impulse) {
  unsigned long requestTimer = 0;
  
  // create request with curr2ent/voltage values according to influxdb line protocol
  // https://docs.influxdata.com/influxdb/v1.7/guides/writing_data/
  String content = "esp8266_stromzaehler,device="+String(INFLUXDB_DEVICE_TAG)
      +" counter="+String(counter)+",threshold="+String(threshold)+",impulse="+String(impulse);
                   
  // send udp packet
  Serial.print("UDP (:"+String(INFLUXDB_HOST)+":"+String(INFLUXDB_UDP_PORT)+"): "+String(content));
  requestTimer = millis();
  udp.beginPacket(INFLUXDB_HOST, INFLUXDB_UDP_PORT);
  udp.print(content);
  udp.endPacket();
  Serial.println(" ("+String(millis()-requestTimer)+"ms)");
}
#endif


void setup() {
  Serial.begin(115200);
  delay(500);
  
  Serial.println();
  Serial.print(F("Starting Sketch "));
  Serial.print(SKETCH_NAME);
  Serial.print(F(" "));
  Serial.print(SKETCH_VERSION);
  Serial.println(F("..."));
  Serial.print(F("Compiled on "));
  Serial.print(__DATE__);
  Serial.print(F(", "));
  Serial.println(__TIME__);

  EEP.begin(4096);
  readSettingsEEPROM();
  if (counterTotal >= UINT32_MAX-1) {
#ifdef LANGUAGE_EN
    Serial.println(F("Resetting invalid values in EEPROM..."));
#else    
    Serial.println(F("Ungültige Werte im EEPROM zurückgesetzt..."));
#endif    
    counterTotal = 0;
    impulseThreshold = 0;
    saveSettingsEEPROM();
  } else {
#ifdef LANGUAGE_EN
    Serial.print(F("Counter reading from EEPROM: "));
    Serial.println(counterTotal);
    Serial.print(F("Threshold value read from EEPROM: "));
    Serial.println(impulseThreshold);
#else   
    Serial.print(F("Zählerstand im EEPROM: "));
    Serial.println(counterTotal);
    Serial.print(F("Schwellwert im EEPROM: "));
    Serial.println(impulseThreshold);
#endif    
  }  
  
  pinMode(LED_BUILTIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);  // Wemos D1 mini: LOW = on, HIGH = off
  pinMode(A0, INPUT);
  
  mqtt.setServer(MQTT_HOST_IP, 1883);
  connectWifi();

  // setup local webserver with all url handlers
  httpUpdater.setup(&httpServer);
  httpServer.begin();
  httpServer.on("/", handleRoot);
  httpServer.on("/readThreshold", handleReadThreshold);
  httpServer.on("/saveThreshold", handleSaveThreshold);
  httpServer.on("/resetCounter", handleResetCounter);
  httpServer.on("/readings", handleGetReadings);
  httpServer.on("/restart", handleRestartSystem);
  httpServer.on("/message", handleGetMessage);
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
    setMessage("Schwellwertmessung gestartet...", 5);
    Serial.println(F("Schwellwertmessung gestartet..."));
#endif
    memset(impulseReadings, 0, sizeof(impulseReadings));
    impulseIndex = 0;
    impulseThreshold = 0;
    impulseSpread = 0;
    impulseMax = 0;
    impulseMin = 0;
  }

  // get analog reading from IR sensor
  if (currentMillis - previousMeasurementMillis > READINGS_INTERVAL_MS) {
    previousMeasurementMillis = currentMillis;
    impulseReading = analogRead(A0);
    
#ifdef INFLUXDB_HOST   
    send2influx_udp(counterTotal, impulseThreshold, impulseReading);
#endif

    // Threshold calculation: fill array with analog readings and then
    // try to derive a threshold value to detect a single revolution.
    // While pushing readings to the array for READINGS_TOTAL_SEC seconds
    // make sure that the ferraris disk is turning fast enough: the
    // red marker should pass by the IR sensor at least twice...turn
    // on your oven for a while might help to speed up things... ;-)   
    if (readThreshold) {
      Serial.print(impulseIndex+1); 
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
    if (impulseThreshold > 0 && (currentMillis - previousCountMillis > IMPULSE_DEBOUNCE_SEC*1000) &&
        impulseReading >= impulseThreshold && ++aboveThreshold >= ABOVE_THRESHOLD_TRIGGER && findRisingEdge()) {
          
      previousCountMillis = millis();
      aboveThreshold = 0;
      blinkLED(4,50);
      counterTotal++;
      publishData();  
    }
  }

  // frequently save counter readings and threshold to EEPROM
  if (currentMillis - previousSaveMillis > BACKUP_CYCLE_MIN*1000*60) {
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
