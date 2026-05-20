#include "platform.hpp"
#include "control.hpp"

#define DEFAULT_APNAME "greenhouse"
#define DEFAULT_APPASS "Tomatos#123"


// ---------------- Server ----------------
ESP8266WebServer server(80);
ESP8266WebServer& getWebServer()
{
  return server;
}

//-------------- helper methods -----------

String ipToString(const IPAddress& ip) {
  return ip.toString();
}

IPAddress stringToIP(const String& s) {
  IPAddress ip;
  ip.fromString(s);
  return ip;
}

// --------------- Accesspoint mode ------
void startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(DEFAULT_APNAME, DEFAULT_APPASS);

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

bool saveConfig(const NetworkConfig& cfg) {
  StaticJsonDocument<512> doc;
  doc["version"] = 1; // for future use
  doc["dhcp"] = cfg.dhcp;

  doc["ip"]      = ipToString(cfg.ip);
  doc["gateway"] = ipToString(cfg.gateway);
  doc["subnet"]  = ipToString(cfg.subnet);

  doc["ap_enable"]   = cfg.ap_enable;
  doc["ap_ssid"]     = cfg.ap_ssid;
  doc["ap_password"] = cfg.ap_password;

  doc["sta_ssid"]     = cfg.sta_ssid;
  doc["sta_password"] = cfg.sta_password;

  File f = LittleFS.open("/config.json", "w");
  if (!f) return false;

  serializeJson(doc, f);
  f.close();

  return true;
}

bool loadConfig(NetworkConfig& cfg) {
  if (!LittleFS.exists("/config.json")) return false;

  File f = LittleFS.open("/config.json", "r");
  if (!f) return false;

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) return false;

  cfg.dhcp = doc["dhcp"] | true;

  cfg.ip      = stringToIP(doc["ip"] | "192.168.1.100");
  cfg.gateway = stringToIP(doc["gateway"] | "192.168.1.1");
  cfg.subnet  = stringToIP(doc["subnet"] | "255.255.255.0");

  cfg.ap_enable   = doc["ap_enable"] | false;
  cfg.ap_ssid     = doc["ap_ssid"] | DEFAULT_APNAME;
  cfg.ap_password = doc["ap_password"] | DEFAULT_APPASS;

  cfg.sta_ssid     = doc["sta_ssid"] | "";
  cfg.sta_password = doc["sta_password"] | "";

  return true;
}

bool clearConfig() {
  if (LittleFS.exists("/config.json")) {
    return LittleFS.remove("/config.json");
  }
  return true;
}

void applyNetwork(NetworkConfig& cfg) {
  WiFi.mode(WIFI_OFF);
  delay(200);

  if (cfg.ap_enable) {
    WiFi.mode(WIFI_AP_STA);
  } else {
    WiFi.mode(WIFI_STA);
  }

  // STA
  if (!cfg.dhcp) {
    WiFi.config(cfg.ip, cfg.gateway, cfg.subnet);
  }

  WiFi.begin(cfg.sta_ssid.c_str(), cfg.sta_password.c_str());

  // AP
  if (cfg.ap_enable) {
    WiFi.softAP(cfg.ap_ssid.c_str(), cfg.ap_password.c_str());
  }
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
