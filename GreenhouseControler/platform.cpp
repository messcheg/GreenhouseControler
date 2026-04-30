#include "platform.hpp"

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

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println();
  Serial.print("Connected to ");
  Serial.println(ssid);
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

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

void setLed(LedAction action)
{
  digitalWrite(LED_PIN, action == LED_ON ? LOW : HIGH);  
}

void initPlatform(const char* hostname, const char* ssid, const char* password){
  setupWiFi(hostname, ssid, password);
  
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // LED off initially
  
  setupFileSystem();
 }

