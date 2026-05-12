#include "platform.hpp"
#include "control.hpp"

// ---------------- Server ----------------
ESP8266WebServer server(80);
ESP8266WebServer& getWebServer()
{
  return server;
}

// ---------------- Setup ----------------
static void setupWiFi(const char* hostname, const char* ssid, const char* password) {
  WiFi.hostname(hostname);
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  WiFi.setAutoReconnect(true);
  Serial.println();
  Serial.println("Connecting to WiFi...");

  // add code to count the number of attempts and to continue without Wifi if necessary
  // like: if (wifi_enabled) or if(webserveractive) etc. an do extra reconnect checks in the loop.
  // also correct the time fallback with the number connection attempts times 500milis
  int retries = 10;
  while ((WiFi.status() != WL_CONNECTED) && (retries-- > 0)) {
    delay(500);
    Serial.print(".");
  }
  if (WiFi.status() == WL_CONNECTED){
    Serial.println();
    Serial.print("Connected to ");
    Serial.println(ssid);
    Serial.print("IP address: ");
    Serial.println(WiFi.localIP());
  }
  else {
    Serial.println();
    Serial.println("Connection failed to start, continuing without internet!!");
  }
}

static void setupFileSystem() {
  if (!LittleFS.begin()) {
      Serial.println("LittleFS mount failed, trying to format");
      LittleFS.format();
      if (!LittleFS.begin()) Serial.println("LittleFS mount failed again"); 
        else Serial.println("LittleFS mounted");
    } else {
      Serial.println("LittleFS mounted");
    }
}

static void setupOta(const char* hostname, const char* otaPassword)
{
  
ArduinoOTA.setHostname(hostname);        // you already have this
  ArduinoOTA.setPassword(otaPassword);     // REQUIRED

  ArduinoOTA.onStart([]() {
    // Safety: ensure outputs go OFF
    suppressOperation(true);
    Serial.println("OTA update started");
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("OTA update finished");
  });

  ArduinoOTA.onError([](ota_error_t error) {
    suppressOperation(true);
    Serial.printf("OTA error %u\n", error);
  });

  ArduinoOTA.begin();

}

void setLed(LedAction action)
{
  digitalWrite(LED_PIN, action == LED_ON ? LOW : HIGH);  
}

void initPlatform(const char* hostname, const char* ssid, const char* password, const char* otaPassword){
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // LED off initially
  
  setupWiFi(hostname, ssid, password);
#ifdef OTA_ENABLED
  setupOta(hostname, otaPassword);
#endif

  setupFileSystem();
  
}

void performPlatformHandling(){  
#ifdef OTA_ENABLED
  ArduinoOTA.handle();
#endif  
}

bool isConnectedToWiFi(){
  return WiFi.status() == WL_CONNECTED;
}