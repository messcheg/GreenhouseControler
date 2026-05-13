#pragma once
#include <ESP8266WiFi.h>          // Wi‑Fi setup
#include <ESP8266WebServer.h>     // Web server
#include <LittleFS.h>             // Filesystem setup
#include <ArduinoOTA.h>           // This is used for remote firmware updates

#include <ArduinoJson.h>

#define FW_VERSION "1.0.0"
//#define OTA_ENABLED
// ---------------- Hardware ----------------
const int LED_PIN = LED_BUILTIN;  // onboard LED (active LOW)

// -----------------------------------------------------------------------------
// Public datatypes
// -----------------------------------------------------------------------------
enum LedAction {
  LED_ON,
  LED_OFF
};

// -----------------------------------------------------------------------------
// Methods
// -----------------------------------------------------------------------------
void setLed(LedAction action);

void initPlatform(const char* hostname, const char* ssid, const char* password, const char* otaPassword);

ESP8266WebServer& getWebServer();
bool isConnectedToWiFi();
void sendJsonResponse(const ArduinoJson::JsonDocument& doc);

void performPlatformHandling();