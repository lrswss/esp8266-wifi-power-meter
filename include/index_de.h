/***************************************************************************
  Copyright (c) 2019-2023 Lars Wessels

  This file a part of the "ESP8266 Wifi Power Meter" source code.
  https://github.com/lrswss/esp8266-wifi-power-meter

  Licensed under the MIT License. You may not use this file except in
  compliance with the License. You may obtain a copy of the License at

  https://opensource.org/licenses/MIT

***************************************************************************/

const char HEADER_html[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="author" content="(c) 2019-2023 Lars Wessels">
<meta name="description" content="https://github.com/lrswss/esp8266-wifi-power-meter/">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no"/>
<title>Wifi Stromz&auml;hler __SYSTEMID__</title>
<style>
body{text-align:center;font-family:verdana;background:white;}
div,fieldset,input,select{padding:5px;font-size:1em;}
h2{margin-top:2px;margin-bottom:8px;width:340px}
h3{font-size:0.8em;margin-top:-2px;margin-bottom:2px;font-weight:lighter;}
p{margin:0.5em 0;}
a{text-decoration:none;color:inherit;}
input{width:100%;box-sizing:border-box;-webkit-box-sizing:border-box;-moz-box-sizing:border-box;}
input[type=checkbox],input[type=radio]{width:1em;margin-right:6px;vertical-align:-1px;}
p,input[type=text]{font-size:0.96em;}
select{width:100%;}
button{border:0;border-radius:0.3rem;background:#009374;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;-webkit-transition-duration:0.4s;transition-duration:0.4s;cursor:pointer;}
button:hover{background:#007364;}
button:focus {outline:0;}
td{text-align:right;}
.bred{background:#d43535;}
.bred:hover{background:#931f1f;}
.bgrn{background:#47c266;}
.bgrn:hover{background:#5aaf6f;}
.bdisabled{pointer-events: none;cursor: not-allowed;opacity: 0.65;filter: alpha(opacity=65);}
.footer{font-size:0.6em;color:#aaa;}
</style>
)=====";


const char MAIN_html[] PROGMEM = R"=====(
<script>
var thresholdCalculation = 0;
var thresholdSaved = 1;
var pulseThreshold = -1;
var currentPower = -2;
var totalCounter = 0;
var messageShown = 0;
var msgType = 0;
var msgTimeout = 0;

setInterval(function() {
  getReadings();
  updateUI();
}, 1000);

function updateUI() {
  if (msgType != 0)
    showMessage(msgType, msgTimeout);
  if (thresholdCalculation || (!thresholdSaved && pulseThreshold > 0)) {
    document.getElementById("tr3").style.display = "table-row";
    document.getElementById("tr4").style.display = "table-row";
    document.getElementById("tr5").style.display = "table-row";
    document.getElementById("calcThreshold").style.display = "none";
    document.getElementById("saveThreshold").style.display = "block";
    if (pulseThreshold > 0) {
      document.getElementById("btnSaveThreshold").classList.remove("bdisabled");
    } else if (pulseThreshold == 0) {
      showMessage("thresholdCalculation", 0);
      document.getElementById("btnSaveThreshold").classList.add("bdisabled");
    }
  } else if (messageShown == 0) {
    if (pulseThreshold == 0) {
      showMessage("invalidThreshold", 0);
    }
    document.getElementById("tr3").style.display = "none";
    document.getElementById("tr4").style.display = "none";
    document.getElementById("tr5").style.display = "none";
    document.getElementById("calcThreshold").style.display = "block";
    document.getElementById("saveThreshold").style.display = "none";
  }
  if (totalCounter <= 0)
    document.getElementById("resetCounter").style.display = "none";
  else
    document.getElementById("resetCounter").style.display = "block";
  if (currentPower > -2)
    document.getElementById("tr1").style.display = "table-row";
  else
    document.getElementById("tr1").style.display = "none";
}

function hideMessages() {
  document.getElementById("message").style.display = "none";
  document.getElementById("invalidThreshold").style.display = "none";
  document.getElementById("thresholdCalculation").style.display = "none";
  document.getElementById("thresholdReset").style.display = "none";
  document.getElementById("thresholdSaved").style.display = "none";
  document.getElementById("thresholdFound").style.display = "none";
  document.getElementById("thresholdFailed").style.display = "none";
  document.getElementById("mqttConnFailed").style.display = "none";
  document.getElementById("publishFailed").style.display = "none";
  document.getElementById("restartSystem").style.display = "none";
  document.getElementById("publishData").style.display = "none";
  messageShown = 0;
}

function showMessage(message, timeout) {
  hideMessages();
  document.getElementById("message").style.display = "block";
  document.getElementById(message).style.display = "block";
  document.getElementById("heading").scrollIntoView();
  if (timeout > 0)
    setTimeout(hideMessages, timeout-50);
  messageShown = 1;
}

function calcThreshold() {
  var xhttp = new XMLHttpRequest();
  if (confirm("Aktuellen Schwellwert wirklich löschen?")) {
    xhttp.open("GET", "calcThreshold", true);
  	xhttp.send();
    pulseThreshold = -1;
  	thresholdSaved = 0;
  }
}

function saveThreshold() {
  var xhttp = new XMLHttpRequest();
  xhttp.open("GET", "saveThreshold", true);
  xhttp.send();
  showMessage("thresholdSaved", 3000);
  thresholdSaved = 1;
}

function resetCounter() {
  var xhttp = new XMLHttpRequest();
  if (confirm("Zählerstand wirklich löschen?")) {
    xhttp.open("GET", "resetCounter", true);
  	xhttp.send();
    showMessage("thresholdReset", 3000);
  }
}

function restartSystem() {
  var xhttp = new XMLHttpRequest();
  if (confirm("Wirklich neu starten?")) {
    setTimeout(function(){location.href='/';}, 15000);
    showMessage("restartSystem", 15000);
  	xhttp.open("GET", "restart", true);
  	xhttp.send();
  }
}

function getReadings() {
  var xhttp = new XMLHttpRequest();
  var json;
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      json = JSON.parse(xhttp.responseText);
      msgType = 0;
      if ("msgType" in json && "msgTimeout" in json) {
        msgType = json.msgType;
        msgTimeout = (json.msgTimeout * 1000);
      }
      document.getElementById("TotalCounter").innerHTML = json.totalCounter;
      document.getElementById("TotalConsumption").innerHTML = (json.totalConsumption > 0 ? json.totalConsumption : "--");
      document.getElementById("CurrentPower").innerHTML = (json.currentPower > 0 ? json.currentPower : "--");
      document.getElementById("Runtime").innerHTML = json.runtime;
      document.getElementById("RSSI").innerHTML = json.rssi;
      document.getElementById("CurrentReadings").innerHTML = json.currentReadings;
      document.getElementById("TotalReadings").innerHTML = json.totalReadings;
      document.getElementById("PulseMin").innerHTML = (json.pulseMin > 0 ? json.pulseMin : "--");
      document.getElementById("PulseMax").innerHTML = (json.pulseMax > 0 ? json.pulseMax : "--");
      document.getElementById("PulseThreshold").innerHTML = (json.pulseThreshold > 0 ? json.pulseThreshold : "--");
      pulseThreshold = json.pulseThreshold;
      thresholdCalculation = json.thresholdCalculation;
      totalCounter = json.totalCounter;
      currentPower = json.currentPower;
   	} 
  };
  xhttp.open("GET", "readings?local=true", true);
  xhttp.send();
}
</script>
</head>

<body onload="hideMessages(); getReadings(); updateUI();">
<div style="text-align:left;display:inline-block;min-width:340px;">
<div style="text-align:center;">
<h2 id="heading">Stromz&auml;hler __SYSTEMID__</h2>
<div id="message" style="display:none;margin-top:10px;color:red;text-align:center;font-weight:bold;max-width:335px">
<span id="invalidThreshold" style="display:none">Ungültiger Schwellwert</span>
<span id="thresholdCalculation" style="display:none">Schwellwertmessung, bitte warten</span>
<span id="thresholdFound" style="display:none;color:green">Schwellwertberechnung erfolgreich</span>
<span id="thresholdReset" style="display:none;color:green">Zähler zurückgesetzt</span>
<span id="thresholdSaved" style="display:none;color:green">Schwellwert gespeichert</span>
<span id="thresholdFailed" style="display:none">Messwerte nicht ausreichend</span>
<span id="mqttConnFailed" style="display:none">MQTT-Verbindung fehlgeschlagen</span>
<span id="publishFailed" style="display:none">Publizieren über MQTT fehlgeschlagen</span>
<span id="restartSystem" style="display:none">System wird neu gestartet...</span>
<span id="publishData" style="display:none;color:green">Daten über MQTT publiziert</span>
</div>
<noscript>Bitte JavaScript aktivieren!<br/></noscript>
</div>
<div id="readings" style="margin-top:5px;">
<table style="min-width:340px">
	<tr><th>Anzahl Umdrehungen:</th><td><span id="TotalCounter">--</span></td></tr>
	<tr id="tr1"><th>Momentanleistung:</th><td><span id="CurrentPower">--</span> W</td></tr>
	<tr><th>Gesamtverbrauch:</th><td><span id="TotalConsumption">--</span> kWh</td></tr>
	<tr id="tr3"><th>A/D Messwerte:</th><td><span id="CurrentReadings">--</span>/<span id="TotalReadings">--</span></td></tr>
    <tr id="tr4"><th>Minimum/Maximum:</th><td><span id="PulseMin">--</span>/<span id="PulseMax">--</span></td></tr>
	<tr id="tr5"><th>Impulsschwellwert:</th><td><span id="PulseThreshold">--</span></td></tr>
	<tr><th>Laufzeit:</th><td><span id="Runtime">--d --h --m</span></td></tr>
    <tr><th>WLAN RSSI:</th><td><span id="RSSI">--</span> dBm</td></tr>
</table>
</div>
<div id="buttons" style="margin-top:5px">
<p id="resetCounter"><button onclick="resetCounter()">Z&auml;hler zur&uuml;cksetzen</button><p>
<p id="calcThreshold"><button onclick="calcThreshold()">Schwellwert einmessen</button><p>
<p id="saveThreshold"><button id="btnSaveThreshold" onclick="saveThreshold()">Schwellwert speichern</button><p>
<p><button onclick="location.href='/config';">Einstellungen</button></p>
<p><button onclick="location.href='/update';">Firmware aktualisieren</button></p>
<p><button class="button bred" onclick="restartSystem()">System neu starten</button></p>
</div>
)=====";


const char EXPERT_html[] PROGMEM = R"=====(
<script>
var messageShown = 0;

function hideMessages() {
  document.getElementById("message").style.display = "none";
  document.getElementById("configSaved").style.display = "none";
  document.getElementById("restartSystem").style.display = "none";
  document.getElementById("resetSystem").style.display = "none";
  messageShown = 0;
}

function showMessage(message, timeout) {
  hideMessages();
  document.getElementById("message").style.display = "block";
  document.getElementById(message).style.display = "block";
  document.getElementById("heading").scrollIntoView();
  setTimeout(hideMessages, timeout);
  messageShown = 1;
}

function restartSystem() {
  var xhttp = new XMLHttpRequest();
  setTimeout(function(){location.href='/';}, 15000);
  showMessage("restartSystem", 15000);
  xhttp.open("GET", "restart", true);
  xhttp.send();
}

function resetSystem() {
  if (confirm("Alle Einstellungen zurücksetzen und neu starten?")) {
    var xhttp = new XMLHttpRequest();
    setTimeout(function(){location.href='/';}, 15000);
    showMessage("resetSystem", 15000);
    xhttp.open("GET", "reset", true);
    xhttp.send();
  }
}

function confirmRestart() {
    if (confirm("Einstellungen speichern und neu starten?"))
        return true;
    return false;
}

function configSaved() {
  const url = new URLSearchParams(window.location.search);
    if (url.has("saved")) {
      showMessage("configSaved", 2500)
      setTimeout(restartSystem, 3000);
    }
}

</script>
</head>
<body onload="hideMessages(); configSaved();">
<div style="text-align:left;display:inline-block;min-width:340px;">
<div style="text-align:center;">
<h2 id="heading">Experteneinstellungen</h2>
<div id="message" style="display:none;margin-top:10px;color:red;font-weight:bold;text-align:center;max-width:335px">
<span id="configSaved" style="display:none;color:green">Einstellungen gespeichert</span>
<span id="restartSystem" style="display:none;color:red">System wird neu gestartet...</span>
<span id="resetSystem" style="display:none">Nach dem Systemreset wird ein WLAN Access Point zur Neukonfiguration gestartet!<br></span>
</div>
</div>

<div style="max-width:335px;margin-top:10px;">
<form method="POST" action="/expert" onsubmit="return confirmRestart();">
  <fieldset><legend><b>&nbsp;Ferraris-Abtastung&nbsp;</b></legend>
  <p><b>Schwellwert für Zählung (__PULSE_THRESHOLD_MIN__-__PULSE_THRESHOLD_MAX__)</b><br />
  <input id="input_pulse_threshold" name="pulse_threshold" size="16" maxlength="4" value="__PULSE_THRESHOLD__" onkeyup="digitsOnly(this);"></p>
   <p><b>Varianz für Erkennung (__READINGS_SPREAD_MIN__-__READINGS_SPREAD_MAX__)</b><br />
  <input id="input_readings_spread" name="readings_spread" size="16" maxlength="2" value="__READINGS_SPREAD__" onkeyup="digitsOnly(this);"></p>
  <p><b>Abtastrate IR-Sensor (__READINGS_INTERVAL_MS_MIN__-__READINGS_INTERVAL_MS_MAX__)</b><br />
  <input id="input_readings_interval" name="readings_interval" size="16" maxlength="3" value="__READINGS_INTERVAL_MS__" onkeyup="digitsOnly(this);"></p>
  <p><b>Ringspeicher (__READINGS_BUFFER_SECS_MIN__-__READINGS_BUFFER_SECS_MAX__ Sek.)</b><br />
  <input id="input_readings_buffer" name="readings_buffer" size="16" maxlength="3" value="__READINGS_BUFFER_SECS__" onkeyup="digitsOnly(this);"></p>
  <p><b>Pulse für Zählung (__THRESHOLD_TRIGGER_MIN__-__THRESHOLD_TRIGGER_MAX__)</b><br />
  <input id="input_threshold_tigger" name="threshold_trigger" size="16" maxlength="2" value="__THRESHOLD_TRIGGER__" onkeyup="digitsOnly(this);"></p>
  <p><b>Totzeit Zählungen (__DEBOUNCE_TIME_MS_MIN__-__DEBOUNCE_TIME_MS_MAX__ ms)</b><br />
  <input id="input_debounce_time" name="debounce_time" size="16" maxlength="4" value="__DEBOUNCE_TIME_MS__" onkeyup="digitsOnly(this);"></p>
  </fieldset>
  <br />

  <fieldset><legend><b>&nbsp;InfluxDB&nbsp;</b></legend>
  <p><input id="checkbox_influxdb" name="influxdb" type="checkbox" __INFLUXDB__><b>Sensorrohdaten streamen</b></p>
  </fieldset>

  <br />
  <p><button class="button bred" type="submit">Speichern und Neustart</button></p>
</form>
<p><button onclick="resetSystem();">System zurücksetzen</button></p>
<p><button onclick="location.href='/config';">Einstellungen</button></p>
<p><button onclick="location.href='/';">Startseite</button></p>
</div>
)=====";


const char CONFIG_html[] PROGMEM = R"=====(
<script>
var messageShown = 0;

function hideMessages() {
  document.getElementById("message").style.display = "none";
  document.getElementById("configSaved").style.display = "none";
  document.getElementById("counterError").style.display = "none";
  document.getElementById("kwhError").style.display = "none";
  document.getElementById("mqttError").style.display = "none";
  document.getElementById("mqttAuthError").style.display = "none";
  document.getElementById("restartSystem").style.display = "none";
  messageShown = 0;
}

function showMessage(message, timeout) {
  hideMessages();
  document.getElementById("message").style.display = "block";
  document.getElementById(message).style.display = "block";
  document.getElementById("heading").scrollIntoView();
  setTimeout(hideMessages, timeout);
  messageShown = 1;
}

function restartSystem() {
  var xhttp = new XMLHttpRequest();
  setTimeout(function(){location.href='/';}, 15000);
  showMessage("restartSystem", 15000);
  xhttp.open("GET", "restart", true);
  xhttp.send();
}

function configSaved() {
  const url = new URLSearchParams(window.location.search);
  if (url.has("saved"))
    showMessage("configSaved", 4000);
}

function digitsOnly(input) {
  var regex = /[^0-9]/g;
  input.value = input.value.replace(regex, "");
}

function checkInput() {
  var err = 0;
  var height = 0;
  var xhttp = new XMLHttpRequest();

  if (document.getElementById("input_kwh_turns").value.length < 2 ||
    Number(document.getElementById("input_kwh_turns").value) < 50 ||
    Number(document.getElementById("input_kwh_turns").value) > 800) {
    document.getElementById("kwhError").style.display = "block";
    err++;
  }

  if (document.getElementById("input_consumption_kwh").value.length < 3 ||
    Number(document.getElementById("input_consumption_kwh").value) > 999999) {
    document.getElementById("counterError").style.display = "block";
    err++;
  }

  if (document.getElementById("checkbox_mqtt").checked == true &&
    (document.getElementById("input_mqttbroker").value.length < 4 ||
     document.getElementById("input_mqttbasetopic").value.length < 4 ||
     document.getElementById("input_mqttport").value.length < 4)) {
    document.getElementById("mqttError").style.display = "block";
    err++;
  }

  if (document.getElementById("checkbox_mqttauth").checked == true && 
    (document.getElementById("input_mqttuser").value.length < 4 ||
     document.getElementById("input_mqttpassword").value.length < 4)) {
    document.getElementById("mqttAuthError").style.display = "block";
    err++;
  }

  if (err > 0) {
    height = (err * 12) + 4;
    document.getElementById("message").style.height = height + "px";
    document.getElementById("message").style.display = "block";
    document.getElementById("heading").scrollIntoView();
    setTimeout(hideMessages, 4000);
    return false;
  } else {
    document.getElementById("message").style.display = "none";
    return true;
  }
}

function digitsOnly(input) {
  var regex = /[^0-9]/g;
  input.value = input.value.replace(regex, "");
}

function floatsOnly(input) {
  var regex = /[^0-9\.]/g;
  input.value = input.value.replace(/,/, ".").replace(regex, "");
}

function ASCIIOnly(input) {
  var regex = /[^0-9A-Z/_\-\.]/ig;
  input.value = input.value.replace(regex, "")
}

function toggleMQTT() {
  if (document.getElementById("checkbox_mqtt").checked == true) {
    document.getElementById("mqtt").style.display = "block";
  } else {
    document.getElementById("mqtt").style.display = "none";
  }
}

function toggleMQTTAuth() {
  if (document.getElementById("checkbox_mqttauth").checked == true) {
    document.getElementById("mqttauth").style.display = "block";
  } else {
    document.getElementById("mqttauth").style.display = "none";
  }
}

function toggleMQTTSecure() {
  if (document.getElementById("checkbox_mqtt_secure").checked == true) {
    document.getElementById("input_mqttport").value = 8883;
  } else {
    document.getElementById("input_mqttport").value = 1883;
  }
}

function togglePower() {
  if (document.getElementById("checkbox_power").checked == true) {
    document.getElementById("power").style.display = "block";
  } else {
    document.getElementById("power").style.display = "none";
  }
}

function togglePowerAvg() {
  if (document.getElementById("checkbox_power_avg").checked == true) {
    document.getElementById("power_avg").style.display = "block";
  } else {
    document.getElementById("power_avg").style.display = "none";
  }
}

function togglePowerSaving(warning) {
  if (document.getElementById("checkbox_powersaving").checked == true) {
    if (warning)
      alert('ACHTUNG: Die Weboberfläche ist nach dem Aktivieren des Energieparmodus '+
        'nur noch 5 Minuten erreichbar. Danach wird die WLAN-Verbindung abgeschaltet '+
        'und fortlaufend immer nur für wenige Sekunden zur Datenübermittelung per MQTT aktiviert. '+
        'Ein Zugriff auf die Weboberfläche ist nur durch einen Neustart des Wifi Power Meter '+
        'oder das Senden einer MQTT Nachricht (__MQTT_BASE_TOPIC__/__SYSTEMID__/cmd/powersave 0) '+
        'mit Retain Flag möglich. Für die Momentanleistung wird automatisch der gleitende ' +
        'Durchschnitt aktiviert.');
      document.getElementById("checkbox_power_avg").checked = true;
      document.getElementById("input_power_avg_secs").value = __POWER_AVG_SECS_POWERSAVING__;
      togglePowerAvg();
    if (Number(document.getElementById("input_mqttinterval").value) < __MQTT_INTERVAL_MIN_POWERSAVING__)
        document.getElementById("input_mqttinterval").value = __MQTT_INTERVAL_MIN_POWERSAVING__;
  }
}

function toggleJSON() {
  if (document.getElementById("checkbox_mqtt_json").checked == false) {
    document.getElementById("checkbox_mqtt_ha_discovery").checked = false;
  }
}

function toggleHADiscovery() {
  if (document.getElementById("checkbox_mqtt_ha_discovery").checked == true) {
    document.getElementById("checkbox_mqtt_json").checked = true;
  }
}

</script>
</head>
<body onload="configSaved(); togglePower(); toggleMQTT(); toggleMQTTAuth(); toggleMQTTSecure(); togglePowerAvg(); togglePowerSaving(false); toggleJSON(); toggleHADiscovery();">
<div style="text-align:left;display:inline-block;min-width:340px;">
<div style="text-align:center;">
<h2 id="heading">Einstellungen</h2>
<div id="message" style="display:none;margin-top:10px;color:red;font-weight:bold;text-align:center;max-width:335px">
<span id="configSaved" style="display:none;color:green">Einstellungen gespeichert</span>
<span id="counterError" style="display:none">Zählerstand prüfen</span>
<span id="kwhError" style="display:none">Umdrehung/kWh prüfen</span>
<span id="mqttError" style="display:none">MQTT-Einstellungen prüfen</span>
<span id="mqttAuthError" style="display:none">MQTT-Benutzer/Passwort prüfen</span>
<span id="restartSystem" style="display:none;color:red">System wird neu gestartet...</span>
</div>
</div>

<div style="max-width:335px;margin-top:10px;">
<form method="POST" action="/config" onsubmit="return checkInput();">
  <fieldset><legend><b>&nbsp;Ferraris-Zähler&nbsp;</b></legend>
  <p><b>Umdrehungen pro kWh (__KWH_TURNS_MIN__-__KWH_TURNS_MAX__)</b><br />
  <input id="input_kwh_turns" name="kwh_turns" size="16" maxlength="3" value="__TURNS_KWH__" onkeyup="digitsOnly(this);"></p>
  <p><b>Aktueller Zählerstand (kWh)</b><br />
  <input id="input_consumption_kwh" name="consumption_kwh" size="16" maxlength="9" value="__CONSUMPTION_KWH__" onkeyup="floatsOnly(this);"></p>
  <p><b>Zählernummer (optional)</b><br />
  <input id="input_meter_id" name="meter_id" size="16" maxlength="16" value="__METER_ID__" onkeyup="ASCIIOnly(this);"></p>
  <p><b>Backup Zählerstand (__BACKUP_CYCLE_MIN__-__BACKUP_CYCLE_MAX__ Min.)</b><br />
  <input id="input_backup_cycle" name="backup_cycle" size="16" maxlength="3" value="__BACKUP_CYCLE__" onkeyup="digitsOnly(this);"></p>
  <p><input id="checkbox_power" name="current_power" onclick="togglePower();" type="checkbox" __CURRENT_POWER__><b>Momentanleistung errechnen</b></p>
  <span id="power"><p><input id="checkbox_power_avg" name="current_power_avg" onclick="togglePowerAvg();" type="checkbox" __POWER_AVG__><b>Gleitender Durchschnitt</b></p>
  <span id="power_avg"><p><b>Zeitraum (__POWER_AVG_SECS_MIN__-__POWER_AVG_SECS_MAX__ Sek.)</b><br />
  <input id="input_power_avg_secs" name="power_avg_secs" size="16" maxlength="3" value="__POWER_AVG_SECS__" onkeyup="digitsOnly(this);"></p></span>
  </span>
  </fieldset>
  <br />
  
  <fieldset><legend><b>&nbsp;MQTT&nbsp;</b></legend>
  <p><input id="checkbox_mqtt" name="mqtt" onclick="toggleMQTT();" type="checkbox" __MQTT__><b>Aktivieren</b></p>
  <span id="mqtt" style="display:none">
  <p><b>Adresse des Brokers</b><br />
  <input id="input_mqttbroker" name="mqttbroker" size="16" maxlength="64" value="__MQTT_BROKER__" onkeyup="ASCIIOnly(this);"></p>
  <p><b>Port (Standard 1883)</b><br />
  <input id="input_mqttport" name="mqttport" size="16" maxlength="5" value="__MQTT_PORT__" onkeyup="digitsOnly(this);"></p>
  <p><b>Topic f&uuml;r Nachrichten</b><br />
  <input id="input_mqttbasetopic" name="mqttbasetopic" size="16" maxlength="64" value="__MQTT_BASE_TOPIC__" onkeyup="ASCIIOnly(this);"></p>
  <p><input id="checkbox_mqtt_json" name="mqtt_json" onclick="toggleJSON();"  type="checkbox" __MQTT_JSON__><b>Daten als JSON publizieren</b></p>
  <p><input id="checkbox_mqtt_ha_discovery" name="mqtt_ha_discovery" onclick="toggleHADiscovery();" type="checkbox" __MQTT_HA_DISCOVERY__><b>Home Assistant Discovery</b></p>
  <p><b>Nachrichteninterval (Sek.)</b><br />
  <input id="input_mqttinterval" name="mqttinterval" value="__MQTT_INTERVAL__" maxlength="4" onkeyup="digitsOnly(this);"></p>
  <p><input id="checkbox_powersaving" name="powersavingmode" type="checkbox" onclick="togglePowerSaving(true);" __POWER_SAVING_MODE__><b>Energiesparmodus</b></p>
  <p><input id="checkbox_mqttauth" name="mqttauth" onclick="toggleMQTTAuth();" type="checkbox" __MQTT_AUTH__><b>Authentifizierung aktivieren</b></p>
     <span style="display:none" id="mqttauth">
     <p><b>Benutzername</b><br />
     <input id="input_mqttuser" name="mqttuser" size="16" maxlength="32" value="__MQTT_USERNAME__"></p>
     <p><b>Passwort</b><br />
     <input id="input_mqttpassword" type="password" name="mqttpassword" size="16" maxlength="32" value="__MQTT_PASSWORD__"></p>
     </span>
     <p><input id="checkbox_mqtt_secure" name="mqtt_secure" type="checkbox" onclick="toggleMQTTSecure();" __MQTT_SECURE__><b>Verschlüsselte Verbindung</b></p>
  </span>
  </fieldset>
  <br />

  <p><button class="button bred" type="submit">Einstellungen speichern</button></p>
</form>
<p><button onclick="location.href='/nvsbackup';">Einstellungen exportieren</button></p>
<p><button onclick="location.href='/nvsimport';">Einstellungen importieren</button></p>
<p><button onclick="location.href='/expert';">Experten-Einstellungen</button></p>
<p><button onclick="location.href='/';">Startseite</button></p>
</div>
)=====";


const char UPDATE_html[] PROGMEM = R"=====(
<script>
function initPage() {
  document.getElementById('upload_form').onsubmit = function(e) {
    e.preventDefault();
    var uploadForm = new FormData();
    var firmwareFile = document.getElementById('firmware_file').files[0];
    if (!firmwareFile) {
      alert('Bitte zuerst eine Firmware-Datei *.bin zum Hochladen auswählen.');
      return false;
    }
    uploadForm.append("files", firmwareFile, firmwareFile.name);
    var xhttp = new XMLHttpRequest();
    xhttp.upload.addEventListener("progress", function(e) {
      if (e.lengthComputable) {
        var percent = Math.round((e.loaded/e.total)*100);
        var progress = document.getElementById('progress');
        if (percent == 100) {
            progress.value = percent;
            progress.style.display = "none";
            document.getElementById('install').style.display = "block";
        } else {
            progress.style.display = "block";
            progress.value = percent;
        }
      }
    }, false);
    xhttp.open("POST", "/update", true);
    xhttp.onload = function() {
      if (xhttp.status == 200) {
        location.href='/update?res=ok';
        return true;
      } else {
        location.href='/update?res=err';
        return false;
      }
    };
    xhttp.send(uploadForm);
  }
}
</script>
</head>
<body onload="initPage();">
<div style="text-align:left;display:inline-block;min-width:340px;">
<div style="text-align:center;">
<h2>Firmware-Update</h2>
<progress id="progress" style="display:none;margin: 17px 0px 6px 50px;width:235px" value="0" max="100"></progress>
<div id="install" style="display:none;margin-top:10px;text-align:center;color:red"><strong>Installiere Update...</strong></div>
</div>
<div style="margin-left:30px;margin-bottom:10px">
1. Firmware-Datei *.bin ausw&auml;hlen<br>
2. Aktualisierung starten<br>
3. Upload dauert ca. 20 Sek.<br>
4. System neu starten
</div>
<div>
<form method="POST" action="#" enctype="multipart/form-data" id="upload_form">
  <input id="firmware_file" type="file" accept=".bin" name="update">
  <p><button class="button bred" type="submit">Aktualisierung durchf&uuml;hren</button></p>
</form>
<p><button onclick="location.href='/';">Startseite</button></p>
</div>
)=====";


const char UPDATE_OK_html[] PROGMEM = R"=====(
<script>
var skip = 0

function restartSystem() {
  if (skip)
    return;
  if (confirm("System neu starten?")) {
    var xhttp = new XMLHttpRequest();
    setTimeout(function(){location.href='/';}, 15000);
    document.getElementById("message").style.display = "block";
    document.getElementById("restartSystem").style.display = "block";
    xhttp.open("GET", "restart", true);
    xhttp.send();
  }
}


function resetSystem() {
  if (confirm("Alle Einstellungen zurücksetzen und neu starten?")) {
    var xhttp = new XMLHttpRequest();
    setTimeout(function(){location.href='/';}, 15000);
    document.getElementById("message").style.display = "block";
    document.getElementById("resetSystem").style.display = "block";
    xhttp.open("GET", "reset", true);
    xhttp.send();
    skip = 1;
  }
}
</script>
</head>
<body>
<div style="text-align:left;display:inline-block;min-width:340px;">
<div style="text-align:center;">
<h2 id="heading">Firmware-Update<br />erfolgreich</h2>
<div id="message" style="display:none;margin-top:10px;color:red;font-weight:bold;text-align:center;max-width:335px">
<span id="restartSystem" style="display:none">System wird neu gestartet...<br></span>
<span id="resetSystem" style="display:none">Nach dem Systemreset wird ein WLAN Access Point zur Neukonfiguration gestartet!<br></span>
</div>
</div>
<div>
<p><button class="button bred" onclick="resetSystem(); restartSystem();">System neu starten</button></p>
<p><button onclick="location.href='/';">Startseite</button></p>
</div>
)=====";


const char UPDATE_ERR_html[] PROGMEM = R"=====(
</head>
<body>
<div style="text-align:left;display:inline-block;min-width:340px;">
<div style="text-align:center;">
<h2>Firmware-Update<br />fehlgeschlagen</h2>
</div>
<div>
<p><button onclick="location.href='/update';">Update wiederholen</button></p>
<p><button onclick="location.href='/';">Startseite</button></p>
</div>
)=====";


const char IMPORT_html[] PROGMEM = R"=====(
<script>
function initPage() {
  document.getElementById('upload_form').onsubmit = function(e) {
    e.preventDefault();
    var uploadForm = new FormData();
    var configFile = document.getElementById('json_file').files[0];
    if (!configFile) {
      alert('Bitte zuerst eine Einstellungsdatei (JSON) zum Hochladen auswählen.');
      return false;
    }
    uploadForm.append("files", configFile, configFile.name);
    var xhttp = new XMLHttpRequest();
    xhttp.upload.addEventListener("progress", function(e) {
      if (e.lengthComputable) {
        var percent = Math.round((e.loaded/e.total)*100);
        var progress = document.getElementById('progress');
        if (percent == 100) {
            progress.value = percent;
            progress.style.display = "none";
            document.getElementById('install').style.display = "block";
        } else {
            progress.style.display = "block";
            progress.value = percent;
        }
      }
    }, false);
    xhttp.open("POST", "/nvsimport", true);
    xhttp.onload = function() {
      if (xhttp.status == 200) {
        location.href='/nvsimport?res=ok';
        return true;
      } else {
        location.href='/nvsimport?res=err';
        return false;
      }
    };
    xhttp.send(uploadForm);
  }
}
</script>
</head>
<body onload="initPage();">
<div style="text-align:left;display:inline-block;min-width:340px;">
<div style="text-align:center;">
<h2>Einstellungen importieren</h2>
<progress id="progress" style="display:none;margin: 17px 0px 6px 50px;width:235px" value="0" max="100"></progress>
<div id="install" style="display:none;margin-top:10px;text-align:center;color:red"><strong>Installiere Update...</strong></div>
</div>
<div style="margin-left:40px;margin-bottom:10px">
1. Konfigurationsdatei ausw&auml;hlen<br />
2. Import der Datei starten<br />
3. System neu starten
</div>
<div>
<form method="POST" action="#" enctype="multipart/form-data" id="upload_form">
  <input id="json_file" type="file" accept=".json" name="update">
  <p><button class="button bred" type="submit">Einstellungen hochladen</button></p>
</form>
<p><button onclick="location.href='/';">Startseite</button></p>
</div>
)=====";


const char IMPORT_OK_html[] PROGMEM = R"=====(
<script>
function restartSystem() {
  if (confirm("System neu starten?")) {
    var xhttp = new XMLHttpRequest();
    setTimeout(function(){location.href='/';}, 15000);
    document.getElementById("message").style.display = "block";
    document.getElementById("restartSystem").style.display = "block";
    xhttp.open("GET", "restart", true);
    xhttp.send();
  }
}
</script>
</head>
<body>
<div style="text-align:left;display:inline-block;min-width:340px;">
<div style="text-align:center;">
<h2 id="heading">Einstellung importieren<br />erfolgreich</h2>
<div id="message" style="display:none;margin-top:10px;color:red;font-weight:bold;text-align:center;max-width:335px">
<span id="restartSystem" style="display:none">System wird neu gestartet...<br></span>
</div>
</div>
<div>
<p><button class="button bred" onclick="restartSystem();">System neu starten</button></p>
<p><button onclick="location.href='/';">Startseite</button></p>
</div>
)=====";


const char IMPORT_ERR_html[] PROGMEM = R"=====(
</head>
<body>
<div style="text-align:left;display:inline-block;min-width:340px;">
<div style="text-align:center;">
<h2>Einstellungen importieren<br />fehlgeschlagen</h2>
</div>
<div>
<p><button onclick="location.href='/nvsimport';">Import wiederholen</button></p>
<p><button onclick="location.href='/';">Startseite</button></p>
</div>
)=====";


const char FOOTER_html[] PROGMEM = R"=====(
<div class="footer"><hr/>
<p style="float:left;margin-top:-2px">
	<a href="https://github.com/lrswss/esp8266-wifi-power-meter" title="build on __BUILD__">Firmware __FIRMWARE__
</p>
<p style="float:right;margin-top:-2px">
	<a href="mailto:software@bytebox.org">&copy; 2019-2023 Lars Wessels</a>
</p>
<div style="clear:both;"></div>
</div>
</div>
</body>
</html>
)=====";
