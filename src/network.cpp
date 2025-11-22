#include "network.h"

#include "game_config.h"
#include "state_machine.h"
#include "util.h"
#include "wifi_config.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_timer.h>

namespace network {
namespace {
struct RuntimeConfig {
  String wifiSsid;
  String wifiPass;
  String defuseCode;
  uint32_t bombDurationMs;
  String apiEndpoint;
};

RuntimeConfig runtimeConfig{String(DEFAULT_WIFI_SSID), String(DEFAULT_WIFI_PASS),
                            String(DEFAULT_DEFUSE_CODE), DEFAULT_BOMB_DURATION_MS,
                            String(DEFAULT_API_ENDPOINT)};

uint64_t lastSuccessfulApiMs = 0;
uint64_t lastApiPostMs = 0;
MatchStatus remoteStatus = WaitingOnStart;
bool apiResponseReceived = false;

uint8_t wifiRetryCount = 0;
uint32_t wifiAttemptStartMs = 0;
bool wifiFailedPermanently = false;

Preferences preferences;
bool preferencesInitialized = false;

WebServer server(80);
bool configPortalActive = false;
bool configPortalReconnectRequested = false;
String configPortalSsid;
bool webServerRunning = false;
bool webServerRoutesConfigured = false;

constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 5000;

void configureRoutes();
void handleConfigPortalGet();
void handleConfigPortalSave();

Preferences &getPreferences() {
  if (!preferencesInitialized) {
    preferences.begin("digital_flame", false);
    preferencesInitialized = true;
  }
  return preferences;
}

void loadRuntimeConfig() {
  Preferences &prefs = getPreferences();
  runtimeConfig.wifiSsid = prefs.getString("wifi_ssid", DEFAULT_WIFI_SSID);
  runtimeConfig.wifiPass = prefs.getString("wifi_pass", DEFAULT_WIFI_PASS);
  runtimeConfig.defuseCode = prefs.getString("defuse_code", DEFAULT_DEFUSE_CODE);
  runtimeConfig.apiEndpoint = prefs.getString("api_endpoint", DEFAULT_API_ENDPOINT);
  runtimeConfig.bombDurationMs = prefs.getUInt("bomb_duration_ms", DEFAULT_BOMB_DURATION_MS);

  if (runtimeConfig.wifiSsid.isEmpty()) runtimeConfig.wifiSsid = DEFAULT_WIFI_SSID;
  if (runtimeConfig.apiEndpoint.isEmpty()) runtimeConfig.apiEndpoint = DEFAULT_API_ENDPOINT;
  if (runtimeConfig.defuseCode.isEmpty()) runtimeConfig.defuseCode = DEFAULT_DEFUSE_CODE;
  if (runtimeConfig.bombDurationMs == 0) runtimeConfig.bombDurationMs = DEFAULT_BOMB_DURATION_MS;
}

void persistRuntimeConfig() {
  Preferences &prefs = getPreferences();
  prefs.putString("wifi_ssid", runtimeConfig.wifiSsid);
  prefs.putString("wifi_pass", runtimeConfig.wifiPass);
  prefs.putString("defuse_code", runtimeConfig.defuseCode);
  prefs.putUInt("bomb_duration_ms", runtimeConfig.bombDurationMs);
  prefs.putString("api_endpoint", runtimeConfig.apiEndpoint);
}

void configureRoutes() {
  if (webServerRoutesConfigured) {
    return;
  }
  server.on("/", HTTP_GET, handleConfigPortalGet);
  server.on("/save", HTTP_POST, handleConfigPortalSave);
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  webServerRoutesConfigured = true;
}

void startWebServerIfNeeded() {
  configureRoutes();
  if (webServerRunning) {
    return;
  }
  server.begin();
  webServerRunning = true;
}

void startWifiAttempt() {
  wifiAttemptStartMs = millis();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  WiFi.begin(runtimeConfig.wifiSsid.c_str(), runtimeConfig.wifiPass.c_str());
#ifdef DEBUG
  Serial.print("NET: WiFi attempt ");
  Serial.print(static_cast<int>(wifiRetryCount + 1));
  Serial.print("/");
  Serial.println(static_cast<int>(MAX_WIFI_RETRIES));
#endif
}

void handleConfigPortalGet() {
  String page;
  page.reserve(1024);
  page += "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>Digital Flame</title></head><body>";
  page += "<h2>Digital Flame Configuration</h2>";
  page += "<form action=\"/save\" method=\"POST\">";
  page += "<label>WiFi SSID: <input type=\"text\" name=\"wifi_ssid\" value=\"" + runtimeConfig.wifiSsid + "\"></label><br><br>";
  page += "<label>WiFi Password: <input type=\"password\" name=\"wifi_pass\" value=\"" + runtimeConfig.wifiPass + "\"></label><br><br>";
  page += "<label>Defuse Code: <input type=\"text\" name=\"defuse_code\" value=\"" + runtimeConfig.defuseCode + "\"></label><br><br>";
  page += "<label>Bomb Duration (ms): <input type=\"number\" name=\"bomb_duration_ms\" value=\"" +
          String(runtimeConfig.bombDurationMs) + "\"></label><br><br>";
  page += "<label>API Endpoint: <input type=\"text\" name=\"api_endpoint\" value=\"" + runtimeConfig.apiEndpoint +
          "\"></label><br><br>";
  page += "<button type=\"submit\">Save</button></form></body></html>";
  server.send(200, "text/html", page);
}

void handleConfigPortalSave() {
  const String ssid = server.arg("wifi_ssid");
  const String pass = server.arg("wifi_pass");
  const String defuse = server.arg("defuse_code");
  const String endpoint = server.arg("api_endpoint");
  const uint32_t duration = static_cast<uint32_t>(server.arg("bomb_duration_ms").toInt());

  if (ssid.isEmpty()) {
    server.send(400, "text/plain", "SSID cannot be empty.");
    return;
  }

  runtimeConfig.wifiSsid = ssid;
  runtimeConfig.wifiPass = pass;
  runtimeConfig.defuseCode = defuse.isEmpty() ? String(DEFAULT_DEFUSE_CODE) : defuse;
  runtimeConfig.apiEndpoint = endpoint.isEmpty() ? String(DEFAULT_API_ENDPOINT) : endpoint;
  runtimeConfig.bombDurationMs = (duration == 0) ? DEFAULT_BOMB_DURATION_MS : duration;

  persistRuntimeConfig();

  server.send(200, "text/html",
              "<html><body><h3>Settings saved.</h3><p>Reconnecting with new settings.</p></body></html>");

  configPortalReconnectRequested = true;
}

void beginConfigPortal() {
  if (configPortalActive) {
    return;
  }

  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);

  uint8_t mac[6] = {0};
  WiFi.macAddress(mac);
  char suffix[5] = {0};
  snprintf(suffix, sizeof(suffix), "%02X%02X", mac[4], mac[5]);
  configPortalSsid = String(SOFTAP_SSID_PREFIX) + String(suffix);

  WiFi.softAP(configPortalSsid.c_str(), SOFTAP_PASSWORD);
  startWebServerIfNeeded();

  configPortalActive = true;
  wifiFailedPermanently = false;
#ifdef DEBUG
  Serial.print("NET: Config portal SSID ");
  Serial.print(configPortalSsid);
  Serial.print(" pass ");
  Serial.println(SOFTAP_PASSWORD);
#endif
}
}

void beginWifi() {
  loadRuntimeConfig();
  wifiRetryCount = 0;
  wifiFailedPermanently = false;
  configPortalActive = false;
  configPortalReconnectRequested = false;
  webServerRunning = false;
  webServerRoutesConfigured = false;
  lastSuccessfulApiMs = 0;
  startWifiAttempt();
}

void updateWifi() {
  if (configPortalActive) {
    return;
  }

  if (wifiFailedPermanently) {
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    startWebServerIfNeeded();
    return;
  }

  const uint32_t now = millis();
  const bool attemptTimedOut = now - wifiAttemptStartMs >= WIFI_CONNECT_TIMEOUT_MS;
  if (!attemptTimedOut) {
    return;
  }

  wifiRetryCount++;
  if (wifiRetryCount >= MAX_WIFI_RETRIES) {
    wifiFailedPermanently = true;
    beginConfigPortal();
    return;
  }

  startWifiAttempt();
}

bool isWifiConnected() { return WiFi.status() == WL_CONNECTED; }

bool hasWifiFailedPermanently() { return wifiFailedPermanently && !configPortalActive; }

bool isConfigPortalActive() { return configPortalActive; }

const String &getConfiguredWifiSsid() { return runtimeConfig.wifiSsid; }
const String &getConfiguredDefuseCode() { return runtimeConfig.defuseCode; }
const String &getConfiguredApiEndpoint() { return runtimeConfig.apiEndpoint; }
uint32_t getConfiguredBombDurationMs() { return runtimeConfig.bombDurationMs; }

String getConfigPortalSsid() { return configPortalSsid; }

String getConfigPortalPassword() { return String(SOFTAP_PASSWORD); }

String getConfigPortalAddress() {
  if (configPortalActive) {
    return String("http://") + WiFi.softAPIP().toString();
  }
  if (isWifiConnected()) {
    return String("http://") + WiFi.localIP().toString();
  }
  return String("http://192.168.4.1");
}

String getWifiIpString() {
  if (!isWifiConnected()) {
    return String("");
  }
  return WiFi.localIP().toString();
}

uint64_t getLastSuccessfulApiMs() { return lastSuccessfulApiMs; }

MatchStatus getRemoteMatchStatus() { return remoteStatus; }

bool hasReceivedApiResponse() { return apiResponseReceived; }

bool isNetworkWarningActive() {
  const uint64_t now = millis();
  return isWifiConnected() && apiResponseReceived && (now - lastSuccessfulApiMs > API_POST_INTERVAL_MS * 3);
}

void updateApi() {
  const uint32_t now = millis();
  if (!isWifiConnected() || configPortalActive) {
    return;
  }
  if (now - lastApiPostMs < API_POST_INTERVAL_MS) {
    return;
  }
  lastApiPostMs = now;

  DynamicJsonDocument doc(128);
  doc["state"] = flameStateToString(getState());
  doc["timer"] = 0;
  doc["timestamp"] = esp_timer_get_time();

  String payload;
  serializeJson(doc, payload);

  HTTPClient http;
  if (!http.begin(runtimeConfig.apiEndpoint)) {
#ifdef DEBUG
    Serial.println("NET: HTTP begin failed");
#endif
    return;
  }

  http.addHeader("Content-Type", "application/json");
  const int httpCode = http.POST(payload);

  if (httpCode == HTTP_CODE_OK) {
    const String response = http.getString();
    DynamicJsonDocument respDoc(256);
    if (deserializeJson(respDoc, response) == DeserializationError::Ok) {
      const char *statusStr = respDoc["status"];
      MatchStatus parsedStatus;
      if (util::parseMatchStatus(statusStr, parsedStatus)) {
        remoteStatus = parsedStatus;
        apiResponseReceived = true;
        lastSuccessfulApiMs = now;
      }
    }
  }

  http.end();
}

void updateConfigPortal() {
  if (!webServerRunning) {
    return;
  }

  server.handleClient();

  if (configPortalActive && configPortalReconnectRequested) {
    configPortalReconnectRequested = false;
    server.stop();
    webServerRunning = false;
    webServerRoutesConfigured = false;
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    configPortalActive = false;
    wifiRetryCount = 0;
    wifiFailedPermanently = false;
    startWifiAttempt();
  }
}

}  // namespace network

