const char MAIN_page[] PROGMEM = R"=====(
<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="author" content="(c) 2019-2021 Lars Wessels">
<meta name="description" content="https://github.com/lrswss/esp8266-wifi-power-meter/">
<meta name="viewport" content="width=device-width,initial-scale=1,user-scalable=no"/>
<title>Wifi Power Meter</title>
<style>
body{text-align:center;font-family:verdana;background:white;}
h2{margin-bottom:10px;margin-top:0px}
fieldset{background-color:#f2f2f2;}
div,fieldset,input,select{padding:5px;font-size:1em;}
button{border:0;border-radius:0.3rem;background:#009374;color:#fff;line-height:2.4rem;font-size:1.2rem;width:100%;-webkit-transition-duration:0.4s;transition-duration:0.4s;cursor:pointer;}
button:hover{background:#007364;}
button:focus {outline:0;}
.bred{background:#d43535;}
.bred:hover{background:#931f1f;}
.bdisabled{pointer-events: none;cursor: not-allowed;opacity: 0.65;filter: alpha(opacity=65);}
.footer{font-size:0.6em;color:#aaa;}
a{text-decoration:none;color:inherit;}
p{margin:0.5em 0;}
</style>
<script>
var readingThreshold = false;
var savedThreshold = true;
var impulseThreshold = 0;

setInterval(function() {
  getReadings();
  updateUI();
}, 1000);

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

function calcThreshold() {
  var xhttp = new XMLHttpRequest();
  if (confirm("Delete current threshold value?")) {
  	readingThreshold = true;
  	savedThreshold = false;
    xhttp.open("GET", "calcThreshold", true);
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
  if (confirm("Really reset counter?")) {
  	xhttp.open("GET", "resetCounter", true);
  	xhttp.send();
  	setTimeout(getReadings(), 500);
  }
}

function restartSystem() {
  var xhttp = new XMLHttpRequest();
  if (confirm("Are you to restart the system?")) {
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
      document.getElementById("Message").innerHTML = json.message;
      document.getElementById("TotalCounter").innerHTML = json.totalCounter;
      document.getElementById("TotalConsumption").innerHTML = json.totalConsumption;
      document.getElementById("ImpulseThreshold").innerHTML = (json.impulseThreshold > 0 ? json.impulseThreshold : "-");
      impulseThreshold = json.impulseThreshold;
      document.getElementById("Runtime").innerHTML = json.runtime;
      readingThreshold = (json.readingThreshold > 0 ? true : false);
      document.getElementById("CurrentReadings").innerHTML = json.currentReadings;
      document.getElementById("TotalReadings").innerHTML = json.totalReadings;
      document.getElementById("ImpulseMin").innerHTML = (json.impulseMin > 0 ? json.impulseMin : "-");
      document.getElementById("ImpulseMax").innerHTML = (json.impulseMax > 0 ? json.impulseMax : "-");
      document.getElementById("InfluxEnabled").innerHTML = (json.influxEnabled > 0 ? " | InfluxUDP" : "");
      document.getElementById("Version").innerHTML = json.version;
      document.getElementById("GitHub").setAttribute("title", json.build);
    }
  };
  xhttp.open("GET", "readings?local=true", true);
  xhttp.send();
}
</script>
</head>

<body onload="getReadings(); updateUI();">
<div style="text-align:left;display:inline-block;min-width:340px;">
<div style="text-align:center;">
<h2>Ferraris Power Meter</h2>
<div id="Message" style="color:red;height:16px;text-align:center;max-width:335px">
<noscript>Activate JavaScript!<br/></noscript>
</div>
</div>
<div id="readings" style="margin-top:2px">
<table style="min-width:340px">
	<tr><th>Total revolutions:</th><td><span id="TotalCounter">--</span></td></tr>
	<tr><th>Total consumption:</th><td><span id="TotalConsumption">--</span> kWh</td></tr>
	<tr id="tr2"><th>Readings saved:</th><td><span id="CurrentReadings">--</span>/<span id="TotalReadings">--</span></td></tr>
  <tr id="tr4"><th>Minimum/Maximum:</th><td><span id="ImpulseMin">--</span>/<span id="ImpulseMax">--</span></td></tr>
	<tr id="tr3"><th>Threshold value:</th><td><span id="ImpulseThreshold">--</span></td></tr>
	<tr><th>Runtime:</th><td><span id="Runtime">--d --h --m</span></td></tr>
</table>
</div>
<div id="buttons" style="margin-top:5px">
<p><button onclick="resetCounter()">Reset Counter</button><p>
<p id="p2"><button id="b2" onclick="calcThreshold()">Calculate Threshold</button><p>
<p id="p3"><button id="b3" onclick="saveThreshold()">Save Threshold</button><p>
<p><button onclick="location.href='/update';">Update Firmware</button></p>
<p><button class="button bred" onclick="restartSystem()">Restart System</button></p>
</div>
<div class="footer"><hr/>
<p style="float:left;margin-top:-2px">
  <a href="https://github.com/lrswss/esp8266-wifi-power-meter" id="GitHub" title=""><span id="Version"></span></a><span id="InfluxEnabled"></span>
</p>
<p style="float:right;margin-top:-2px">
  <a href="mailto:software@bytebox.org">&copy; 2019-2021 Lars Wessels</a>
</p>
<div style="clear:both;"></div>
</div>
</div>
</body>
</html>
)=====";
