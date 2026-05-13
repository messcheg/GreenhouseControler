#include "WebHandlers.hpp"
#include <ESP8266WiFi.h>
#include <Arduino.h>
#include "platform.hpp"

static const char CSS_HTML[] PROGMEM = R"rawliteral(
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

button:disabled {
  background: #bbb;
  color: #666;
  cursor: not-allowed;
  opacity: 0.6;
}

button:not(:disabled) {
  opacity: 1;
}

.badge-on { color: green; font-weight: bold; }
.badge-off { color: red; font-weight: bold; }
.row, .schedule-row {
    display: flex;
    justify-content: space-between;
    align-items: center;
    margin: 6px 0;
}

.schedule-item {
    background: var(--card);
    border: 1px solid var(--border);
    border-radius: 6px;
    padding: 10px;
    margin: 10px 0;
}

.schedule-meta {
    font-size: 14px;
    color: #555;
}
input[type="number"], select {
    width: 100%;
    max-width: 120px;
    padding: 6px;
    font-size: 16px;
    box-sizing: border-box;
}
.nav-links a {
    display: block;
    padding: 8px 0;
    font-size: 18px;
}

/* --- Schedule form layout --- */

.schedule-form {
  display: grid;
  grid-template-columns: 70px 1fr;
  row-gap: 10px;
  column-gap: 10px;
  align-items: center;
}

.schedule-form .time-row,
.schedule-form .date-row {
  display: grid;
  grid-template-columns: 1fr 1fr;
  gap: 6px;
}

.schedule-form label {
  font-size: 14px;
  color: #333;
}

.schedule-form input,
.schedule-form select {
  width: 100%;
  padding: 6px;
  font-size: 16px;
}

/* --- Schedule list layout --- */

.schedule-item {
  display: grid;
  grid-template-columns: 80px 1fr auto;
  gap: 10px;
  align-items: center;
}

.schedule-item strong {
  font-size: 18px;
}

.schedule-item.inactive {
  opacity: 0.45;
  filter: grayscale(40%);
}

.schedule-meta {
  font-size: 14px;
  color: #555;
}

.schedule-actions {
  text-align: right;
}

)rawliteral";

void handleStylesheet() {
  ESP8266WebServer& localServer = getWebServer();
  localServer.send_P(200, "text/css; charset=utf-8", CSS_HTML);
}

static const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Greenhouse Status</title>
<link rel="stylesheet" href="/style.css">
</head>
<body>
<header>Greenhouse Controller</header>
<div class="card">
  <div class="row"><span>Firmware</span><span id="fw">—</span></div>
  <div class="row"><span>Status</span><span id="status">—</span></div>
  <div class="row"><span>Output</span><span id="output">—</span></div>
  <div class="row"><span>ControlMode</span><span id="mode">—</span></div>
  <div class="row"><span>Schedules</span><span id="slots">—</span></div>
  <div class="row"><span>Time</span><span id="time">—</span></div>
  <div class="row"><span>TimeSource</span><span id="timeSource">—</span></div>
  <div class="row"><span>Uptime</span><span id="uptime">—</span></div>
  <div class="row"><span>IP</span><span id="ip">—</span></div>
</div>

<div class="card nav-links">
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
      document.getElementById('fw').textContent = s.fw;
      document.getElementById('status').textContent = s.status;
      document.getElementById('ip').textContent = s.ip;
      document.getElementById('time').textContent = s.time;
      document.getElementById('timeSource').textContent = s.timeSource;
      document.getElementById('slots').textContent = s.scheduleCount;
      document.getElementById('uptime').textContent = msToTime(s.uptime_ms);

      document.getElementById('output').innerHTML =
        s.controlpin === "ON"
          ? '<span class="badge-on">ON</span>'
          : '<span class="badge-off">OFF</span>';
      document.getElementById('mode').innerHTML =
        s.controlpin === "ON"
          ? '<span class="badge-on">' + s.mode + '</span>'
          : '<span class="badge-off">'+ s.mode +'</span>';    
    });
}

refresh();
setInterval(refresh, 2000);
</script>
</body>
</html>
)rawliteral";

void handleDashboard() {
 ESP8266WebServer& localServer = getWebServer();
   localServer.send_P(200, "text/html; charset=utf-8", DASHBOARD_HTML);
}

static const char MANUAL_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>Manual Control</title>
<link rel="stylesheet" href="/style.css">
</head>

<body>
<header>Manual Control</header>

<div class="card">
  <button id="manualOnBtn" onclick="manualOn()">Manual ON</button>
  <button id="manualOffBtn" onclick="manualOff()">Manual OFF</button>
  <div></div>
  <label>Duration (minutes)</label>
  <input id="duration" type="number" value="10">
  
   <div id="manualRemaining"
     style="margin-top:10px; font-size:14px; color:#555; display:none;">
   </div>

</div>

<div class="card nav-links">
  <a href="/">Dashboard</a><br>
  <a href="/schedule">Schedule</a><br>
  <a href="/manual">Manual</a>
</div>

<script>
let manualRemainingSeconds = 0;

function formatDuration(seconds) {
  if (seconds <= 0) return "";

  const h = Math.floor(seconds / 3600);
  const m = Math.floor((seconds % 3600) / 60);
  const s = seconds % 60;

  if (h > 0) {
    return `${h}:${String(m).padStart(2,'0')}:${String(s).padStart(2,'0')}`;
  }
  return `${m}:${String(s).padStart(2,'0')}`;
}

function refreshManualState() {
  fetch('/api/status')
    .then(r => r.json())
    .then(s => {
      
      const onBtn  = document.getElementById('manualOnBtn');
      const offBtn = document.getElementById('manualOffBtn');
      const dur = document.getElementById('duration');
      const rem    = document.getElementById('manualRemaining');

      onBtn.disabled = !s.manual_can_on;
      offBtn.disabled  = !s.manual_can_off;
      dur.style.display = s.manual_can_on ? "inline-block" : "none";

      
      // Show remaining time until OFF
      if (s.valve_off_in && s.valve_off_in > 0) {
        rem.textContent =
          "Manual mode ends in " + formatDuration(s.valve_off_in);
        rem.style.display = "block";
      } else {
        rem.style.display = "none";
      }
      manualRemainingSeconds = s.valve_off_in || 0;
    });
}

function manualOn() {
  fetch(`/api/oneTime?state=on&duration=${duration.value}`)
    .then(refreshManualState);
}

function manualOff() {
  fetch(`/api/oneTime?state=off`)
    .then(refreshManualState);
}

refreshManualState();
setInterval(refreshManualState, 5000);
setInterval(() => {
  if (manualRemainingSeconds > 0) {
    manualRemainingSeconds--;

    const rem = document.getElementById('manualRemaining');
    rem.textContent =
      "Manual mode ends in " + formatDuration(manualRemainingSeconds);
    if (manualRemainingSeconds == 0) refreshManualState();  
  }
}, 1000);

</script>
</body>
</html>
)rawliteral";

void handleManualPage() {
  ESP8266WebServer& localServer = getWebServer();
  localServer.send_P(200, "text/html; charset=utf-8", MANUAL_HTML);
}

static const char SCHEDULE_HTML[] PROGMEM = R"rawliteral(
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
<div class="card schedule-form">
<h3 id="editorTitle" style="grid-column: 1 / -1;">Add schedule</h3>


<label>Time</label>
<div class="time-row">
  <input id="hour" type="number" placeholder="HH">
  <input id="minute" type="number" placeholder="MM">
</div>

<label>Action</label>
<select id="action">
  <option value="on">ON</option>
  <option value="off">OFF</option>
</select>

<label>Start (month / day)</label>
<div class="date-row">
  <input id="sm" type="number" placeholder="MM">
  <input id="sd" type="number" placeholder="DD">
</div>

<label>End (month / day)</label>
<div class="date-row">
  <input id="em" type="number" placeholder="MM">
  <input id="ed" type="number" placeholder="DD">
</div>

<div></div>
<div>
  <button id="submitBtn" onclick="submitSlot()">Add</button>
  <button id="cancelBtn"
          class="secondary"
          style="display:none;"
          onclick="cancelEdit()">Cancel</button>
</div>
</section>

<section>
  <div id="schedule"></div>
</section>

<div class="card nav-links">
  <a href="/">Dashboard</a><br>
  <a href="/schedule">Schedule</a><br>
  <a href="/manual">Manual</a>
</div>

<script>
let editingId = null;

function load() {
  fetch('/api/schedule')
    .then(r => r.json())
    .then(data => {
      const c = document.getElementById('schedule');
      c.innerHTML = '';
      
      data.forEach(s => {
        
      c.innerHTML += `
      <div class="schedule-item  ${s.active ? '' : 'inactive'}">
        <strong>${s.hour}:${String(s.minute).padStart(2,'0')}</strong>

        <div class="schedule-meta">
          ${s.action.toUpperCase()}<br>
          ${s.startMonth}/${s.startDay} → ${s.endMonth}/${s.endDay}
        </div>

        <div class="schedule-actions">
          <label> <input type="checkbox" ${s.active ? 'checked' : ''} onchange="toggleActive(${s.id}, this.checked)"> Active </label>
          <button onclick='editSlot(${JSON.stringify(s)})'>Edit</button>
          <button class="danger" onclick='deleteSlot(${s.id})'>Delete</button>
        </div>
      </div>`;

      });
    });
}

function submitSlot() {
  const payload =
    `hour=${hour.value}&minute=${minute.value}` +
    `&action=${action.value}` +
    `&startMonth=${sm.value}&startDay=${sd.value}` +
    `&endMonth=${em.value}&endDay=${ed.value}` +
    `&active=1`;

  if (editingId === null) {
    // ADD
    fetch('/api/slot', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: payload
    }).then(load);
  } else {
    // UPDATE
    fetch('/api/slot/update', {
      method: 'POST',
      headers: {'Content-Type': 'application/x-www-form-urlencoded'},
      body: `id=${editingId}&` + payload
    }).then(() => {
      exitEditMode();
      load();
    });
  }
}

function cancelEdit() {
  exitEditMode();
}

function exitEditMode() {
  editingId = null;

  hour.value = '';
  minute.value = '';
  action.value = 'on';
  sm.value = '';
  sd.value = '';
  em.value = '';
  ed.value = '';

  document.getElementById('submitBtn').textContent = "Add";
  document.getElementById('cancelBtn').style.display = "none";
  document.getElementById('editorTitle').textContent = "Add schedule";
}

function deleteSlot(id) {
  fetch('/api/slot?id=' + id, { method: 'DELETE' })
    .then(load);
}

function toggleActive(id, on) {
  fetch(`/api/slot/toggle?id=${id}&active=${on?1:0}`, { method: 'POST' })
    .then(load);
}

function editSlot(s) {
  editingId = s.id;

  hour.value = s.hour;
  minute.value = s.minute;
  action.value = s.action;
  sm.value = s.startMonth;
  sd.value = s.startDay;
  em.value = s.endMonth;
  ed.value = s.endDay;

  document.getElementById('submitBtn').textContent = "Save";
  document.getElementById('cancelBtn').style.display = "inline-block";
  document.getElementById('editorTitle').textContent = "Edit schedule (" + s.id + ")";
}

load();
</script>

</body>
</html>
)rawliteral";

void handleSchedulePage() {
ESP8266WebServer& localServer = getWebServer();
    localServer.send_P(200, "text/html; charset=utf-8", SCHEDULE_HTML);
} 

void handleNotFound() {
  ESP8266WebServer& localServer = getWebServer();
  localServer.send(404, F("application/json"),
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

