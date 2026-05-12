//#include <Arduino.h>              // Explicit, even though .ino gets it implicitly
#include "platform.hpp"
#include "control.hpp"
#include "schedule.hpp"
#include "timeservice.hpp"
#include "webhandlers.hpp"
#include "apihandlers.hpp"
#include "wificredentials.hpp"

// ---------------- WiFi credentials ----------------
//const char ssid[]     = "SSID";
//const char password[] = "PASSWORD";

const char hostname[] = "greenhouse";

// const int CONTROL_PIN = D2;
const int CONTROL_PIN = 16;

bool connectionWasEstablished = false;

void setup() {
  Serial.begin(115200);
  
  initPlatform(hostname, ssid, password, otaPassword);
  
  connectionWasEstablished = isConnectedToWiFi();
  // Synchronize the time
  setupTime();

  initSchedule();
  initControl(CONTROL_PIN);

  registerWebHandlers(getWebServer());
  registerApiHandlers(getWebServer());
  // 404 handler
  getWebServer().onNotFound(handleNotFound);
  if (connectionWasEstablished) 
  {
    getWebServer().begin();
    Serial.println("HTTP server started");
  }
}

// ---------------- Loop ----------------

void loop() {
  performPlatformHandling(); 
  if (!connectionWasEstablished && isConnectedToWiFi()){
    connectionWasEstablished = true;
    getWebServer().begin();
    Serial.println("HTTP server started");
    setupTime();  
  }
  if (connectionWasEstablished) getWebServer().handleClient();
  checkIrrigationStatus();
  checkSavedTime();
  checkSchedulePersistency();
}
