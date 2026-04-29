#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <sys/time.h>
#include <time.h>

// ---------------- WiFi credentials ----------------
const char ssid[]     = "village_of_the_knives";
const char password[] = "FD20041972";
//const char ssid[]     = "H369AB1E97C";
//const char password[] = "C77699C9E65E";
const char hostname[] = "greenhouse";

// ---------------- Server ----------------
ESP8266WebServer server(80);

// ---------------- Hardware ----------------
const int LED_PIN = LED_BUILTIN;  // onboard LED (active LOW)
// const int CONTROL_PIN = D2;
const int CONTROL_PIN = 16;

// ---------------- Functional Datatypes -------------
enum PinAction {
  PIN_ON,
  PIN_OFF
};

#define MAX_SLOTS 10

struct TimeSlot {
  uint8_t hour;
  uint8_t minute;
  PinAction action;

  uint8_t startMonth;
  uint8_t startDay;
  uint8_t endMonth;
  uint8_t endDay;

  bool active;
};

TimeSlot schedule[MAX_SLOTS];
int scheduleCount = 0;
bool scheduleDirty = false;
unsigned long lastChangeMillis = 0;

// ---------------- Utilities --------------------
String getCurrentTimeISO8601() {
  time_t now = time(nullptr);
  struct tm* timeinfo = localtime(&now);

  char buffer[25];
  strftime(buffer, sizeof(buffer), "%Y-%m-%dT%H:%M:%S", timeinfo);
  return String(buffer);
}

void setSystemTime(time_t restoredTime) {
  struct timeval tv;
  tv.tv_sec = restoredTime;
  tv.tv_usec = 0;

  settimeofday(&tv, nullptr);
}


bool isSlotActiveAtGivenDay(struct tm* t, TimeSlot& slot) {
  // No date restriction
  if (slot.startMonth == 0 || slot.endMonth == 0) {
    return true;
  }

  int month = t->tm_mon + 1;
  int day   = t->tm_mday;

  auto afterOrEqual = [&](int m, int d) {
    return (month > m) || (month == m && day >= d);
  };

  auto beforeOrEqual = [&](int m, int d) {
    return (month < m) || (month == m && day <= d);
  };

  bool normalRange =
    (slot.startMonth < slot.endMonth) ||
    (slot.startMonth == slot.endMonth && slot.startDay <= slot.endDay);

  if (normalRange) {
    // Inclusive active window: start ≤ today ≤ end
    return afterOrEqual(slot.startMonth, slot.startDay) &&
           beforeOrEqual(slot.endMonth, slot.endDay);
  } else {
    // Exclusion gap: inactive between end+1 and start-1
    // Active OUTSIDE that gap
    return afterOrEqual(slot.startMonth, slot.startDay) ||
           beforeOrEqual(slot.endMonth, slot.endDay);
  }
}

// Comparison function: returns true if a should come after b
bool isLater(const TimeSlot &a, const TimeSlot &b) {
  if (a.hour != b.hour) {
    return a.hour > b.hour;
  }
  return a.minute > b.minute;
}

// Simple bubble sort for small arrays
void sortSchedule(TimeSlot arr[], int size) {
  for (int i = 0; i < size - 1; i++) {
    for (int j = 0; j < size - i - 1; j++) {
      if (isLater(arr[j], arr[j + 1])) {
        // Swap
        TimeSlot temp = arr[j];
        arr[j] = arr[j + 1];
        arr[j + 1] = temp;
      }
    }
  }
}

// ---------------- Scheduler control functions ------
PinAction pinStatus = PIN_OFF;

void setControlPin(PinAction action, bool force) {
    if (force || pinStatus != action) {
      pinStatus = action;
      digitalWrite(
          CONTROL_PIN,
          pinStatus == PIN_ON ? HIGH : LOW
      );
    }
}

PinAction actionAccordingToSchedule() {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);

  // NTP not ready
  if (t->tm_year < (2020 - 1900)) return PIN_OFF;

  // look for the most recent previous action, this one denotes whot the current state should be 
  TimeSlot* best = nullptr;

  for (int i = 0; i < scheduleCount; i++) {
    auto &slot = schedule[i]; 
    if (!slot.active) continue;
    if (!isSlotActiveAtGivenDay(t, slot)) continue;

    bool timeReached = 
      (t->tm_hour > slot.hour) ||
      (t->tm_hour == slot.hour && t->tm_min >= slot.minute);
    
    bool timeReachedAndCloserThanPrevious = timeReached &&
      ((best == nullptr) || isLater(slot, *best)
      );
    
    if (timeReachedAndCloserThanPrevious) best = &slot;
  }

  if (best == nullptr) { // time before first slot of the day
    time_t yesterday = now - 86400; // 24 * 60 * 60
    struct tm *tYesterday = localtime(&yesterday);
    int j = scheduleCount -1;
    while (j >= 0)
    {
      auto &slot = schedule[j]; 
      if (slot.active && isSlotActiveAtGivenDay(tYesterday, slot)){
        return slot.action;
      }
      j--;
    } 
  }
  return best ? best->action : PIN_OFF;
}

time_t oneTimeTimer = time(nullptr);
int minutesToTime = -1; 
PinAction actionAccordingToOnetimeTimer(){
  if (minutesToTime < 0) return PIN_OFF;

  time_t now = time(nullptr);
  if (now > oneTimeTimer + 60 * minutesToTime) return PIN_OFF;
  else return PIN_ON;
}

void checkIrrigationStatus(){
  PinAction scheduleAction = actionAccordingToSchedule();
  PinAction oneTimeAction = actionAccordingToOnetimeTimer();
  PinAction result = (scheduleAction == PIN_ON || oneTimeAction == PIN_ON) ? PIN_ON : PIN_OFF; 
  setControlPin(result, false);
}

// --------- Storage methods ------------------------

#define version_magic 0x53434830
// the magich is'SCH0' -> changed from 'SCHD' because removed 'hasRun' property from schedule

void saveScheduleToFlash() {
  File f = LittleFS.open("/schedule.dat", "w");
  if (!f) {
    Serial.println("Failed to open schedule.dat for writing");
    return;
  }

  uint32_t magic = version_magic;
  f.write((uint8_t*)&magic, sizeof(magic));

  f.write((uint8_t*)&scheduleCount, sizeof(scheduleCount));
  f.write((uint8_t*)schedule, sizeof(TimeSlot) * scheduleCount);

  f.close();
  Serial.println("Schedule saved to flash");
}


void loadScheduleFromFlash() {
  if (!LittleFS.exists("/schedule.dat")) {
    Serial.println("No schedule file found");
    scheduleCount = 0;
    return;
  }

  File f = LittleFS.open("/schedule.dat", "r");
  if (!f) {
    Serial.println("Failed to open schedule.dat");
    scheduleCount = 0;
    return;
  }
  
  uint32_t magic;
  f.read((uint8_t*)&magic, sizeof(magic));
  if (magic != version_magic) {
    Serial.println("Schedule file version inncorrect, resetting");
    scheduleCount = 0;
    f.close();
    return;
  }

  f.read((uint8_t*)&scheduleCount, sizeof(scheduleCount));

  if (scheduleCount > MAX_SLOTS) {
    Serial.println("Schedule file corrupt, resetting");
    scheduleCount = 0;
    f.close();
    return;
  }

  f.read((uint8_t*)schedule, sizeof(TimeSlot) * scheduleCount);

  // Reset runtime-only flags
  /*for (int i = 0; i < scheduleCount; i++) {
    schedule[i].active = true;
  }
*/

  f.close();
  Serial.printf("Loaded %d schedule entries\n", scheduleCount);
}

struct TimeBackup {
  uint32_t unixTime;
  uint32_t millisAtSave;
};

void saveTimeToFS(time_t now) {
  TimeBackup backup;
  backup.unixTime = now;
  backup.millisAtSave = millis();

  File f = LittleFS.open("/time.dat", "w");
  if (!f) {
    Serial.println("Failed to open time.dat for writing");
    return;
  }

  f.write((uint8_t*)&backup, sizeof(backup));
  f.close();
}

bool restoreTimeFromFS(time_t &restoredTime) {
  if (!LittleFS.exists("/time.dat")) {
    Serial.println("No time backup found");
    return false;
  }

  File f = LittleFS.open("/time.dat", "r");
  if (!f || f.size() != sizeof(TimeBackup)) {
    Serial.println("Invalid time backup file");
    if (f) f.close();
    return false;
  }

  TimeBackup backup;
  f.read((uint8_t*)&backup, sizeof(backup));
  f.close();

  // Sanity checks
  if (backup.unixTime < 1609459200UL) {   // Jan 1, 2021
    Serial.println("Backup time is invalid");
    return false;
  }

  uint32_t nowMillis = millis();

  // Handle millis() rollover safely
  uint32_t elapsedMillis =
      (nowMillis >= backup.millisAtSave)
      ? (nowMillis - backup.millisAtSave)
      : (UINT32_MAX - backup.millisAtSave + nowMillis + 1);

  restoredTime = backup.unixTime + (elapsedMillis / 1000);

  return true;
}

time_t lastSavedTime = time(nullptr); 
// This method can periodically be used to store the time every 5 minutes
void checkSavedTime(){
  time_t now = time(nullptr); 
  if (now > lastSavedTime + 300){
    saveTimeToFS(now);
    lastSavedTime = now;
  }
}

void checkSchedulePersistency()
{
  if (scheduleDirty)
    if (millis() - lastChangeMillis > 1000)
    {
      saveScheduleToFlash();
      scheduleDirty = false;
    }
}

void setScheduleDirty()
{
  scheduleDirty = true;
  lastChangeMillis = millis();
}

//----------------- HTTP API Handlers ----------------
void handleGetSchedule() {
  bool first = true;
  String json = "[";
  for (int i = 0; i < scheduleCount; i++) {
    if (!first)
       json +=",";
    first = false;
    json += "{";
    json += "\"id\":" + String(i) + ",";
    json += "\"hour\":" + String(schedule[i].hour) + ",";
    json += "\"minute\":" + String(schedule[i].minute) + ",";
    json += "\"action\":\"" + String(schedule[i].action == PIN_ON ? "on" : "off") + "\",";
    json += "\"startMonth\":" + String(schedule[i].startMonth) + ",";
    json += "\"startDay\":" + String(schedule[i].startDay) + ",";
    json += "\"endMonth\":" + String(schedule[i].endMonth) + ",";
    json += "\"endDay\":" + String(schedule[i].endDay);
    json += "}";
  }
  json += "]";
  server.send(200, "application/json", json);
}


void handleAddSlot() {
  if (!server.hasArg("hour") || !server.hasArg("minute")) {
    server.send(400, "text/plain", "Missing parameters");
    return;
  }

  int hour   = server.arg("hour").toInt();
  int minute = server.arg("minute").toInt();

  if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
    server.send(400, "text/plain", "Invalid time");
    return;
  }

  if (scheduleCount >= MAX_SLOTS) {
    server.send(400, "text/plain", "Schedule full");
    return;
  }

  TimeSlot &s = schedule[scheduleCount];
  s.hour   = hour;
  s.minute = minute;
  s.action = server.arg("action") == "on" ? PIN_ON : PIN_OFF;

  s.startMonth = server.arg("startMonth").toInt();
  s.startDay   = server.arg("startDay").toInt();
  s.endMonth   = server.arg("endMonth").toInt();
  s.endDay     = server.arg("endDay").toInt();

  s.active = true;

  scheduleCount++;
  sortSchedule(schedule, scheduleCount);
  setScheduleDirty();

  server.send(201, "application/json", "{\"result\":\"created\"}");
}

void handleDeleteSlot() {
  if (!server.hasArg("id")) {
    server.send(400, "application/json",
      "{\"error\":\"missing id\"}");
    return;
  }

  int id = server.arg("id").toInt();
  if (id < 0 || id >= scheduleCount) {
    server.send(404, "application/json",
      "{\"error\":\"not found\"}");
    return;
  }

  for (int i = id; i < scheduleCount - 1; i++) {
    schedule[i] = schedule[i + 1];
  }
  scheduleCount--;

  setScheduleDirty();

  server.send(200, "application/json", "{\"result\":\"deleted\"}");
}

void handleOneTime() {
  if (!server.hasArg("state")) {
    server.send(400, "application/json",
      "{\"error\":\"missing parameter: state\"}");
    return;
  }

  String state = server.arg("state");
  int duration = 10;
  if (server.hasArg("duration")) duration = server.arg("duration").toInt(); 

  if (state == "on") {
    oneTimeTimer = time(nullptr);
    minutesToTime = duration;
  } else if (state == "off") {
    minutesToTime = -1;  
  } else {
    server.send(400, "application/json",
      "{\"error\":\"state must be on or off\"}");
    return;
  }

  server.send(200, "application/json",
    "{\"result\":\"ok\"}");
}

void handleUpdateSlotActive() {
  if (!server.hasArg("id") || !server.hasArg("active")) {
    server.send(400, "application/json",
      "{\"error\":\"missing id or active\"}");
    return;
  }

  int id = server.arg("id").toInt();
  bool active = server.arg("active").toInt() != 0;

  if (id < 0 || id >= scheduleCount) {
    server.send(404, "application/json",
      "{\"error\":\"not found\"}");
    return;
  }

  schedule[id].active = active;
  setScheduleDirty();

  server.send(200, "application/json",
    "{\"result\":\"ok\"}");
}

// ----- HTTP Web handlers
void handleStylesheet() {
  server.send(200, "text/css", R"rawliteral(
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
  server.send(200, "text/html", R"rawliteral(
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
  server.send(200, "text/html", R"rawliteral(
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
  server.send(200, "text/html", R"rawliteral(
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

// ---------------- REST handlers ----------------
void handleStatus() {
  String json = "{";
  json += "\"status\":\"ok\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"uptime_ms\":" + String(millis()) + ","; 
  json += "\"time\":\"" + getCurrentTimeISO8601() + "\",";
  json += "\"scheduleCount\":" + String(scheduleCount) + ",";
  json += "\"controlpin\":\"" + String(pinStatus == PIN_ON ? "ON" : "OFF") + "\"";
  json += "}";

  server.send(200, "application/json", json);
}

void handleLed() {
  if (!server.hasArg("state")) {
    server.send(400, "application/json",
      "{\"error\":\"missing parameter: state\"}");
    return;
  }

  String state = server.arg("state");

  if (state == "on") {
    digitalWrite(LED_PIN, LOW);   // LED ON
  } else if (state == "off") {
    digitalWrite(LED_PIN, HIGH);  // LED OFF
  } else {
    server.send(400, "application/json",
      "{\"error\":\"state must be on or off\"}");
    return;
  }

  server.send(200, "application/json",
    "{\"result\":\"ok\"}");
}

void handleNotFound() {
  server.send(404, "application/json",
      "{\"error\":\"not found\"}");
}
// ---------------- Setup ----------------

// Timezone: Central European Time (CET / CEST)
const long GMT_OFFSET_SEC = 3600;      // UTC +1
const int DAYLIGHT_OFFSET_SEC = 3600; // DST


void setupTime() {

  configTime(
    GMT_OFFSET_SEC,
    DAYLIGHT_OFFSET_SEC,
    "pool.ntp.org",
    "time.nist.gov"
  );

  Serial.println("Waiting for NTP time sync...");

  time_t now = time(nullptr);
  unsigned long start = millis();

  // Wait up to 15 seconds for NTP
  while (now < 100000 && millis() - start < 15000) {
    delay(500);
    now = time(nullptr);
  }

  // NTP successful
  if (now >= 100000) {
    Serial.println("Time synchronized via NTP");
    saveTimeToFS(now);
    return;
  }

  Serial.println("NTP failed, attempting restore from filesystem");

  // Fallback #1: filesystem
  time_t restoredTime;
  if (restoreTimeFromFS(restoredTime)) {
    // compensate ~25 seconds for: the restart, setting up the serial port and the failed NTP sync
    restoredTime += 25;
    setSystemTime(restoredTime);
    Serial.println("Time restored from filesystem");
    return;
  }

  Serial.println("Filesystem restore failed, using build time");

  // Fallback #2: build time (__DATE__ / __TIME__)
  struct tm tmBuild{};
  char monthStr[4];

  sscanf(__DATE__, "%3s %d %d",
         monthStr,
         &tmBuild.tm_mday,
         &tmBuild.tm_year);

  sscanf(__TIME__, "%d:%d:%d",
         &tmBuild.tm_hour,
         &tmBuild.tm_min,
         &tmBuild.tm_sec);

  tmBuild.tm_year -= 1900;

  const char *months = "JanFebMarAprMayJunJulAugSepOctNovDec";
  tmBuild.tm_mon = (strstr(months, monthStr) - months) / 3;

  time_t buildTime = mktime(&tmBuild);
  // compensate ~30 seconds for: uploading the binaries, setting up the serial port and the failed NTP sync
  buildTime += 30;
  setSystemTime(buildTime);

  Serial.println("Time set from firmware build time");
}

void setup() {
  Serial.begin(115200);
  delay(4000);

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // LED off initially
  pinMode(CONTROL_PIN, OUTPUT);
  setControlPin(PIN_OFF,true);
  
  Serial.println();
  Serial.println("Connecting to WiFi...");
  WiFi.hostname(hostname);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true);

  // Wait for connection
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  
  // load schedule from flash memory
  if (!LittleFS.begin()) {
      Serial.println("LittleFS mount failed, trying to format");
      LittleFS.format();
      if (!LittleFS.begin()) Serial.println("LittleFS mount failed again"); 
        else Serial.println("LittleFS mounted");
    } else {
      Serial.println("LittleFS mounted");
    }

    loadScheduleFromFlash();

  // Synchronize the time
  setupTime();

  // REST routes
  server.on("/led", HTTP_GET, handleLed);
  
  // website  
  server.on("/style.css", HTTP_GET, handleStylesheet);
  server.on("/", HTTP_GET, handleDashboard);
  server.on("/manual", HTTP_GET, handleManualPage);
  server.on("/schedule", handleSchedulePage);
  server.on("/api/schedule", HTTP_GET, handleGetSchedule);
  server.on("/api/slot", HTTP_POST, handleAddSlot);
  server.on("/api/slot", HTTP_DELETE, handleDeleteSlot);
  server.on("/api/oneTime", HTTP_GET, handleOneTime);
  server.on("/api/slot/toggle", HTTP_POST, handleUpdateSlotActive);
  server.on("/api/status", HTTP_GET, handleStatus);
  
  // 404 handler
  server.onNotFound(handleNotFound);

  server.begin();
  Serial.println("HTTP REST server started");
}

// ---------------- Loop ----------------

void loop() {
  server.handleClient();
  checkIrrigationStatus();
  checkSavedTime();
  checkSchedulePersistency();
}
