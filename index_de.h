const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="de">
<head>
<meta charset="utf-8">
<meta name="author" content="(c) 2019 Lars Wessels">
<meta name="description" content="https://github.com/lrswss/esp8266-wifi-power-meter/">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no"/>
<title>Wifi Stromz&auml;hler</title>
<style>
body{text-align:center;font-family:verdana;background:white;}
h2{margin-bottom:12px;}
fieldset{background-color:#f2f2f2;}
div,fieldset,input,select{padding:5px;font-size:1em;}
button{border:0;border-radius:0.3rem;background:#009374;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;-webkit-transition-duration:0.4s;transition-duration:0.4s;cursor:pointer;}
button:hover{background:#007364;}
.bred{background:#d43535;}
.bred:hover{background:#931f1f;}
.bdisabled{pointer-events: none;cursor: not-allowed;opacity: 0.65;filter: alpha(opacity=65);}
a{text-decoration:none;}"
</style>
<script>
var readingThreshold = false;
var savedThreshold = true;
var impulseThreshold = 0;

setInterval(function() {
  updateUI();
  getMessage(); 
}, 1000);

setInterval(function() {
  getReadings();
}, 5000);

function updateUI() {
  if (readingThreshold || (!savedThreshold && impulseThreshold > 0)) {
  	document.getElementById("tr2").style.display = "table-row";
	document.getElementById("tr3").style.display = "table-row";
	document.getElementById("tr4").style.display = "table-row";
	document.getElementById("p2").style.display = "none";
	document.getElementById("p3").style.display = "block";
	if (impulseThreshold > 0) {
	  document.getElementById("b3").classList.remove("bdisabled");
	} else {
	  document.getElementById("b3").classList.add("bdisabled");
	}
  } else {
  	document.getElementById("tr2").style.display = "none";
	document.getElementById("tr3").style.display = "none";
	document.getElementById("tr4").style.display = "none";
	document.getElementById("p2").style.display = "block";
	document.getElementById("p3").style.display = "none";
  }
}

function readThreshold() {
  var xhttp = new XMLHttpRequest();
  if (confirm("Aktuellen Schwellwert wirklich löschen?")) {
  	readingThreshold = true;
  	savedThreshold = false;
  	xhttp.open("GET", "readThreshold", true);
  	xhttp.send();
  	setTimeout(getReadings(), 500);
  }
}

function saveThreshold() {
  var xhttp = new XMLHttpRequest();
  savedThreshold = true;
  xhttp.open("GET", "saveThreshold", true);
  xhttp.send();
  setTimeout(getReadings(), 500);
}

function resetCounter() {
  var xhttp = new XMLHttpRequest();
  if (confirm("Zählerstand wirklich löschen?")) {
  	xhttp.open("GET", "resetCounter", true);
  	xhttp.send();
  	setTimeout(getReadings(), 500);
  }
}

function restartSystem() {
  var xhttp = new XMLHttpRequest();
  if (confirm("Wirklich neustarten?")) {
  	xhttp.open("GET", "restart", true);
  	xhttp.send();
  }
}

function getMessage() {
  var xhttp = new XMLHttpRequest();
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      document.getElementById("message").innerHTML = this.responseText;
    }
  };
  xhttp.open("GET", "message", true);
  xhttp.send();
}

function getReadings() {
  var xhttp = new XMLHttpRequest();
  var arr;
  xhttp.onreadystatechange = function() {
    if (this.readyState == 4 && this.status == 200) {
      arr = this.responseText.split(',');
      document.getElementById("TotalRevolutions").innerHTML = arr[0];
	  document.getElementById("TotalConsumption").innerHTML = arr[1];
      document.getElementById("Threshold").innerHTML = (arr[2] > 0 ? arr[2] : "-");
      impulseThreshold = arr[2];
      document.getElementById("Run").innerHTML = arr[3];
      document.getElementById("Build").innerHTML = arr[4];
      readingThreshold = (arr[5] > 0 ? true : false);
	  document.getElementById("CurrentReadings").innerHTML = arr[6];
	  document.getElementById("TotalReadings").innerHTML = arr[7];
      document.getElementById("Min").innerHTML = (arr[8] > 0 ? arr[8] : "-");
      document.getElementById("Max").innerHTML = (arr[9] > 0 ? arr[9] : "-");
	  document.getElementById("Influx").innerHTML = (arr[10] > 0 ? " | InfluxUDP" : "");	  
   	} 
  };
  xhttp.open("GET", "readings", true);
  xhttp.send();
}
</script>
</head>

<body onload="getReadings(); updateUI();">
<div style="text-align:left;display:inline-block;min-width:340px;">
<div style="text-align:center;">
<h2>Stromz&auml;hler</h2>
<div id="message" style="color:red;height:16px;text-align:center;max-width:335px">
<noscript>Bitte JavaScript aktivieren!<br/></noscript>
</div>
</div>
<div id="readings" style="margin-top:5px">
<table style="min-width:340px">
	<tr><th>Anzahl Umdrehungen:</th><td><span id="TotalRevolutions">--</span></td></tr>
	<tr><th>Stromverbrauch:</th><td><span id="TotalConsumption">--</span> kWh</td></tr>
	<tr id="tr2"><th>Messwerte gelesen:</th><td><span id="CurrentReadings">--</span>/<span id="TotalReadings">--</span></td></tr>
	<tr id="tr3"><th>Impulsschwellwert:</th><td><span id="Threshold">--</span></td></tr>
    <tr id="tr4"><th>Min./Max. Impulswert:</th><td><span id="Min">--</span>/<span id="Max">--</span></td></tr>
	<tr><th>Laufzeit:</th><td><span id="Run">--d --h --m</span></td></tr>
</table>
</div>
<div id="buttons" style="margin-top:5px">
<p><button onclick="resetCounter()">Z&auml;hler zur&uuml;cksetzen</button><p>
<p id="p2"><button id="b2" onclick="readThreshold()">Schwellwert einmessen</button><p>
<p id="p3"><button id="b3" onclick="saveThreshold()">Schwellwert speichern</button><p>
<p><button onclick="location.href='/update';">Firmware aktualisieren</button></p>
<p><button class="button bred" onclick="restartSystem()">System neustarten</button></p>
</div>
<div style="text-align:right;font-size:11px;color:#aaa"><hr/>Build <span id="Build">--</span><span id="Influx"></span></div>
</div>
</div>
</body>
</html>
)=====";