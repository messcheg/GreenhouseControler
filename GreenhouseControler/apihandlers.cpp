#include "apihandlers.hpp"
#include "platform.hpp"
#include "control.hpp"
#include "schedule.hpp"
#include "timeservice.hpp"
#include "platform.hpp"

#include <Arduino.h>

void handleStatus() {
  ESP8266WebServer& localServer = getWebServer();
  String json = "{";
  json += "\"status\":\"ok\",";
  json += "\"ip\":\"" + WiFi.localIP().toString() + "\",";
  json += "\"uptime_ms\":" + String(millis()) + ","; 
  json += "\"time\":\"" + getCurrentTimeISO8601() + "\",";
  json += "\"scheduleCount\":" + String(getScheduleCount()) + ",";
  json += "\"mode\":\"";

  switch (getControlMode()) {
    case MODE_AUTO: json += "auto"; break;
    case MODE_MANUAL: json += "manual"; break;
    case MODE_AUTO_AND_MANUAL: json += "auto+manual"; break;
    default: json += "off";
  }
  json += "\",";
  time_t valveOffTime = getValveOffTime(); 
  if (valveOffTime > time(nullptr)) {
    json += "\"valve_off_in\":" +
            String(valveOffTime - time(nullptr)) + ",";
  } else {
    json += "\"valve_off_in\":0,";
  }

  json += "\"controlpin\":\"" + String(getPinStatus() == PIN_ON ? "ON" : "OFF") + "\"";
  
  json += "}";

  localServer.send(200, "application/json", json);
}

void handleLed() {
  ESP8266WebServer& localServer = getWebServer();
  if (!localServer.hasArg("state")) {
    localServer.send(400, "application/json",
      "{\"error\":\"missing parameter: state\"}");
    return;
  }

  String state = localServer.arg("state");

  if (state == "on") {
    setLed(LED_ON);
  } else if (state == "off") {
    setLed(LED_OFF);
  } else {
    localServer.send(400, "application/json",
      "{\"error\":\"state must be on or off\"}");
    return;
  }
  localServer.send(200, "application/json",
    "{\"result\":\"ok\"}");
}

void handleGetSchedule() {
  ESP8266WebServer& localServer = getWebServer();
  bool first = true;
  String json = "[";
  for (int i = 0; i < getScheduleCount(); i++) {
    if (!first)
       json +=",";
    first = false;
    json += "{";
    json += "\"id\":" + String(i) + ",";
    auto slot = getSlot(i);
    json += "\"hour\":" + String(slot.hour) + ",";
    json += "\"minute\":" + String(slot.minute) + ",";
    json += "\"action\":\"" + String(slot.action == PIN_ON ? "on" : "off") + "\",";
    json += "\"startMonth\":" + String(slot.startMonth) + ",";
    json += "\"startDay\":" + String(slot.startDay) + ",";
    json += "\"endMonth\":" + String(slot.endMonth) + ",";
    json += "\"endDay\":" + String(slot.endDay);
    json += "}";
  }
  json += "]";
  localServer.send(200, "application/json", json);
}


void handleAddSlot() {
  ESP8266WebServer& localServer = getWebServer();
  if (!localServer.hasArg("hour") || !localServer.hasArg("minute")) {
    localServer.send(400, "text/plain", "Missing parameters");
    return;
  }

  int hour   = localServer.arg("hour").toInt();
  int minute = localServer.arg("minute").toInt();

  if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
    localServer.send(400, "text/plain", "Invalid time");
    return;
  }

  if (getScheduleCount() >= MAX_SLOTS) {
    localServer.send(400, "text/plain", "Schedule full");
    return;
  }
  
  TimeSlot slot;
  slot.hour   = hour;
  slot.minute = minute;
  slot.action = localServer.arg("action") == "on" ? PIN_ON : PIN_OFF;

  slot.startMonth = localServer.arg("startMonth").toInt();
  slot.startMonth = localServer.arg("startMonth").toInt();
  slot.startMonth = localServer.arg("startMonth").toInt();
  slot.startDay   = localServer.arg("startDay").toInt();
  slot.endMonth   = localServer.arg("endMonth").toInt();
  slot.endDay     = localServer.arg("endDay").toInt();

  slot.active = true;

  addSlot(slot);

  localServer.send(201, "application/json", "{\"result\":\"created\"}");
}

void handleDeleteSlot() {
  ESP8266WebServer& localServer = getWebServer();
  if (!localServer.hasArg("id")) {
    localServer.send(400, "application/json",
      "{\"error\":\"missing id\"}");
    return;
  }

  int id = localServer.arg("id").toInt();
  if (id < 0 || id >= getScheduleCount()) {
    localServer.send(404, "application/json",
      "{\"error\":\"not found\"}");
    return;
  }
  deleteSlot(id);
  localServer.send(200, "application/json", "{\"result\":\"deleted\"}");
}

void handleOneTime() {
  ESP8266WebServer& localServer = getWebServer();
  if (!localServer.hasArg("state")) {
    localServer.send(400, "application/json",
      "{\"error\":\"missing parameter: state\"}");
    return;
  }

  String state = localServer.arg("state");
  int duration = 10;
  if (localServer.hasArg("duration")) duration = localServer.arg("duration").toInt(); 

  if (state == "on") {
    setManualOverride(duration);
  } else if (state == "off") {
    clearManualOverride();
  } else {
    localServer.send(400, "application/json",
      "{\"error\":\"state must be on or off\"}");
    return;
  }

  localServer.send(200, "application/json",
    "{\"result\":\"ok\"}");
}

void handleUpdateSlotActive() {
  ESP8266WebServer& localServer = getWebServer();
  if (!localServer.hasArg("id") || !localServer.hasArg("active")) {
    localServer.send(400, "application/json",
      "{\"error\":\"missing id or active\"}");
    return;
  }

  int id = localServer.arg("id").toInt();
  bool active = localServer.arg("active").toInt() != 0;

  if (id < 0 || id >= getScheduleCount()) {
    localServer.send(404, "application/json",
      "{\"error\":\"not found\"}");
    return;
  }
  setSlotActive(id, active);
  localServer.send(200, "application/json",
    "{\"result\":\"ok\"}");
}

// ---- Registration ----
void registerApiHandlers(ESP8266WebServer& server) {
  server.on("/api/schedule", HTTP_GET, handleGetSchedule);
  server.on("/api/slot", HTTP_POST, handleAddSlot);
  server.on("/api/slot", HTTP_DELETE, handleDeleteSlot);
  server.on("/api/oneTime", HTTP_GET, handleOneTime);
  server.on("/api/slot/toggle", HTTP_POST, handleUpdateSlotActive);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/led", HTTP_GET, handleLed);  
}
