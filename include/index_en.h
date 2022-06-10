const char HEADER_html[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="author" content="(c) 2019-2022 Lars Wessels">
<meta name="description" content="https://github.com/lrswss/esp8266-wifi-power-meter/">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no"/>
<title>Ferraris Meter __SYSTEMID__</title>
<style>
body{text-align:center;font-family:verdana;background:white;}
div,fieldset,input,select{padding:5px;font-size:1em;}
h2{margin-top:2px;margin-bottom:8px}
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
}

function hideMessages() {
  document.getElementById("message").style.display = "none";
  document.getElementById("invalidThreshold").style.display = "none";
  document.getElementById("thresholdCalculation").style.display = "none";
  document.getElementById("thresholdReset").style.display = "none";
  document.getElementById("thresholdSaved").style.display = "none";
  document.getElementById("thresholdFound").style.display = "none";
  document.getElementById("thresholdFailed").style.display = "none";
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
      document.getElementById("TotalConsumption").innerHTML = (json.totalConsumption > 10 ? json.totalConsumption : "--");
      document.getElementById("Runtime").innerHTML = json.runtime;
      document.getElementById("CurrentReadings").innerHTML = json.currentReadings;
      document.getElementById("TotalReadings").innerHTML = json.totalReadings;
      document.getElementById("PulseMin").innerHTML = (json.pulseMin > 0 ? json.pulseMin : "--");
      document.getElementById("PulseMax").innerHTML = (json.pulseMax > 0 ? json.pulseMax : "--");
      document.getElementById("PulseThreshold").innerHTML = (json.pulseThreshold > 0 ? json.pulseThreshold : "--");
      pulseThreshold = json.pulseThreshold;
      thresholdCalculation = json.thresholdCalculation;
      totalCounter = json.totalCounter;
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
<span id="invalidThreshold" style="display:none">Invalid threshold value!</span>
<span id="thresholdCalculation" style="display:none">Calculating threshold, please wait!</span>
<span id="thresholdFound" style="display:none;color:green">Threshold calculation successful!</span>
<span id="thresholdReset" style="display:none;color:green">Counter reset successful!</span>
<span id="thresholdSaved" style="display:none;color:green">Pulse threshold set successfully!</span>
<span id="thresholdFailed" style="display:none">Readings not sufficient!</span>
<span id="publishFailed" style="display:none">Failed to publish data via MQTT!</span>
<span id="restartSystem" style="display:none">System will restart shortly...</span>
<span id="publishData" style="display:none">Published data via MQTT</span>
</div>
<noscript>Please enable JavaScript!<br/></noscript>
</div>
<div id="readings" style="margin-top:5px;">
<table style="min-width:340px">
	<tr><th>Total revolutions:</th><td><span id="TotalCounter">--</span></td></tr>
	<tr><th>Total consumption:</th><td><span id="TotalConsumption">--</span> kWh</td></tr>
	<tr id="tr3"><th>A/D readings saved:</th><td><span id="CurrentReadings">--</span>/<span id="TotalReadings">--</span></td></tr>
    <tr id="tr4"><th>Minimum/Maximum:</th><td><span id="PulseMin">--</span>/<span id="PulseMax">--</span></td></tr>
	<tr id="tr5"><th>Threshold value:</th><td><span id="PulseThreshold">--</span></td></tr>
	<tr><th>Runtime:</th><td><span id="Runtime">-d -h -m</span></td></tr>
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
  <p><b>Threshold for counter (10-1023)</b><br />
  <input id="input_pulse_threshold" name="pulse_threshold" size="16" maxlength="4" value="__IMPULS_THRESHOLD__" onkeyup="digitsOnly(this);"></p>
   <p><b>Variance für detection (4-40)</b><br />
  <input id="input_readings_spread" name="readings_spread" size="16" maxlength="2" value="__READINGS_SPREAD__" onkeyup="digitsOnly(this);"></p>
  <p><b>Sample rate sensor (50-200 ms)</b><br />
  <input id="input_readings_interval" name="readings_interval" size="16" maxlength="3" value="__READINGS_INTERVAL__" onkeyup="digitsOnly(this);"></p>
  <p><b>Sensor ring buffer (30-120 Sec.)</b><br />
  <input id="input_readings_buffer" name="readings_buffer" size="16" maxlength="3" value="__READINGS_BUFFER__" onkeyup="digitsOnly(this);"></p>
  <p><b>Pulses to increase counter (3-10)</b><br />
  <input id="input_threshold_tigger" name="threshold_trigger" size="16" maxlength="2" value="__THRESHOLD_TRIGGER__" onkeyup="digitsOnly(this);"></p>
  <p><b>Dead time counter (500-3000 ms)</b><br />
  <input id="input_debounce_time" name="debounce_time" size="16" maxlength="4" value="__DEBOUNCE_TIME__" onkeyup="digitsOnly(this);"></p>
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
    Number(document.getElementById("input_kwh_turns").value) > 500) {
    document.getElementById("kwhError").style.display = "block";
    err++;
  }

  if (document.getElementById("input_consumption_kwh").value.length < 3 ||
    Number(document.getElementById("input_consumption_kwh").value) > 999999) {
    document.getElementById("counterError").style.display = "block";
    err++;
  }

  if (document.getElementById("checkbox_mqtt").checked == true && 
    (document.getElementById("input_mqttbroker").length < 4 || 
     document.getElementById("input_mqttbasetopic").length < 4)) {
    document.getElementById("mqttError").style.display = "block";
    err++;
  }

  if (document.getElementById("checkbox_mqttauth").checked == true && 
    (document.getElementById("input_mqttuser").length < 4 || 
     document.getElementById("input_mqttpassword").length < 4)) {
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

</script>
</head>
<body onload="configSaved(); toggleMQTT(); toggleMQTTAuth();">
<div style="text-align:left;display:inline-block;min-width:340px;">
<div style="text-align:center;">
<h2 id="heading">Settings</h2>
<div id="message" style="display:none;margin-top:10px;color:red;font-weight:bold;text-align:center;max-width:335px">
<span id="configSaved" style="display:none;color:green">Saved setttings</span>
<span id="counterError" style="display:none">Check counter reading!</span>
<span id="kwhError" style="display:none">Check value for KWh/revolution!</span>
<span id="mqttError" style="display:none">Check MQTT setting!</span>
<span id="mqttAuthError" style="display:none">Check MQTT username/password!</span>
<span id="restartSystem" style="display:none;color:red">System will restart shortly...</span>
</div>
</div>

<div style="max-width:335px;margin-top:10px;">
<form method="POST" action="/config" onsubmit="return checkInput();">
  <fieldset><legend><b>&nbsp;Ferraris Meter&nbsp;</b></legend>
  <p><b>Turns per KWh (50-500)</b><br />
  <input id="input_kwh_turns" name="kwh_turns" size="16" maxlength="3" value="__TURNS_KWH__" onkeyup="digitsOnly(this);"></p>
  <p><b>Current counter reading (KWh)</b><br />
  <input id="input_consumption_kwh" name="consumption_kwh" size="16" maxlength="9" value="__CONSUMPTION_KWH__" onkeyup="floatsOnly(this);"></p>
  <p><b>Auto-backup counter (60-180 Min.)</b><br />
  <input id="input_backup_cycle" name="backup_cycle" size="16" maxlength="3" value="__BACKUP_CYCLE__" onkeyup="digitsOnly(this);"></p>
  </fieldset>
  <br />
  
  <fieldset><legend><b>&nbsp;MQTT&nbsp;</b></legend>
  <p><input id="checkbox_mqtt" name="mqtt" onclick="toggleMQTT();" type="checkbox" __MQTT__><b>Enable</b></p>
  <span id="mqtt" style="display:none">
  <p><b>Hostname broker</b><br />
  <input id="input_mqttbroker" name="mqttbroker" size="16" maxlength="64" value="__MQTT_BROKER__" onkeyup="ASCIIOnly(this);"></p>
  <p><b>Message topic</b><br />
  <input id="input_mqttbasetopic" name="mqttbasetopic" size="16" maxlength="64" value="__MQTT_BASE_TOPIC__" onkeyup="ASCIIOnly(this);"></p>
  <p><b>Publishing interval (Sec.)</b><br />
  <input name="mqttinterval" value="__MQTT_INTERVAL__" maxlength="4" onkeyup="digitsOnly(this);"></p>
  <p><input id="checkbox_mqttauth" name="mqttauth" onclick="toggleMQTTAuth();" type="checkbox" __MQTT_AUTH__><b>Enable authentication</b></p>
     <span style="display:none" id="mqttauth">
     <p><b>Username</b><br />
     <input id="input_mqttuser" name="mqttuser" size="16" maxlength="32" value="__MQTT_USERNAME__"></p>
     <p><b>Password</b><br />
     <input id="input_mqttpassword" type="password" name="mqttpassword" size="16" maxlength="32" value="__MQTT_PASSWORD__"></p>
     </span>
  </span>
  </fieldset>
  <br />

  <p><button class="button bred" type="submit">Save settings</button></p>
</form>
<p><button onclick="location.href='/expert';">Expert settings</button></p>
<p><button onclick="location.href='/';">Main page</button></p>
</div>
)=====";


const char UPDATE_html[] PROGMEM = R"=====(
</head>
<body>
<div style="text-align:left;display:inline-block;min-width:340px;">
<div style="text-align:center;">
<h2>Firmware update</h2>
</div>
<div style="margin-left:50px;margin-bottom:10px">
1. Select firmware file<br>
2. Start update<br>
3. Upload takes about 20 Sec.<br>
4. Restart system
</div>
<div>
<form method="POST" action="/update" enctype="multipart/form-data" id="upload_form">
  <input type="file" name="update">
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


const char FOOTER_html[] PROGMEM = R"=====(
<div class="footer"><hr/>
<p style="float:left;margin-top:-2px">
	<a href="https://github.com/lrswss/esp8266-wifi-power-meter" title="build on __BUILD__">Firmware __FIRMWARE__
</p>
<p style="float:right;margin-top:-2px">
	<a href="mailto:software@bytebox.org">&copy; 2019-2022 Lars Wessels</a>
</p>
<div style="clear:both;"></div>
</div>
</div>
</body>
</html>
)=====";