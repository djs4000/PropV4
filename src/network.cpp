#include "network.h"

#include "game_config.h"
#include "state_machine.h"
#include "time_sync.h"
#include "util.h"
#include "wifi_config.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <WebServer.h>

#if defined(APP_DEBUG) && defined(API_DEBUG_LOGGING)
// Enable verbose API POST logging when both APP_DEBUG and API_DEBUG_LOGGING are defined.
#define API_DEBUG_ENABLED 1
#else
#define API_DEBUG_ENABLED 0
#endif

namespace network {

// Internal state cached for network interactions. Variables are file-local via
// `static` to avoid exposing them outside this translation unit.
struct RuntimeConfig {
  String wifiSsid;
  String wifiPass;
  String defuseCode;
  uint32_t bombDurationMs;
  String apiEndpoint;
};

static RuntimeConfig runtimeConfig = {String(DEFAULT_WIFI_SSID), String(DEFAULT_WIFI_PASS),
                                      String(DEFAULT_DEFUSE_CODE), DEFAULT_BOMB_DURATION_MS,
                                      String(DEFAULT_API_ENDPOINT)};

enum class ApiRequestState { Idle, InFlight };

static uint64_t lastSuccessfulApiMs = 0;
static MatchStatus remoteStatus = WaitingOnStart;
static FlameState outboundState = ON;
static uint32_t outboundTimerMs = DEFAULT_BOMB_DURATION_MS;
static uint32_t baseRemainingTimeMs = 0;
static uint64_t baseRemainingTimestampMs = 0;
static uint64_t lastApiResponseMs = 0;
static bool apiResponseReceived = false;
static uint32_t lastApiRequestStartMs = 0;
static uint32_t lastSuccessfulApiDebugMs = 0;

// Tracks the last POST attempt to maintain the configured cadence.
static uint32_t lastApiPostMs = 0;
static ApiRequestState apiRequestState = ApiRequestState::Idle;

static uint8_t wifiRetryCount = 0;
static uint32_t wifiAttemptStartMs = 0;
static bool wifiFailedPermanently = false;

static Preferences preferences;
static bool preferencesInitialized = false;

static WebServer server(80);
static bool configPortalActive = false;
static bool configPortalReconnectRequested = false;
static String configPortalSsid;
static bool webServerRunning = false;
static bool webServerRoutesConfigured = false;
static uint32_t lastWebServerServiceMs = 0;

// Timeout for each WiFi connection attempt before retrying.
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 5000;

// Forward declarations for config portal route handlers to ensure registration
// compiles before definitions later in the file.
static void handleConfigPortalGet();
static void handleConfigPortalSave();

static Preferences &getPreferences() {
  if (!preferencesInitialized) {
    preferences.begin("digital_flame", false);
    preferencesInitialized = true;
  }
  return preferences;
}

static void loadRuntimeConfigFromPrefs() {
  Preferences &prefs = getPreferences();
  runtimeConfig.wifiSsid = prefs.getString("wifi_ssid", DEFAULT_WIFI_SSID);
  runtimeConfig.wifiPass = prefs.getString("wifi_pass", DEFAULT_WIFI_PASS);
  runtimeConfig.defuseCode = prefs.getString("defuse_code", DEFAULT_DEFUSE_CODE);
  runtimeConfig.apiEndpoint = prefs.getString("api_endpoint", DEFAULT_API_ENDPOINT);
  runtimeConfig.bombDurationMs = prefs.getUInt("bomb_duration_ms", DEFAULT_BOMB_DURATION_MS);

  if (runtimeConfig.wifiSsid.isEmpty()) {
    runtimeConfig.wifiSsid = DEFAULT_WIFI_SSID;
  }
  if (runtimeConfig.apiEndpoint.isEmpty()) {
    runtimeConfig.apiEndpoint = DEFAULT_API_ENDPOINT;
  }
  if (runtimeConfig.defuseCode.isEmpty()) {
    runtimeConfig.defuseCode = DEFAULT_DEFUSE_CODE;
  }
  if (runtimeConfig.bombDurationMs == 0) {
    runtimeConfig.bombDurationMs = DEFAULT_BOMB_DURATION_MS;
  }
}

static void persistRuntimeConfig() {
  Preferences &prefs = getPreferences();
  prefs.putString("wifi_ssid", runtimeConfig.wifiSsid);
  prefs.putString("wifi_pass", runtimeConfig.wifiPass);
  prefs.putString("defuse_code", runtimeConfig.defuseCode);
  prefs.putUInt("bomb_duration_ms", runtimeConfig.bombDurationMs);
  prefs.putString("api_endpoint", runtimeConfig.apiEndpoint);
}

static void configureWebServerRoutes() {
  if (webServerRoutesConfigured) {
    return;
  }
  server.on("/", HTTP_GET, handleConfigPortalGet);
  server.on("/save", HTTP_POST, handleConfigPortalSave);
  server.onNotFound([]() { server.send(404, "text/plain", "Not found"); });

  webServerRoutesConfigured = true;
}

static void startWebServerIfNeeded() {
  configureWebServerRoutes();
  if (webServerRunning) {
    return;
  }

  server.begin();
  webServerRunning = true;
}

// Starts a single WiFi attempt without blocking the main loop.
static void startWifiAttempt() {
  wifiAttemptStartMs = millis();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  WiFi.begin(runtimeConfig.wifiSsid.c_str(), runtimeConfig.wifiPass.c_str());
#ifdef APP_DEBUG
  Serial.print("WiFi attempt ");
  Serial.print(static_cast<int>(wifiRetryCount + 1));
  Serial.print("/ ");
  Serial.println(static_cast<int>(MAX_WIFI_RETRIES));
  Serial.print("SSID: ");
  Serial.println(runtimeConfig.wifiSsid);
#endif
}

static void handleConfigPortalGet() {
  String page;
  page.reserve(1024);
  page += "<!DOCTYPE html><html><head><meta charset=\"utf-8\"><title>Digital Flame Config</title></head><body>";
  page += "<h2>Digital Flame Configuration</h2>";
  page += "<form action=\"/save\" method=\"POST\">";
  page += "<label>WiFi SSID: <input type=\"text\" name=\"wifi_ssid\" value=\"" + runtimeConfig.wifiSsid + "\"></label><br><br>";
  page += "<label>WiFi Password: <input type=\"password\" name=\"wifi_pass\" value=\"" + runtimeConfig.wifiPass + "\"></label><br><br>";
  page += "<label>Defuse Code: <input type=\"text\" name=\"defuse_code\" value=\"" + runtimeConfig.defuseCode + "\"></label><br><br>";
  page += "<label>Bomb Duration (ms): <input type=\"number\" name=\"bomb_duration_ms\" value=\"" +
          String(runtimeConfig.bombDurationMs) + "\"></label><br><br>";
  page += "<label>API Endpoint: <input type=\"text\" name=\"api_endpoint\" value=\"" + runtimeConfig.apiEndpoint +
          "\"></label><br><br>";
  page += "<button type=\"submit\">Save</button>";
  page += "</form></body></html>";

  server.send(200, "text/html", page);
}

static void handleConfigPortalSave() {
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
              "<html><body><h3>Settings saved.</h3><p>Device will reconnect using the new settings." \
              "</p></body></html>");

  configPortalReconnectRequested = true;
}

const String &getConfiguredWifiSsid() { return runtimeConfig.wifiSsid; }
const String &getConfiguredApiEndpoint() { return runtimeConfig.apiEndpoint; }
const String &getConfiguredDefuseCode() { return runtimeConfig.defuseCode; }
uint32_t getConfiguredBombDurationMs() { return runtimeConfig.bombDurationMs; }

void beginWifi() {
  loadRuntimeConfigFromPrefs();
  wifiRetryCount = 0;
  wifiFailedPermanently = false;
  configPortalActive = false;
  configPortalReconnectRequested = false;
  webServerRunning = false;
  webServerRoutesConfigured = false;
  lastSuccessfulApiMs = millis();  // Prevent false timeouts before first API call.
  startWifiAttempt();
}

void updateWifi() {
  if (configPortalActive) {
    return;  // SoftAP config portal owns the radio while active.
  }

  if (wifiFailedPermanently) {
    return;
  }

  // Successful connection ends the retry loop; keep timestamp fresh for timeout logic.
  if (WiFi.status() == WL_CONNECTED) {
    lastSuccessfulApiMs = millis();

    // Ensure the configuration web server is available on the LAN even when STA connects.
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
#ifdef APP_DEBUG
    Serial.println("WiFi failed after max retries - starting config portal");
#endif
    beginConfigPortal();
    return;
  }

  // Retry with a new non-blocking attempt.
  startWifiAttempt();
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
  // Default SoftAP IP for user guidance when the portal is starting up.
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

uint32_t getRemoteRemainingTimeMs() {
  if (!apiResponseReceived) {
    return 0;
  }

  const uint64_t now = millis();
  const uint64_t elapsed = now - baseRemainingTimestampMs;
  if (elapsed >= baseRemainingTimeMs) {
    return 0;
  }

  return static_cast<uint32_t>(baseRemainingTimeMs - elapsed);
}

bool hasReceivedApiResponse() { return apiResponseReceived; }

void setOutboundStatus(FlameState state, uint32_t timerMs) {
  outboundState = state;
  outboundTimerMs = timerMs;
}

void updateApi() {
  const uint32_t now = millis();
  if (apiRequestState == ApiRequestState::InFlight) {
    return;
  }
  if (now - lastApiPostMs < API_POST_INTERVAL_MS) {
    return;
  }
  if (!isWifiConnected()) {
    return;
  }

  lastApiPostMs = now;

  // Keep outbound state/timer in sync with the current state machine status.
  outboundState = getState();
  uint32_t timerMs = 0;
  if (outboundState == ARMED && isBombTimerActive()) {
    timerMs = getBombTimerRemainingMs();
  } else if (isGameTimerValid()) {
    timerMs = getGameTimerRemainingMs();
  }
  outboundTimerMs = timerMs;

  const uint32_t payloadNowMs = millis();
  int64_t timestampTicks = 0;
  if (time_sync::isValid()) {
    timestampTicks = time_sync::getCurrentServerTicks(payloadNowMs);
  }

  JsonDocument doc;
  doc["state"] = flameStateToString(outboundState);
  doc["timer"] = outboundTimerMs;
  doc["timestamp"] = timestampTicks;

  String payload;
  serializeJson(doc, payload);

  const ApiMode mode = getApiMode();

  if (mode == ApiMode::Disabled) {
    // Prevent timeout triggers while intentionally offline.
    lastSuccessfulApiMs = now;
    return;
  }

  struct ApiRequestGuard {
    explicit ApiRequestGuard(ApiRequestState &stateRef) : state(stateRef) { state = ApiRequestState::InFlight; }
    ~ApiRequestGuard() { state = ApiRequestState::Idle; }
    ApiRequestState &state;
  } guard(apiRequestState);

  lastApiRequestStartMs = millis();

  HTTPClient http;
  if (!http.begin(runtimeConfig.apiEndpoint)) {
#if API_DEBUG_ENABLED
    Serial.println("HTTP begin failed for API endpoint");
#endif
    if (mode == ApiMode::TestSendOnly) {
      lastSuccessfulApiMs = now;
    }
    return;
  }

  http.setTimeout(2000);
  http.addHeader("Content-Type", "application/json");
  const int httpCode = http.POST(payload);
  const uint32_t responseNow = millis();

  if (mode == ApiMode::TestSendOnly) {
    if (httpCode != HTTP_CODE_OK) {
#if API_DEBUG_ENABLED
      Serial.print("API POST failed (test mode): ");
      Serial.println(httpCode);
#endif
    }
    // Keep timeout logic from firing in this mode regardless of response.
    lastSuccessfulApiMs = responseNow;
    http.end();
    return;
  }

  // FullOnline mode: enforce strict success + JSON parsing.
  if (httpCode == HTTP_CODE_OK) {
    const String response = http.getString();
    JsonDocument respDoc;
    const DeserializationError err = deserializeJson(respDoc, response);
    if (!err) {
      const char *statusStr = respDoc["status"];
      MatchStatus parsedStatus;
      const bool statusParsed = util::parseMatchStatus(statusStr, parsedStatus);

      const JsonVariantConst timestampVariant = respDoc["timestamp"];
      if (!timestampVariant.isNull()) {
        const int64_t serverTicks = timestampVariant.as<int64_t>();
        time_sync::updateFromServer(serverTicks, responseNow);
      }

      const uint32_t remainingMs = respDoc["remaining_time_ms"] | 0;
      baseRemainingTimeMs = remainingMs;
      baseRemainingTimestampMs = responseNow;
      lastApiResponseMs = responseNow;
      apiResponseReceived = true;

      updateGameTimerFromApi(remainingMs, millis());

      if (statusParsed) {
        remoteStatus = parsedStatus;
      }

      // Treat a well-formed JSON body as a successful API interaction for timeout tracking.
      lastSuccessfulApiMs = responseNow;

      const uint32_t rttMs = responseNow - lastApiRequestStartMs;

      if (lastSuccessfulApiDebugMs != 0) {
        const uint32_t delta = responseNow - lastSuccessfulApiDebugMs;
        const uint32_t intervals = delta / API_POST_INTERVAL_MS;
        if (intervals > 1) {
#if API_DEBUG_ENABLED
          Serial.printf("[API] Missed approx %lu intervals since last success\n",
                        static_cast<unsigned long>(intervals - 1));
#endif
        }
      }

      lastSuccessfulApiDebugMs = responseNow;

#if API_DEBUG_ENABLED
      Serial.print("API status: ");
      Serial.print(statusStr ? statusStr : "<null>");
      Serial.print(" remaining_ms=");
      Serial.println(remainingMs);
      Serial.printf("[API] RTT: %lu ms\n", static_cast<unsigned long>(rttMs));
#endif
    } else {
#if API_DEBUG_ENABLED
      Serial.print("API JSON parse error: ");
      Serial.println(err.f_str());
#endif
    }
  } else {
#if API_DEBUG_ENABLED
    Serial.print("API POST failed: ");
    Serial.println(httpCode);
#endif
  }

  http.end();
}

void beginConfigPortal() {
  if (configPortalActive) {
    return;
  }

  // Stop any ongoing STA attempts and start the SoftAP.
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);

  uint8_t mac[6] = {0};
  WiFi.macAddress(mac);
  char suffix[5] = {0};
  snprintf(suffix, sizeof(suffix), "%02X%02X", mac[4], mac[5]);
  configPortalSsid = String(SOFTAP_SSID_PREFIX) + String(suffix);

  WiFi.softAP(configPortalSsid.c_str(), SOFTAP_PASSWORD);
  startWebServerIfNeeded();

  const String portalAddress = getConfigPortalAddress();

  configPortalActive = true;
  wifiFailedPermanently = false;  // Prevent ERROR state while AP is active.
#ifdef APP_DEBUG
  Serial.print("Config portal started. SSID: ");
  Serial.print(configPortalSsid);
  Serial.print(" Password: ");
  Serial.println(SOFTAP_PASSWORD);
  Serial.print("Browse to ");
  Serial.println(portalAddress);
#endif
}

void updateConfigPortal(uint32_t now, FlameState state) {
  if (!webServerRunning) {
    return;
  }

  uint32_t interval = 200;
  if (state == ACTIVE || state == ARMING || state == ARMED) {
    interval = 500;
  }

  if (now - lastWebServerServiceMs < interval) {
    return;
  }
  lastWebServerServiceMs = now;

  server.handleClient();

  if (configPortalActive && configPortalReconnectRequested) {
    configPortalReconnectRequested = false;
    server.stop();
    webServerRunning = false;
    webServerRoutesConfigured = false;  // Re-register routes after restart to avoid missing handlers.
    WiFi.softAPdisconnect(true);
    WiFi.mode(WIFI_STA);
    configPortalActive = false;
    wifiRetryCount = 0;
    wifiFailedPermanently = false;
    startWifiAttempt();
  }
}

}  // namespace network
