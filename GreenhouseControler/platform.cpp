#include "platform.hpp"
#include "control.hpp"

// ---------------- Server ----------------
ESP8266WebServer server(80);
ESP8266WebServer& getWebServer()
{
  return server;
}

// --------------- Accesspoint mode ------
void startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("greenhouse", "Tomatos#123");

  Serial.println("AP Mode started");
  Serial.println(WiFi.softAPIP());
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

bool saveConfig(const char* ssid, const char* password) {
  
  StaticJsonDocument<256> doc;
  doc["ssid"] = ssid;
  doc["password"] = password;

  File f = LittleFS.open("/config.json", "w");
  if (!f) {
    return false;
  }

  serializeJson(doc, f);
  f.close();
  
  return true;
}

bool loadConfig(String &ssid, String &pass) {
  if (!LittleFS.exists("/config.json")) return false;

  File f = LittleFS.open("/config.json", "r");
  if (!f) return false;

  StaticJsonDocument<256> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) return false;

  ssid = doc["ssid"].as<String>();
  pass = doc["password"].as<String>();
  return true;
}

void sendJsonResponse(const ArduinoJson::JsonDocument& doc)
{
  ESP8266WebServer& server = getWebServer();
  // Tell ESP8266WebServer we will stream content
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, F("application/json"), "");  // commit headers

  // ---- CRITICAL ESP8266 SEQUENCE ----
  // code below exists because serializeJson(doc, server) doesn't work for the ESP8266 libraries
  size_t jsonSize = measureJson(doc);
  // Allocate buffer on heap
  std::unique_ptr<char[]> buffer(new char[jsonSize + 1]);
  if (!buffer) {
    server.sendContent("{\"error\":\"out of memory\"}");
    server.sendContent("");
    return;
  }
  
  size_t written = serializeJson(doc, buffer.get(), jsonSize + 1);
  server.sendContent(buffer.get(), written);
  server.sendContent("");
}

// this methods sets up the LittelFS filesystem, we use it to store our state (time and schedule)
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
  ArduinoOTA.setHostname(hostname);      
  ArduinoOTA.setPassword(otaPassword);   

  ArduinoOTA.onStart([]() {
    // Safety: ensure outputs go OFF
    suppressOperation(true, SUPR_UPDATE);
    Serial.println("OTA update started");
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("OTA update finished");
    // restore the operation (usually a reboot has happend to this wouldnt be necessary)
    suppressOperation(false, SUPR_UPDATE);
  });

  ArduinoOTA.onError([](ota_error_t error) {
    suppressOperation(true, SUPR_UPDATE);
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
