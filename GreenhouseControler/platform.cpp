#include "arch/cc.h"
#include "platform.hpp"
#include "control.hpp"

#define DEFAULT_APNAME "greenhouse"
#define DEFAULT_APPASS "Tomatos#123"
#define DEFAULT_IP "192.168.1.100"
#define DEFAULT_SUBNETMASK "255.255.255.0"
#define DEFAULT_GATEWAY "192.168.1.1"
#define DEFAULT_HOSTNAME "greenhouse"

static NetworkConfig currentConfig;

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

// --------------- Network helpers ------
void startAPMode() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP(DEFAULT_APNAME, DEFAULT_APPASS);

  Serial.println("AP Mode started");
  Serial.println(WiFi.softAPIP());
}

void setDefaults(NetworkConfig& cfg) {
  cfg.dhcp = false;
  cfg.ip      = stringToIP(DEFAULT_IP);
  cfg.gateway = stringToIP(DEFAULT_GATEWAY);
  cfg.subnet  = stringToIP(DEFAULT_SUBNETMASK);

  cfg.ap_enable   = true;
  cfg.ap_ssid     = DEFAULT_APNAME;
  cfg.ap_password = DEFAULT_APPASS;

  cfg.sta_ssid     = DEFAULT_APNAME;
  cfg.sta_password = DEFAULT_APPASS;

  cfg.hostname = DEFAULT_HOSTNAME;
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

  Serial.println("Configuration saved.");
  Serial.println(cfg.sta_ssid);

  return true;
}

bool loadConfig(NetworkConfig& cfg) {
  if (!LittleFS.exists("/config.json")){
    return false;
  }
  File f = LittleFS.open("/config.json", "r");
  if (!f) {
    return false;
  }
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) return false;

  cfg.dhcp = doc["dhcp"] | true;

  cfg.ip      = stringToIP(doc["ip"] | DEFAULT_IP);
  cfg.gateway = stringToIP(doc["gateway"] | DEFAULT_GATEWAY);
  cfg.subnet  = stringToIP(doc["subnet"] | DEFAULT_SUBNETMASK);

  cfg.ap_enable   = doc["ap_enable"] | false;
  cfg.ap_ssid     = doc["ap_ssid"] | DEFAULT_APNAME;
  cfg.ap_password = doc["ap_password"] | DEFAULT_APPASS;

  cfg.sta_ssid     = doc["sta_ssid"] | "";
  cfg.sta_password = doc["sta_password"] | "";
  cfg.hostname = DEFAULT_HOSTNAME;

  return true;
}

bool clearConfig() {
  setDefaults(currentConfig);
  if (LittleFS.exists("/config.json")) {
    return LittleFS.remove("/config.json");
  }
  return true;
}

const NetworkConfig getConfig(){
  return currentConfig;
}

bool setConfig(const NetworkConfig& cfg){
  currentConfig = cfg;
  return saveConfig(currentConfig);
}

void applyNetwork(NetworkConfig& cfg) {
  WiFi.mode(WIFI_OFF);
  delay(200);

  if (cfg.ap_enable) {
    WiFi.mode(WIFI_AP_STA);
    Serial.println("AP Enabled, entering mode WIFI_AP_STA");
  } else {
    WiFi.mode(WIFI_STA);
    Serial.println("AP Disabled, entering mode WIFI_STA");
  }

  WiFi.setHostname(cfg.hostname.c_str());
  
  // STA
  if (!cfg.dhcp) {
    Serial.println("DHCP Disabled, static IP:");
    Serial.println(ipToString(cfg.ip));
    Serial.println(ipToString(cfg.gateway));
    Serial.println(ipToString(cfg.subnet));
    WiFi.config(cfg.ip, cfg.gateway, cfg.subnet);
  } else Serial.println("DHCP Enabled");

  WiFi.begin(cfg.sta_ssid.c_str(), cfg.sta_password.c_str());

  WiFi.setAutoReconnect(!cfg.ap_enable);
    
  // AP
  if (cfg.ap_enable) {
    WiFi.softAP(cfg.ap_ssid.c_str(), cfg.ap_password.c_str());
    Serial.println("Start accesspoint:" );
    Serial.println(cfg.ap_ssid);
    Serial.println(cfg.ap_password);
  }
  else{
    Serial.println("Connecting to WiFi...");
    int retries = 10;
    while ((WiFi.status() != WL_CONNECTED) && (retries-- > 0)) {
      delay(250);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED){
      Serial.println();
      Serial.print("Connected to ");
      Serial.println(cfg.sta_ssid.c_str());
      Serial.print("IP address: ");
      Serial.println(WiFi.localIP());
    }
    else {
      Serial.println();
      Serial.println("Connection failed to start, continuing without internet!!");
    }
  }
}

// ---------------- Setup ----------------
static void setupWiFi() {
  if (!loadConfig(currentConfig)){
    setDefaults(currentConfig);
  }
  applyNetwork(currentConfig);
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

const char otaPassword[] = "green19700926#OTA";
static void setupOta()
{
  ArduinoOTA.setHostname(getConfig().hostname.c_str());      
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

void initPlatform(){
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // LED off initially
  
  setupWiFi();
#ifdef OTA_ENABLED
  setupOta();
#endif
  setupFileSystem();
}

void performPlatformHandling(){  
#ifdef OTA_ENABLED
  ArduinoOTA.handle();
#endif  
}

bool isConnectedToWiFi(){
  if (!getConfig().ap_enable)
    return WiFi.status() == WL_CONNECTED;
  else return true;
}
