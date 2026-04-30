#include "WebHandlers.hpp"
#include <ESP8266WiFi.h>
#include <Arduino.h>
#include "platform.hpp"

  
void handleStylesheet() {
  ESP8266WebServer& localServer = getWebServer();
  localServer.send(200, "text/css", R"rawliteral(
:root {
  --bg: #f5f5f5;
  --card: #ffffff;
  --accent: #2d7cff;
  --danger: #c0392b;
  --border: #ddd;
}

body {
  font-family: Arial, sans-serif;
  margin: 0;
  background: var(--bg);
}

header {
  padding: 16px;
  background: var(--accent);
  color: white;
  text-align: center;
  font-size: 20px;
}

.card {
  background: var(--card);
  border-radius: 8px;
  padding: 12px;
  margin: 12px;
  box-shadow: 0 2px 5px rgba(0,0,0,0.08);
}

button {
  background: var(--accent);
  color: white;
  border: none;
  border-radius: 6px;
  padding: 8px;
}

button.danger {
  background: var(--danger);
}

.badge-on { color: green; font-weight: bold; }
.badge-off { color: red; font-weight: bold; }
)rawliteral");
}

void handleDashboard() {
 ESP8266WebServer& localServer = getWebServer();
   localServer.send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Greenhouse Status</title>
<link rel="stylesheet" href="/style.css">
</head>

<body>
<header>Greenhouse Controller</header>

<div class="card">
  <div class="row"><span>Status</span><span id="status">—</span></div>
  <div class="row"><span>Output</span><span id="output">—</span></div>
  <div class="row"><span>Schedules</span><span id="slots">—</span></div>
  <div class="row"><span>Time</span><span id="time">—</span></div>
  <div class="row"><span>Uptime</span><span id="uptime">—</span></div>
  <div class="row"><span>IP</span><span id="ip">—</span></div>
</div>

<div class="card">
  <a href="/">Dashboard</a><br>
  <a href="/schedule">Schedule</a><br>
  <a href="/manual">Manual</a>
</div>

<script>
function msToTime(ms) {
  const s = Math.floor(ms/1000);
  const h = Math.floor(s/3600);
  const m = Math.floor((s%3600)/60);
  const sec = s%60;
  return `${h}:${String(m).padStart(2,'0')}:${String(sec).padStart(2,'0')}`;
}

function refresh() {
  fetch('/api/status')
    .then(r => r.json())
    .then(s => {
      status.textContent = s.status.toUpperCase();
      ip.textContent = s.ip;
      time.textContent = s.time;
      slots.textContent = s.scheduleCount;
      uptime.textContent = msToTime(s.uptime_ms);

      output.innerHTML =
        s.controlpin === "ON"
          ? '<span class="badge-on">ON</span>'
          : '<span class="badge-off">OFF</span>';
    });
}

refresh();
setInterval(refresh, 5000);
</script>
</body>
</html>
)rawliteral");
}

void handleManualPage() {
  ESP8266WebServer& localServer = getWebServer();
  localServer.send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Manual Control</title>
<link rel="stylesheet" href="/style.css">
</head>

<body>
<header>Manual Control</header>

<div class="card">
  <label>Action</label>
  <select id="action">
    <option value="on">ON</option>
    <option value="off">OFF</option>
  </select>

  <label>Duration (minutes)</label>
  <input id="duration" type="number" value="10">

  <button onclick="apply()">Apply</button>
</div>

<div class="card">
  <a href="/">Dashboard</a><br>
  <a href="/schedule">Schedule</a><br>
  <a href="/manual">Manual</a>
</div>

<script>
function apply() {
  fetch(`/api/oneTime?state=${action.value}&duration=${duration.value}`);
}
</script>
</body>
</html>
)rawliteral");
}

void handleSchedulePage() {
ESP8266WebServer& localServer = getWebServer();
    localServer.send(200, "text/html", R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Irrigation Scheduler</title>
<link rel="stylesheet" href="/style.css">
</head>

<body>

<header>Irrigation Scheduler</header>

<section>
<div class="card">
<h3>Add schedule</h3>

<label>Time</label>
<div class="schedule-row">
  <input id="hour" type="number" placeholder="HH">
  <input id="minute" type="number" placeholder="MM">
</div>

<label>Action</label>
<select id="action">
  <option value="on">ON</option>
  <option value="off">OFF</option>
</select>

<label>Start (month / day)</label>
<div class="schedule-row">
  <input id="sm" type="number">
  <input id="sd" type="number">
</div>

<label>End (month / day)</label>
<div class="schedule-row">
  <input id="em" type="number">
  <input id="ed" type="number">
</div>

<button onclick="addSlot()">Add</button>
</div>
</section>

<section>
<h3 style="padding-left:16px;">Schedule</h3>
<div id="schedule"></div>
</section>

<div class="card">
  <a href="/">Dashboard</a><br>
  <a href="/schedule">Schedule</a><br>
  <a href="/manual">Manual</a>
</div>

<script>
function load() {
  fetch('/api/schedule')
    .then(r => r.json())
    .then(data => {
      const c = document.getElementById('schedule');
      c.innerHTML = '';

      data.forEach(s => {
        c.innerHTML += `
          <div class="schedule-item">
            <div class="schedule-row">
              <strong>${s.hour}:${String(s.minute).padStart(2,'0')}</strong>
              <input class="toggle" type="checkbox"
                     ${s.active ? 'checked' : ''}
                     onchange="toggleActive(${s.id}, this.checked)">
            </div>

            <div class="schedule-meta">
              ${s.action.toUpperCase()}<br>
              ${s.startMonth}/${s.startDay} → ${s.endMonth}/${s.endDay}
            </div>

            <button class="danger" onclick="deleteSlot(${s.id})">
              Delete
            </button>
          </div>`;
      });
    });
}

function addSlot() {
  fetch('/api/slot', {
    method: 'POST',
    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
    body:
      `hour=${hour.value}&minute=${minute.value}` +
      `&action=${action.value}` +
      `&startMonth=${sm.value}&startDay=${sd.value}` +
      `&endMonth=${em.value}&endDay=${ed.value}`
  }).then(load);
}

function deleteSlot(id) {
  fetch('/api/slot?id=' + id, { method: 'DELETE' })
    .then(load);
}

function toggleActive(id, on) {
  fetch(`/api/slot/toggle?id=${id}&active=${on?1:0}`, { method: 'POST' })
    .then(load);
}

function oneTime() {
  fetch(`/api/oneTime?state=${one_action.value}&duration=${one_duration.value}`);
}

load();
</script>

</body>
</html>
)rawliteral");
}


void handleNotFound() {
  ESP8266WebServer& localServer = getWebServer();
  localServer.send(404, "application/json",
      "{\"error\":\"not found\"}");
}


// ---- Registration ----
void registerWebHandlers(ESP8266WebServer& server) {
  server.on("/style.css", HTTP_GET, handleStylesheet);
  server.on("/", HTTP_GET, handleDashboard);
  server.on("/manual", HTTP_GET, handleManualPage);
  server.on("/schedule", HTTP_GET, handleSchedulePage);
  // the handleNotFound has to be added in the end in the main setup
}

