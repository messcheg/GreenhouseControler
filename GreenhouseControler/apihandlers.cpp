#include "apihandlers.hpp"
#include "platform.hpp"
#include "control.hpp"
#include "schedule.hpp"
#include "timeservice.hpp"

#include <Arduino.h>
#include <ArduinoJson.h>

void handleStatus() {
  ESP8266WebServer& server = getWebServer();

  StaticJsonDocument<512> doc;

  doc["fw"] = FW_VERSION;
  doc["status"] = "OK";
  doc["ip"] = WiFi.localIP().toString();
  doc["uptime_ms"] = millis();
  doc["time"] = getCurrentTimeISO8601();
  doc["scheduleCount"] = getScheduleCount();

  switch (GetcurrentTimeSource()){
    case NTP_SYNC:              doc["timeSource"] = "ntp synchronization"; break;
    case FROM_STORAGE:          doc["timeSource"] = "restored from filesystem"; break;
    case COMPILE_TIME:          doc["timeSource"] = "compile time"; break;
    default:                    doc["timeSource"] = "unknown"; break;
  }

  switch (getControlMode()) {
    case MODE_AUTO:             doc["mode"] = "auto"; break;
    case MODE_MANUAL:           doc["mode"] = "manual"; break;
    case MODE_AUTO_AND_MANUAL:  doc["mode"] = "auto+manual"; break;
    case MODE_FORCED_OFF: 
      switch (getOperationSuppressionState()){
        case SUPR_UPDATE:       doc["mode"] = "off (suppressed for update)"; break;
        case SUPR_MANUAL:       doc["mode"] = "off (suppressed manual)"; break;
        case SUPR_MANUAL_UPDATE:doc["mode"] = "off (suppressed update+manual)"; break;
        case SUPR_NONE:         doc["mode"] = "off (suppressed)"; break;
      }; break;
    default:                    doc["mode"] = "off"; break;
  }
  doc["operation_suppressed"] = (getOperationSuppressionState() & SUPR_MANUAL) > 0;
  
  time_t off = getValveOffTime();
  doc["valve_off_in"] = (off > time(nullptr)) ? (off - time(nullptr)) : 0;

  doc["controlpin"] = (getPinStatus() == PIN_ON) ? "ON" : "OFF";
  doc["manual_can_on"] = (getControlMode() == MODE_OFF || getControlMode() == MODE_AUTO);
  doc["manual_can_off"] = (getControlMode() == MODE_MANUAL || getControlMode() == MODE_AUTO_AND_MANUAL);

  sendJsonResponse(doc);
}

void handleLed() {
  ESP8266WebServer& localServer = getWebServer();
  if (!localServer.hasArg("state")) {
    localServer.send(400, F("application/json"),
      "{\"error\":\"missing parameter: state\"}");
    return;
  }

  String state = localServer.arg("state");

  if (state == "on") {
    setLed(LED_ON);
  } else if (state == "off") {
    setLed(LED_OFF);
  } else {
    localServer.send(400, F("application/json"),
      "{\"error\":\"state must be on or off\"}");
    return;
  }
  localServer.send(200, F("application/json"),
    F("{\"result\":\"ok\"}"));
}

void handleGetSchedule() {
  ESP8266WebServer& server = getWebServer();

  // Estimate: each slot is ~100 bytes
  StaticJsonDocument<MAX_SLOTS * 128> doc;

  JsonArray arr = doc.to<JsonArray>();

  int count = getScheduleCount();
  for (int i = 0; i < count; i++) {
    TimeSlot slot = getSlot(i);

    JsonObject obj = arr.createNestedObject();
    obj["id"] = i;
    obj["hour"] = slot.hour;
    obj["minute"] = slot.minute;
    obj["action"] = (slot.action == PIN_ON) ? "on" : "off";
    obj["startMonth"] = slot.startMonth;
    obj["startDay"] = slot.startDay;
    obj["endMonth"] = slot.endMonth;
    obj["endDay"] = slot.endDay;
    obj["active"] = slot.active;
  }
  sendJsonResponse(doc);
}

bool handleTimeAndDateValidity( 
  int& hour, int& minute, 
  int& startMonth, int& startDay, 
  int& endMonth, int& endDay){

  ESP8266WebServer& localServer = getWebServer();
  if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
    localServer.send(400, "text/plain", "Invalid time");
    return false;
  }
  const int maxDays[13] = {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};

  // we will accept valid months or only 0's (which means always)
  // the exceptional 29th of february is not a problem for the schedule logic
  if (startMonth == 0 || endMonth == 0 || startDay == 0 || endDay == 0){
    startMonth = endMonth = startDay = endDay = 0;
  }
  else if (startMonth < 1 || startMonth > 12 || 
      endMonth < 1 || endMonth > 12 || 
      startDay < 1 || startDay > maxDays[startMonth] ||
      endDay < 1 || endDay > maxDays[endMonth] ) {
    localServer.send(400, "text/plain", "Invalid date");
    return false;
  }
  
  return true;
}

void handleAddSlot() {
  ESP8266WebServer& localServer = getWebServer();
  if (!localServer.hasArg("hour") || !localServer.hasArg("minute")) {
    localServer.send(400, "text/plain", "Missing parameters");
    return;
  }

  int hour   = localServer.arg("hour").toInt();
  int minute = localServer.arg("minute").toInt();
  int startMonth = localServer.arg("startMonth").toInt();
  int startDay   = localServer.arg("startDay").toInt();
  int endMonth   = localServer.arg("endMonth").toInt();
  int endDay     = localServer.arg("endDay").toInt();

  if (!handleTimeAndDateValidity(hour, minute, startMonth, startDay, endMonth, endDay)) return;

  if (getScheduleCount() >= MAX_SLOTS) {
    localServer.send(409, "text/plain", "Schedule full");
    return;
  }
  
  TimeSlot slot;
  slot.hour   = hour;
  slot.minute = minute;
  slot.action = localServer.arg("action") == "on" ? PIN_ON : PIN_OFF;

  slot.startMonth = startMonth;
  slot.startDay   = startDay;
  slot.endMonth   = endMonth;
  slot.endDay     = endDay;

  slot.active = true;

  addSlot(slot);

  localServer.send(201, F("application/json"), "{\"result\":\"created\"}");
}

void handleDeleteSlot() {
  ESP8266WebServer& localServer = getWebServer();
  if (!localServer.hasArg("id")) {
    localServer.send(400, F("application/json"),
      "{\"error\":\"missing id\"}");
    return;
  }

  int id = localServer.arg("id").toInt();
  if (id < 0 || id >= getScheduleCount()) {
    localServer.send(404, F("application/json"),
      "{\"error\":\"not found\"}");
    return;
  }
  deleteSlot(id);
  localServer.send(200, F("application/json"), "{\"result\":\"deleted\"}");
}

void handleOneTime() {
  ESP8266WebServer& localServer = getWebServer();
  if (!localServer.hasArg("state")) {
    localServer.send(400, F("application/json"),
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
    localServer.send(400, F("application/json"),
      "{\"error\":\"state must be on or off\"}");
    return;
  }

  localServer.send(200, F("application/json"),
    F("{\"result\":\"ok\"}"));
}

void handleUpdateSlotActive() {

  ESP8266WebServer& localServer = getWebServer();
  if (!localServer.hasArg("id") || !localServer.hasArg("active")) {
    localServer.send(400, F("application/json"),
      "{\"error\":\"missing id or active\"}");
    return;
  }

  int id = localServer.arg("id").toInt();
  bool active = localServer.arg("active").toInt() != 0;

  if (id < 0 || id >= getScheduleCount()) {
    localServer.send(404, F("application/json"),
      "{\"error\":\"not found\"}");
    return;
  }
  setSlotActive(id, active);
  localServer.send(200, F("application/json"),
    F("{\"result\":\"ok\"}"));
}

void handleUpdateSlot() {
  ESP8266WebServer& localServer = getWebServer();
  if (!localServer.hasArg("id")) {
    localServer.send(400, F("application/json"),
      "{\"error\":\"missing id\"}");
    return;
  }

  int id = localServer.arg("id").toInt();
  if (id < 0 || id >= getScheduleCount()) {
    localServer.send(404, F("application/json"),
      "{\"error\":\"not found\"}");
    return;
  }

  int hour       = localServer.arg("hour").toInt();
  int minute     = localServer.arg("minute").toInt();
  int startMonth = localServer.arg("startMonth").toInt();
  int startDay   = localServer.arg("startDay").toInt();
  int endMonth   = localServer.arg("endMonth").toInt();
  int endDay     = localServer.arg("endDay").toInt();
  
  if (!handleTimeAndDateValidity(hour, minute, startMonth, startDay, endMonth, endDay)) return;

  TimeSlot s;
  s.hour       = hour;
  s.minute     = minute;
  s.action     = localServer.arg("action") == "on" ? PIN_ON : PIN_OFF;
  s.startMonth = startMonth;
  s.startDay   = startDay;
  s.endMonth   = endMonth;
  s.endDay     = endDay;
  s.active     = localServer.arg("active").toInt() != 0;

  updateSlot(id, s);   
  localServer.send(200, F("application/json"), "{\"result\":\"updated\"}");
}

void handleHoldOperation() {
  suppressOperation(true, SUPR_MANUAL);
  ESP8266WebServer& server = getWebServer();
  server.send(200, F("application/json"),
              "{\"result\":\"operation suppressed\"}");
}

void handleResumeOperation() {
  suppressOperation(false, SUPR_MANUAL);  // if you allow manual resume
  ESP8266WebServer& server = getWebServer();
  server.send(200, F("application/json"),
              "{\"result\":\"operation resumed\"}");
}

#include <ArduinoJson.h>

void handleUpdateConfig() {
  NetworkConfig cfg;
  ESP8266WebServer& server = getWebServer();
 
  cfg.dhcp = server.arg("dhcp") == "true";
  cfg.ap_enable = server.arg("ap_enable") == "true";

  cfg.ip.fromString(server.arg("ip"));
  cfg.gateway.fromString(server.arg("gateway"));
  cfg.subnet.fromString(server.arg("subnet"));

  cfg.ap_ssid = server.arg("ap_ssid");
  cfg.ap_password = server.arg("ap_password");

  // keep existing WiFi creds or update if included
  cfg.sta_ssid = server.arg("ssid");
  cfg.sta_password = server.arg("password");

  saveConfig(cfg);   // EEPROM / SPIFFS
  server.send(200, "text/plain", "OK");

  delay(1000);
  ESP.restart();
}

void handleGetConfig() {
  NetworkConfig cfg;
  ESP8266WebServer& server = getWebServer();
  StaticJsonDocument<512> doc;

  if (loadConfig(cfg)) {
    doc["dhcp"] = cfg.dhcp;

    doc["ip"]      = ipToString(cfg.ip);
    doc["gateway"] = ipToString(cfg.gateway);
    doc["subnet"]  = ipToString(cfg.subnet);

    doc["ap_enable"]   = cfg.ap_enable;
    doc["ap_ssid"]     = cfg.ap_ssid;
    doc["ap_password"] = cfg.ap_password;

    doc["sta_ssid"]     = cfg.sta_ssid;

    // safe password handling 
    doc["sta_password"] = cfg.sta_password.length() ? "********" : "";

    doc["status"] = "ok";
  } else {
    doc["status"] = "empty";
  }

  sendJsonResponse(doc);
}


void handleFactoryReset() {
  ESP8266WebServer& server = getWebServer();
  clearConfig();  // erase EEPROM/SPIFFS
  server.send(200, "text/plain", "RESET");

  delay(1000);
  ESP.restart();
}

// ---- Registration ----
void registerApiHandlers() {
  ESP8266WebServer& server = getWebServer();
  server.on("/api/schedule", HTTP_GET, handleGetSchedule);
  server.on("/api/slot", HTTP_POST, handleAddSlot);
  server.on("/api/slot", HTTP_DELETE, handleDeleteSlot);
  server.on("/api/oneTime", HTTP_GET, handleOneTime);
  server.on("/api/slot/toggle", HTTP_POST, handleUpdateSlotActive);
  server.on("/api/slot/update", HTTP_POST, handleUpdateSlot);
  server.on("/api/status", HTTP_GET, handleStatus);
  server.on("/api/led", HTTP_GET, handleLed);   
  server.on("/api/hold", HTTP_POST, handleHoldOperation);
  server.on("/api/resume", HTTP_POST, handleResumeOperation);
  server.on("/api/config/update", HTTP_POST, handleUpdateConfig);
  server.on("/api/config/get", HTTP_GET, handleGetConfig);
  server.on("/api/factory_reset", HTTP_POST, handleFactoryReset);
}
