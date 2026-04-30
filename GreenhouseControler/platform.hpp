#pragma once
#include <ESP8266WiFi.h>          // Wi‑Fi setup
#include <ESP8266WebServer.h>     // Web server
#include <LittleFS.h>             // Filesystem setup



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

void initPlatform(const char* hostname, const char* ssid, const char* password);

ESP8266WebServer& getWebServer();