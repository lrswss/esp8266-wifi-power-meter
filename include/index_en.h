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
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="author" content="(c) 2019-2023 Lars Wessels">
<meta name="description" content="https://github.com/lrswss/esp8266-wifi-power-meter/">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no"/>
<title>Ferraris Meter __SYSTEMID__</title>
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
  if (confirm("Really reset impuls threshold?")) {
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
  if (confirm("Really reset all counters?")) {
    xhttp.open("GET", "resetCounter", true);
  	xhttp.send();
    showMessage("thresholdReset", 3000);
  }
}

function restartSystem() {
  var xhttp = new XMLHttpRequest();
  if (confirm("Really restart system?")) {
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
<h2 id="heading">Ferraris Meter __SYSTEMID__</h2>
<div id="message" style="display:none;margin-top:10px;color:red;text-align:center;font-weight:bold;max-width:335px">
<span id="invalidThreshold" style="display:none">Invalid threshold value</span>
<span id="thresholdCalculation" style="display:none">Calculating threshold, please wait</span>
<span id="thresholdFound" style="display:none;color:green">Threshold calculation successful</span>
<span id="thresholdReset" style="display:none;color:green">Counter reset successful</span>
<span id="thresholdSaved" style="display:none;color:green">Pulse threshold set successfully</span>
<span id="thresholdFailed" style="display:none">Readings not sufficient</span>
<span id="mqttConnFailed" style="display:none">MQTT connection failed</span>
<span id="publishFailed" style="display:none">Failed to publish data via MQTT</span>
<span id="restartSystem" style="display:none">System will restart shortly...</span>
<span id="publishData" style="display:none;color:green">Published data via MQTT</span>
</div>
<noscript>Please enable JavaScript!<br/></noscript>
</div>
<div id="readings" style="margin-top:5px;">
<table style="min-width:340px">
	<tr><th>Total rotations:</th><td><span id="TotalCounter">--</span></td></tr>
	<tr id="tr1"><th>Current Consumption:</th><td><span id="CurrentPower">--</span> W</td></tr>
	<tr><th>Total consumption:</th><td><span id="TotalConsumption">--</span> kWh</td></tr>
	<tr id="tr3"><th>A/D readings saved:</th><td><span id="CurrentReadings">--</span>/<span id="TotalReadings">--</span></td></tr>
    <tr id="tr4"><th>Minimum/Maximum:</th><td><span id="PulseMin">--</span>/<span id="PulseMax">--</span></td></tr>
	<tr id="tr5"><th>Threshold value:</th><td><span id="PulseThreshold">--</span></td></tr>
	<tr><th>Runtime:</th><td><span id="Runtime">-d -h -m</span></td></tr>
    <tr><th>WiFi RSSI:</th><td><span id="RSSI">--</span> dBm</td></tr>
</table>
</div>
<div id="buttons" style="margin-top:5px">
<p id="resetCounter"><button onclick="resetCounter()">Reset Counter</button><p>
<p id="calcThreshold"><button onclick="calcThreshold()">Calculate threshold</button><p>
<p id="saveThreshold"><button id="btnSaveThreshold" onclick="saveThreshold()">Save Threshold</button><p>
<p><button onclick="location.href='/config';">Settings</button></p>
<p><button onclick="location.href='/update';">Update firmware</button></p>
<p><button class="button bred" onclick="restartSystem()">Restart system</button></p>
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
  if (confirm("Reset settings and restart system?")) {
    var xhttp = new XMLHttpRequest();
    setTimeout(function(){location.href='/';}, 15000);
    showMessage("resetSystem", 15000);
    xhttp.open("GET", "reset", true);
    xhttp.send();
  }
}

function confirmRestart() {
    if (confirm("Save settings and restart system?"))
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
<h2 id="heading">Expert Settings</h2>
<div id="message" style="display:none;margin-top:10px;color:red;font-weight:bold;text-align:center;max-width:335px">
<span id="configSaved" style="display:none;color:green">Settings saved</span>
<span id="restartSystem" style="display:none;color:red">System will restart shortly...</span>
<span id="resetSystem" style="display:none">After system reset connect to local access point to configure system!<br></span>
</div>
</div>

<div style="max-width:335px;margin-top:10px;">
<form method="POST" action="/expert" onsubmit="return confirmRestart();">
  <fieldset><legend><b>&nbsp;Ferraris marker scanner&nbsp;</b></legend>
  <p><b>Threshold for counter (__PULSE_THRESHOLD_MIN__-__PULSE_THRESHOLD_MAX__)</b><br />
  <input id="input_pulse_threshold" name="pulse_threshold" size="16" maxlength="4" value="__PULSE_THRESHOLD__" onkeyup="digitsOnly(this);"></p>
   <p><b>Variance für detection (__READINGS_SPREAD_MIN__-__READINGS_SPREAD_MAX__)</b><br />
  <input id="input_readings_spread" name="readings_spread" size="16" maxlength="2" value="__READINGS_SPREAD__" onkeyup="digitsOnly(this);"></p>
  <p><b>Sample rate sensor (__READINGS_INTERVAL_MS_MIN__-__READINGS_INTERVAL_MS_MAX__ ms)</b><br />
  <input id="input_readings_interval" name="readings_interval" size="16" maxlength="3" value="__READINGS_INTERVAL_MS__" onkeyup="digitsOnly(this);"></p>
  <p><b>Sensor ring buffer (__READINGS_BUFFER_SECS_MIN__-__READINGS_BUFFER_SECS_MAX__ sec.)</b><br />
  <input id="input_readings_buffer" name="readings_buffer" size="16" maxlength="3" value="__READINGS_BUFFER_SECS__" onkeyup="digitsOnly(this);"></p>
  <p><b>Pulses to increase counter (__THRESHOLD_TRIGGER_MIN__-__THRESHOLD_TRIGGER_MAX__)</b><br />
  <input id="input_threshold_tigger" name="threshold_trigger" size="16" maxlength="2" value="__THRESHOLD_TRIGGER__" onkeyup="digitsOnly(this);"></p>
  <p><b>Dead time counter (__DEBOUNCE_TIME_MS_MIN__-__DEBOUNCE_TIME_MS_MAX__ ms)</b><br />
  <input id="input_debounce_time" name="debounce_time" size="16" maxlength="4" value="__DEBOUNCE_TIME_MS__" onkeyup="digitsOnly(this);"></p>
  </fieldset>
  <br />

  <fieldset><legend><b>&nbsp;InfluxDB&nbsp;</b></legend>
  <p><input id="checkbox_influxdb" name="influxdb" type="checkbox" __INFLUXDB__><b>Stream raw sensor data</b></p>
  </fieldset>

  <br />
  <p><button class="button bred" type="submit">Save and restart</button></p>
</form>
<p><button onclick="resetSystem();">Reset system settings</button></p>
<p><button onclick="location.href='/config';">Main settings</button></p>
<p><button onclick="location.href='/';">Main page</button></p>
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
      alert('WARNING: The web interface is only accessible for 5 minutes after activating '+
        'the power saving mode. The Wifi connection is switched off and then only activated '+
        'repeatedly for a few seconds to publish data via MQTT. To regain access to the web '+
        'interface you need to restart the Wifi Power Meter or send a MQTT message with retain '+
        'flag set (__MQTT_BASE_TOPIC__/__SYSTEMID__/cmd/powersave 0) to the device. Moving '+
        'average for current power reading is automatically enabled.');
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
<h2 id="heading">Settings</h2>
<div id="message" style="display:none;margin-top:10px;color:red;font-weight:bold;text-align:center;max-width:335px">
<span id="configSaved" style="display:none;color:green">Saved setttings</span>
<span id="counterError" style="display:none">Check counter reading</span>
<span id="kwhError" style="display:none">Check value for rotations/kWh</span>
<span id="mqttError" style="display:none">Check MQTT settings</span>
<span id="mqttAuthError" style="display:none">Check MQTT username/password</span>
<span id="restartSystem" style="display:none;color:red">System will restart shortly...</span>
</div>
</div>

<div style="max-width:335px;margin-top:10px;">
<form method="POST" action="/config" onsubmit="return checkInput();">
  <fieldset><legend><b>&nbsp;Ferraris Meter&nbsp;</b></legend>
  <p><b>Rotations per kWh (__KWH_TURNS_MIN__-__KWH_TURNS_MAX__)</b><br />
  <input id="input_kwh_turns" name="kwh_turns" size="16" maxlength="3" value="__TURNS_KWH__" onkeyup="digitsOnly(this);"></p>
  <p><b>Current meter reading (kWh)</b><br />
  <input id="input_consumption_kwh" name="consumption_kwh" size="16" maxlength="9" value="__CONSUMPTION_KWH__" onkeyup="floatsOnly(this);"></p>
  <p><b>Power meter ID (optional)</b><br />
  <input id="input_meter_id" name="meter_id" size="16" maxlength="16" value="__METER_ID__" onkeyup="ASCIIOnly(this);"></p>
  <p><b>Auto-backup counter (__BACKUP_CYCLE_MIN__-__BACKUP_CYCLE_MAX__ min.)</b><br />
  <input id="input_backup_cycle" name="backup_cycle" size="16" maxlength="3" value="__BACKUP_CYCLE__" onkeyup="digitsOnly(this);"></p>
  <p><input id="checkbox_power" name="current_power" onclick="togglePower();" type="checkbox" __CURRENT_POWER__><b>Calculate current consumption</b></p>
  <span id="power"><p><input id="checkbox_power_avg" name="current_power_avg" onclick="togglePowerAvg();" type="checkbox" __POWER_AVG__><b>Enable moving average</b></p>
  <span id="power_avg"><p><b>Averaging interval (__POWER_AVG_SECS_MIN__-__POWER_AVG_SECS_MAX__ sec.)</b><br />
  <input id="input_power_avg_secs" name="power_avg_secs" size="16" maxlength="3" value="__POWER_AVG_SECS__" onkeyup="digitsOnly(this);"></p></span>
  </span>
  </fieldset>
  <br />
  
  <fieldset><legend><b>&nbsp;MQTT&nbsp;</b></legend>
  <p><input id="checkbox_mqtt" name="mqtt" onclick="toggleMQTT();" type="checkbox" __MQTT__><b>Enable</b></p>
  <span id="mqtt" style="display:none">
  <p><b>Hostname broker</b><br />
  <input id="input_mqttbroker" name="mqttbroker" size="16" maxlength="64" value="__MQTT_BROKER__" onkeyup="ASCIIOnly(this);"></p>
  <p><b>Port (default 1883)</b><br />
  <input id="input_mqttport" name="mqttport" size="16" maxlength="5" value="__MQTT_PORT__" onkeyup="digitsOnly(this);"></p>
  <p><b>Message topic</b><br />
  <input id="input_mqttbasetopic" name="mqttbasetopic" size="16" maxlength="64" value="__MQTT_BASE_TOPIC__" onkeyup="ASCIIOnly(this);"></p>
  <p><input id="checkbox_mqtt_json" name="mqtt_json" onclick="toggleJSON();" type="checkbox" __MQTT_JSON__><b>Publish data as JSON</b></p>
  <p><input id="checkbox_mqtt_ha_discovery" name="mqtt_ha_discovery" onclick="toggleHADiscovery();" type="checkbox" __MQTT_HA_DISCOVERY__><b>Home Assistant Discovery</b></p>
  <p><b>Publishing interval (sec.)</b><br />
  <input id="input_mqttinterval" name="mqttinterval" value="__MQTT_INTERVAL__" maxlength="4" onkeyup="digitsOnly(this);"></p>
  <p><input id="checkbox_powersaving" name="powersavingmode" type="checkbox" onclick="togglePowerSaving(true);" __POWER_SAVING_MODE__><b>Power saving mode</b></p>
  <p><input id="checkbox_mqttauth" name="mqttauth" onclick="toggleMQTTAuth();" type="checkbox" __MQTT_AUTH__><b>Enable authentication</b></p>
     <span style="display:none" id="mqttauth">
     <p><b>Username</b><br />
     <input id="input_mqttuser" name="mqttuser" size="16" maxlength="32" value="__MQTT_USERNAME__"></p>
     <p><b>Password</b><br />
     <input id="input_mqttpassword" type="password" name="mqttpassword" size="16" maxlength="32" value="__MQTT_PASSWORD__"></p>
     </span>
     <p><input id="checkbox_mqtt_secure" name="mqtt_secure" type="checkbox" onclick="toggleMQTTSecure();" __MQTT_SECURE__><b>Enable TLS</b></p>
    </span>
  </fieldset>
  <br />

  <p><button class="button bred" type="submit">Save settings</button></p>
</form>
<p><button onclick="location.href='/nvsbackup';">Backup settings</button></p>
<p><button onclick="location.href='/nvsimport';">Import settings</button></p>
<p><button onclick="location.href='/expert';">Expert settings</button></p>
<p><button onclick="location.href='/';">Main page</button></p>
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
      alert('Please select a firmware file *.bin for upload.');
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
<h2>Firmware update</h2>
<progress id="progress" style="display:none;margin: 17px 0px 6px 50px;width:235px" value="0" max="100"></progress>
<div id="install" style="display:none;margin-top:10px;text-align:center;color:red"><strong>Installing update...</strong></div>
</div>
<div style="margin-left:40px;margin-bottom:10px">
1. Select firmware file *.bin<br>
2. Start update<br>
3. Upload takes about 20 sec.<br>
4. Restart system
</div>
<div>
<form method="POST" action="#" enctype="multipart/form-data" id="upload_form">
  <input id="firmware_file" type="file" accept=".bin" name="update">
  <p><button class="button bred" type="submit">Start update</button></p>
</form>
<p><button onclick="location.href='/';">Main page</button></p>
</div>
)=====";


const char UPDATE_OK_html[] PROGMEM = R"=====(
<script>
var skip = 0

function restartSystem() {
  if (skip)
    return;
  if (confirm("Really restart system?")) {
    var xhttp = new XMLHttpRequest();
    setTimeout(function(){location.href='/';}, 15000);
    document.getElementById("message").style.display = "block";
    document.getElementById("restartSystem").style.display = "block";
    xhttp.open("GET", "restart", true);
    xhttp.send();
  }
}


function resetSystem() {
  if (confirm("Reset all settings and restart system?")) {
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
<h2 id="heading">Firmware update<br />successful</h2>
<div id="message" style="display:none;margin-top:10px;color:red;font-weight:bold;text-align:center;max-width:335px">
<span id="restartSystem" style="display:none">System will restart shortly...<br></span>
<span id="resetSystem" style="display:none">After system reset connect to local access point to configure system!<br></span>
</div>
</div>
<div>
<p><button class="button bred" onclick="resetSystem(); restartSystem();">Restart system</button></p>
<p><button onclick="location.href='/';">Main page</button></p>
</div>
)=====";


const char UPDATE_ERR_html[] PROGMEM = R"=====(
</head>
<body>
<div style="text-align:left;display:inline-block;min-width:340px;">
<div style="text-align:center;">
<h2>Firmware update<br />failed</h2>
</div>
<div>
<p><button onclick="location.href='/update';">Repeat update</button></p>
<p><button onclick="location.href='/';">Main page</button></p>
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
      alert('Please select a JSON configuration file for upload.');
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
<h2>Import system settings</h2>
<progress id="progress" style="display:none;margin: 17px 0px 6px 50px;width:235px" value="0" max="100"></progress>
<div id="install" style="display:none;margin-top:10px;text-align:center;color:red"><strong>Importing settings...</strong></div>
</div>
<div style="margin-left:50px;margin-bottom:10px">
1. Select JSON configuration file<br />
2. Start upload<br />
3. Restart system
</div>
<div>
<form method="POST" action="#" enctype="multipart/form-data" id="upload_form">
  <input id="json_file" type="file" accept=".json" name="update">
  <p><button class="button bred" type="submit">Upload settings</button></p>
</form>
<p><button onclick="location.href='/';">Main page</button></p>
</div>
)=====";


const char IMPORT_OK_html[] PROGMEM = R"=====(
<script>
function restartSystem() {
  if (confirm("Really restart system?")) {
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
<h2 id="heading">Settings import<br />successful</h2>
<div id="message" style="display:none;margin-top:10px;color:red;font-weight:bold;text-align:center;max-width:335px">
<span id="restartSystem" style="display:none">System will restart shortly...<br></span>
</div>
</div>
<div>
<p><button class="button bred" onclick="restartSystem();">Restart system</button></p>
<p><button onclick="location.href='/';">Main Page</button></p>
</div>
)=====";


const char IMPORT_ERR_html[] PROGMEM = R"=====(
</head>
<body>
<div style="text-align:left;display:inline-block;min-width:340px;">
<div style="text-align:center;">
<h2>Settings import<br />failed</h2>
</div>
<div>
<p><button onclick="location.href='/nvsimport';">Repeat import</button></p>
<p><button onclick="location.href='/';">Main Page</button></p>
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
