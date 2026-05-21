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

struct NetworkConfig {
  bool dhcp;
  IPAddress ip;
  IPAddress gateway;
  IPAddress subnet;

  bool ap_enable;
  String ap_ssid;
  String ap_password;

  String sta_ssid;
  String sta_password;

  String hostname;
};

// -----------------------------------------------------------------------------
// Methods
// -----------------------------------------------------------------------------
void setLed(LedAction action);

void initPlatform();

ESP8266WebServer& getWebServer();
bool isConnectedToWiFi();
void sendJsonResponse(const ArduinoJson::JsonDocument& doc);

String ipToString(const IPAddress& ip);
IPAddress stringToIP(const String& s);

const NetworkConfig getConfig();
bool setConfig(const NetworkConfig& cfg);
bool clearConfig();

void applyNetwork(NetworkConfig& cfg);

void performPlatformHandling();
