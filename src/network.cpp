#include "network.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>
#include <esp_timer.h>

#include "game_config.h"
#include "util.h"
#include "wifi_config.h"

namespace network {
namespace {
struct RuntimeConfig {
  String wifiSsid;
  String wifiPass;
  String defuseCode;
  uint32_t bombDurationMs;
  String apiEndpoint;
};

RuntimeConfig runtimeConfig = {String(DEFAULT_WIFI_SSID), String(DEFAULT_WIFI_PASS),
                               String(DEFAULT_DEFUSE_CODE), DEFAULT_BOMB_DURATION_MS,
                               String(DEFAULT_API_ENDPOINT)};

uint64_t lastSuccessfulApiMs = 0;
uint64_t lastApiAttemptMs = 0;
bool apiResponseReceived = false;
MatchStatus remoteStatus = WaitingOnStart;

uint8_t wifiRetryCount = 0;
uint32_t wifiAttemptStartMs = 0;
bool wifiFailedPermanently = false;
bool configPortalActive = false;
String configPortalSsid;
bool restartRequestedFromPortal = false;

Preferences preferences;
bool prefsInitialized = false;

WebServer server(80);
bool webServerRunning = false;

constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 5000;

Preferences &getPrefs() {
  if (!prefsInitialized) {
    preferences.begin("digital_flame", false);
    prefsInitialized = true;
  }
  return preferences;
}

void loadRuntimeConfig() {
  Preferences &prefs = getPrefs();
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
  Preferences &prefs = getPrefs();
  prefs.putString("wifi_ssid", runtimeConfig.wifiSsid);
  prefs.putString("wifi_pass", runtimeConfig.wifiPass);
  prefs.putString("defuse_code", runtimeConfig.defuseCode);
  prefs.putUInt("bomb_duration_ms", runtimeConfig.bombDurationMs);
  prefs.putString("api_endpoint", runtimeConfig.apiEndpoint);
}

void startWifiAttempt() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.begin(runtimeConfig.wifiSsid.c_str(), runtimeConfig.wifiPass.c_str());
  wifiAttemptStartMs = millis();
#ifdef DEBUG
  Serial.print("NET: WiFi attempt to SSID ");
  Serial.println(runtimeConfig.wifiSsid);
#endif
}

void handleConfigPortalGet() {
  String page = R"rawliteral(
    <html><body><h2>Digital Flame Config</h2>
    <form method='POST' action='/save'>
      SSID: <input name='wifi_ssid'/><br/>
      Password: <input name='wifi_pass' type='password'/><br/>
      Defuse Code: <input name='defuse_code'/><br/>
      Bomb Duration (ms): <input name='bomb_duration_ms'/><br/>
      API Endpoint: <input name='api_endpoint'/><br/>
      <input type='submit' value='Save'/>
    </form>
    </body></html>)rawliteral";
  server.send(200, "text/html", page);
}

void handleConfigPortalSave() {
  if (server.hasArg("wifi_ssid")) runtimeConfig.wifiSsid = server.arg("wifi_ssid");
  if (server.hasArg("wifi_pass")) runtimeConfig.wifiPass = server.arg("wifi_pass");
  if (server.hasArg("defuse_code")) runtimeConfig.defuseCode = server.arg("defuse_code");
  if (server.hasArg("bomb_duration_ms")) runtimeConfig.bombDurationMs = server.arg("bomb_duration_ms").toInt();
  if (server.hasArg("api_endpoint")) runtimeConfig.apiEndpoint = server.arg("api_endpoint");

  persistRuntimeConfig();
  restartRequestedFromPortal = true;
  server.send(200, "text/plain", "Saved. Device will reconnect.");
}

void ensureWebServer() {
  if (webServerRunning) return;
  server.on("/", HTTP_GET, handleConfigPortalGet);
  server.on("/save", HTTP_POST, handleConfigPortalSave);
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });
  server.begin();
  webServerRunning = true;
}

void startConfigPortalInternal() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  uint8_t mac[6];
  WiFi.macAddress(mac);
  char suffix[5];
  snprintf(suffix, sizeof(suffix), "%02X%02X", mac[4], mac[5]);
  configPortalSsid = String(SOFTAP_SSID_PREFIX) + suffix;
  WiFi.softAP(configPortalSsid.c_str(), SOFTAP_PASSWORD);
  configPortalActive = true;
  wifiFailedPermanently = false;
  ensureWebServer();
#ifdef DEBUG
  Serial.print("NET: Config portal SSID ");
  Serial.print(configPortalSsid);
  Serial.print(" pass ");
  Serial.println(SOFTAP_PASSWORD);
#endif
}

void stopConfigPortalIfRequested() {
  if (configPortalActive && restartRequestedFromPortal) {
    restartRequestedFromPortal = false;
    server.stop();
    webServerRunning = false;
    configPortalActive = false;
    wifiRetryCount = 0;
    wifiFailedPermanently = false;
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    startWifiAttempt();
  }
}

bool parseApiResponse(const String &body, uint32_t now) {
  DynamicJsonDocument doc(256);
  if (deserializeJson(doc, body) != DeserializationError::Ok) {
    return false;
  }

  const char *statusStr = doc["status"];
  MatchStatus parsedStatus;
  if (!util::parseMatchStatus(statusStr, parsedStatus)) {
    return false;
  }

  remoteStatus = parsedStatus;
  apiResponseReceived = true;
  lastSuccessfulApiMs = now;
  return true;
}

}  // namespace

void beginWifi() {
  loadRuntimeConfig();
  lastSuccessfulApiMs = millis();
  ensureWebServer();
  startWifiAttempt();
}

void updateWifi() {
  if (configPortalActive) {
    return;
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiRetryCount = 0;
    return;
  }

  const uint32_t now = millis();
  if (now - wifiAttemptStartMs < WIFI_CONNECT_TIMEOUT_MS) {
    return;
  }

  wifiRetryCount++;
  if (wifiRetryCount >= MAX_WIFI_RETRIES) {
    wifiFailedPermanently = true;
    startConfigPortalInternal();
    return;
  }

  startWifiAttempt();
}

void updateApi() {
  if (!isWifiConnected()) {
    return;
  }

  const uint32_t now = millis();
  if (now - lastApiAttemptMs < API_POST_INTERVAL_MS) {
    return;
  }
  lastApiAttemptMs = now;

  DynamicJsonDocument doc(128);
  doc["state"] = state_machine::flameStateToString(state_machine::getState());
  doc["timer"] = state_machine::getArmedTimerMs();
  doc["timestamp"] = esp_timer_get_time();

  String payload;
  serializeJson(doc, payload);

  HTTPClient http;
  if (!http.begin(runtimeConfig.apiEndpoint)) {
#ifdef DEBUG
    Serial.println("NET: http.begin failed");
#endif
    return;
  }

  http.addHeader("Content-Type", "application/json");
  const int code = http.POST(payload);

  if (code == HTTP_CODE_OK) {
    const String body = http.getString();
    if (!parseApiResponse(body, now)) {
#ifdef DEBUG
      Serial.println("NET: API parse failed");
#endif
    }
  } else {
#ifdef DEBUG
    Serial.print("NET: API POST failed code=");
    Serial.println(code);
#endif
  }

  http.end();
}

void updateConfigPortal() {
  if (webServerRunning) {
    server.handleClient();
  }
  stopConfigPortalIfRequested();
}

bool isWifiConnected() { return WiFi.status() == WL_CONNECTED; }

bool hasWifiFailedPermanently() { return wifiFailedPermanently && !configPortalActive; }

bool isConfigPortalActive() { return configPortalActive; }

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
  if (!isWifiConnected()) return String("");
  return WiFi.localIP().toString();
}

const String &getConfiguredWifiSsid() { return runtimeConfig.wifiSsid; }

const String &getConfiguredApiEndpoint() { return runtimeConfig.apiEndpoint; }

const String &getConfiguredDefuseCode() { return runtimeConfig.defuseCode; }

uint32_t getConfiguredBombDurationMs() { return runtimeConfig.bombDurationMs; }

uint64_t getLastSuccessfulApiMs() { return lastSuccessfulApiMs; }

MatchStatus getRemoteMatchStatus() { return remoteStatus; }

bool hasReceivedApiResponse() { return apiResponseReceived; }

bool isApiWarning() {
  const uint64_t now = millis();
  return apiResponseReceived && (now - lastSuccessfulApiMs > API_POST_INTERVAL_MS * 3);
}

void beginConfigPortal() { startConfigPortalInternal(); }

}  // namespace network
