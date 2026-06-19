#include "core_esp8266_features.h"
#include "definitions.hpp"
//#include "arch/cc.h"
#include "platform.hpp"
#include "secrets.hpp"
#include "control.hpp"

#define CONFIG_FILEPATH "/config.json"

static NetworkConfig currentConfig;
static bool safemode = false;
static bool wifiNetworkHasBeenSeen = false;
static unsigned long wifiConnectStartedAtMilis = 0;
static bool wifiConnectStarted = false;


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
void setDefaults(NetworkConfig& cfg) {
  cfg.dhcp = false;
  cfg.ip      = stringToIP(DEF_IP);
  cfg.gateway = stringToIP(DEF_GATEWAY);
  cfg.subnet  = stringToIP(DEF_SUBNETMASK);

  cfg.ap_enable   = true;
  cfg.ap_ssid     = DEF_APNAME;
  cfg.ap_password = DEF_APPASS;

  cfg.sta_ssid     = "";
  cfg.sta_password = "";

  cfg.hostname = DEF_HOSTNAME;
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

  File f = LittleFS.open(CONFIG_FILEPATH, "w");
  if (!f) return false;

  size_t written = serializeJson(doc, f);
  f.flush();
  if (f.getWriteError() != 0 )
  {
    Serial.println("Error: saving configuration failed!! (write error)");
    f.close();
    return false;
  }
  f.close();

  if (written == 0)
  {
    Serial.println("Error: saving configuration failed!! (no bytes written)");
    return false;
  }
  
  Serial.println("Configuration saved.");
  Serial.println(cfg.sta_ssid);

  return true;
}

bool loadConfig(NetworkConfig& cfg) {
  if (!LittleFS.exists(CONFIG_FILEPATH)){
    Serial.println("DEBUG: NO Configuration file found.");
    return false;
  }
  File f = LittleFS.open(CONFIG_FILEPATH, "r");
  if (!f) {
    Serial.println("ERROR: Unable to open configuration file.");
    return false;
  }
  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, f);
  f.close();

  if (err) {
    Serial.print("ERROR: Deserialization error: ");
    Serial.println(err.c_str());
    return false;
  }

  cfg.dhcp = doc["dhcp"] | true;

  cfg.ip      = stringToIP(doc["ip"] | DEF_IP);
  cfg.gateway = stringToIP(doc["gateway"] | DEF_GATEWAY);
  cfg.subnet  = stringToIP(doc["subnet"] | DEF_SUBNETMASK);

  cfg.ap_enable   = doc["ap_enable"] | false;
  cfg.ap_ssid     = doc["ap_ssid"] | DEF_APNAME;
  cfg.ap_password = doc["ap_password"] | DEF_APPASS;

  cfg.sta_ssid     = doc["sta_ssid"] | "";
  cfg.sta_password = doc["sta_password"] | "";
  cfg.hostname = DEF_HOSTNAME;

  Serial.println("Configuration loaded.");
  Serial.println(cfg.sta_ssid);

  return true;
}

bool clearConfig() {
  setDefaults(currentConfig);
  if (LittleFS.exists(CONFIG_FILEPATH)) {
    return LittleFS.remove(CONFIG_FILEPATH);
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

  if (cfg.ap_enable || safemode) {
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
  wifiConnectStartedAtMilis = millis();
  wifiConnectStarted = true;
  wifiNetworkHasBeenSeen = false;

  WiFi.setAutoReconnect(!cfg.ap_enable);
    
  // AP
  if (cfg.ap_enable) {
    WiFi.softAP(cfg.ap_ssid.c_str(), cfg.ap_password.c_str());
    Serial.print("Start accesspoint:" );
    Serial.println(cfg.ap_ssid);
  }
  else{
    Serial.println("Connecting to WiFi...");
    int retries = 4;
    while ((WiFi.status() != WL_CONNECTED) && (retries-- > 0)) {
      delay(250);
      Serial.print(".");
    }
    if (WiFi.status() == WL_CONNECTED){
      wifiNetworkHasBeenSeen = true;
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
  pinMode(DEFAULT_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH); // LED off initially
  setupFileSystem();
  
  setupWiFi();
#ifdef OTA_ENABLED
  setupOta();
#endif
}

void performPlatformHandling(){  
 // Handle wificonnection
  if (wifiConnectStarted && !wifiNetworkHasBeenSeen && !safemode){
    if (WiFi.status() != WL_CONNECTED){
      if (millis() - wifiConnectStartedAtMilis > timeBeforeSafemodeFallbackMillis){
        // we tried to connect long enough, enter AP mode as safety
        safemode = true;
        Serial.println("Connection timeout, entering safemode (AP-Mode)!!");
        NetworkConfig cfg = getConfig();
        applyNetwork(cfg);
      }
    } else wifiNetworkHasBeenSeen = true;
  }
#ifdef OTA_ENABLED
  ArduinoOTA.handle();
  MDNS.update();
#endif  
  if (digitalRead(DEFAULT_PIN) == LOW){
    bool reset = true;
    for (int i=0; i< 20; i++){
      delay(500);
      if (digitalRead(DEFAULT_PIN) == HIGH){ 
        reset = false;
        break;
      }
    }
    if (reset){
      if (clearConfig()){
        delay(1000);
        ESP.restart();
      }
    } else { // Shorter clicks start or stop a manual session
      toggleManualOverride(DEF_MANUAL_DURATION, MA_SWITCH);
    }
  }
}

bool isConnectedToWiFi(){
  if (!getConfig().ap_enable)
    return WiFi.status() == WL_CONNECTED;
  else return true;
}
