/***************************************************************************
  Copyright (c) 2019-2023 Lars Wessels

  This file a part of the "ESP8266 Wifi Power Meter" source code.
  https://github.com/lrswss/esp8266-wifi-power-meter

  Licensed under the MIT License. You may not use this file except in
  compliance with the License. You may obtain a copy of the License at

  https://opensource.org/licenses/MIT

***************************************************************************/

#ifndef _WEB_H
#define _WEB_H

#include <Arduino.h>
#include <ESP8266WebServer.h>

#ifdef LANGUAGE_EN
#include "index_en.h" // HTML-Page (english)
#else
#include "index_de.h" // HTML-Page (german)
#endif

void startWebserver();
void stopWebserver();
void handleWebrequest();
void setMessage(const char *msg, uint8_t secs);

#endif