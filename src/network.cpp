#include "network.h"

#include "game_config.h"
#include "state_machine.h"
#include "wifi_config.h"

namespace network {

// Internal state cached for network interactions. Variables are file-local via
// `static` to avoid exposing them outside this translation unit.
static uint64_t lastSuccessfulApiMs = 0;
static MatchStatus remoteStatus = WaitingOnStart;
static FlameState outboundState = ON;
static uint32_t outboundTimerMs = DEFAULT_BOMB_DURATION_MS;

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

void setOutboundStatus(FlameState state, uint32_t timerMs) {
  outboundState = state;
  outboundTimerMs = timerMs;
}

}  // namespace network
