#include "network.h"

#include "game_config.h"
#include "state_machine.h"
#include "util.h"
#include "wifi_config.h"

#include <ArduinoJson.h>
#include <HTTPClient.h>

namespace network {

// Internal state cached for network interactions. Variables are file-local via
// `static` to avoid exposing them outside this translation unit.
static uint64_t lastSuccessfulApiMs = 0;
static MatchStatus remoteStatus = WaitingOnStart;
static FlameState outboundState = ON;
static uint32_t outboundTimerMs = DEFAULT_BOMB_DURATION_MS;
static uint32_t baseRemainingTimeMs = 0;
static uint64_t baseRemainingTimestampMs = 0;
static uint64_t lastApiResponseMs = 0;
static bool apiResponseReceived = false;

// Tracks the last POST attempt to maintain the configured cadence.
static uint32_t lastApiPostMs = 0;

static uint8_t wifiRetryCount = 0;
static uint32_t wifiAttemptStartMs = 0;
static bool wifiFailedPermanently = false;

// Timeout for each WiFi connection attempt before retrying.
static constexpr uint32_t WIFI_CONNECT_TIMEOUT_MS = 5000;

// Starts a single WiFi attempt without blocking the main loop.
static void startWifiAttempt() {
  wifiAttemptStartMs = millis();
  WiFi.mode(WIFI_STA);
  WiFi.disconnect(true);
  WiFi.begin(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASS);
#ifdef DEBUG
  Serial.print("WiFi attempt ");
  Serial.print(static_cast<int>(wifiRetryCount + 1));
  Serial.print("/ ");
  Serial.println(static_cast<int>(MAX_WIFI_RETRIES));
#endif
}

void beginWifi() {
  wifiRetryCount = 0;
  wifiFailedPermanently = false;
  lastSuccessfulApiMs = millis();  // Prevent false timeouts before first API call.
  startWifiAttempt();
}

void updateWifi() {
  if (wifiFailedPermanently) {
    return;
  }

  // Successful connection ends the retry loop; keep timestamp fresh for timeout logic.
  if (WiFi.status() == WL_CONNECTED) {
    lastSuccessfulApiMs = millis();
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
#ifdef DEBUG
    Serial.println("WiFi failed after max retries");
#endif
    WiFi.disconnect(true);
    return;
  }

  // Retry with a new non-blocking attempt.
  startWifiAttempt();
}

bool isWifiConnected() { return WiFi.status() == WL_CONNECTED; }

bool hasWifiFailedPermanently() { return wifiFailedPermanently; }

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
  if (now - lastApiPostMs < API_POST_INTERVAL_MS) {
    return;
  }
  lastApiPostMs = now;

  // Keep outbound state/timer in sync with the current state machine status.
  outboundState = getState();

  DynamicJsonDocument doc(128);
  doc["state"] = flameStateToString(outboundState);
  doc["timer"] = outboundTimerMs;  // Placeholder until real timers are wired up.
  doc["timestamp"] = 0;           // Placeholder; real clock will be integrated later.

  String payload;
  serializeJson(doc, payload);

  const ApiMode mode = getApiMode();

  if (mode == ApiMode::Disabled) {
#ifdef DEBUG
    Serial.print("API disabled; payload: ");
    Serial.println(payload);
#endif
    // Prevent timeout triggers while intentionally offline.
    lastSuccessfulApiMs = now;
    return;
  }

  if (!isWifiConnected()) {
    return;
  }

  HTTPClient http;
  if (!http.begin(DEFAULT_API_ENDPOINT)) {
#ifdef DEBUG
    Serial.println("HTTP begin failed for API endpoint");
#endif
    if (mode == ApiMode::TestSendOnly) {
      lastSuccessfulApiMs = now;
    }
    return;
  }

  http.addHeader("Content-Type", "application/json");
  const int httpCode = http.POST(payload);

  if (mode == ApiMode::TestSendOnly) {
    if (httpCode != HTTP_CODE_OK) {
#ifdef DEBUG
      Serial.print("API POST failed (test mode): ");
      Serial.println(httpCode);
#endif
    }
    // Keep timeout logic from firing in this mode regardless of response.
    lastSuccessfulApiMs = now;
    http.end();
    return;
  }

  // FullOnline mode: enforce strict success + JSON parsing.
  if (httpCode == HTTP_CODE_OK) {
    const String response = http.getString();
    DynamicJsonDocument respDoc(256);
    const DeserializationError err = deserializeJson(respDoc, response);
    if (!err) {
      const char *statusStr = respDoc["status"];
      MatchStatus parsedStatus;
      const bool statusParsed = util::parseMatchStatus(statusStr, parsedStatus);

      const uint32_t remainingMs = respDoc["remaining_time_ms"] | 0;
      baseRemainingTimeMs = remainingMs;
      baseRemainingTimestampMs = now;
      lastApiResponseMs = now;
      apiResponseReceived = true;

      if (statusParsed) {
        remoteStatus = parsedStatus;
      }

      // Treat a well-formed JSON body as a successful API interaction for timeout tracking.
      lastSuccessfulApiMs = now;

#ifdef DEBUG
      Serial.print("API status: ");
      Serial.print(statusStr ? statusStr : "<null>");
      Serial.print(" remaining_ms=");
      Serial.println(remainingMs);
#endif
    } else {
#ifdef DEBUG
      Serial.print("API JSON parse error: ");
      Serial.println(err.f_str());
#endif
    }
  } else {
#ifdef DEBUG
    Serial.print("API POST failed: ");
    Serial.println(httpCode);
#endif
  }

  http.end();
}

}  // namespace network
