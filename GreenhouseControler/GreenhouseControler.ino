//#include <Arduino.h>              // Explicit, even though .ino gets it implicitly
#include "definitions.hpp"
#include "platform.hpp"
#include "control.hpp"
#include "schedule.hpp"
#include "timeservice.hpp"
#include "webhandlers.hpp"
#include "apihandlers.hpp"



bool connectionWasEstablished = false;

void setup() {
  Serial.begin(115200);
  
  initPlatform();
  
  connectionWasEstablished = isConnectedToWiFi();
  // Synchronize the time
  setupTime();

  initSchedule();
  initControl(CONTROL_PIN);

  registerApiHandlers();
  registerWebHandlers();
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
