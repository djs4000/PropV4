#include "network.h"

#include "game_config.h"
#include "state_machine.h"
#include "util.h"
#include "wifi_config.h"

#include <esp_heap_caps.h>
#include <esp_timer.h>

namespace network {

// Internal state cached for network interactions. Variables are file-local via
// `static` to avoid exposing them outside this translation unit.
static uint64_t lastSuccessfulApiMs = 0;
static MatchStatus remoteStatus = WaitingOnStart;
static FlameState outboundState = ON;
static uint32_t outboundTimerMs = DEFAULT_BOMB_DURATION_MS;

static unsigned long lastPostAttemptMs = 0;
static WiFiClient wifiClient;
static HTTPClient httpClient;

// Custom allocator that leverages ESP32 SPI RAM for ArduinoJson documents.
struct SpiRamAllocator : ArduinoJson::Allocator {
  void *allocate(size_t size) override { return heap_caps_malloc(size, MALLOC_CAP_SPIRAM); }

  void deallocate(void *pointer) override { heap_caps_free(pointer); }

  void *reallocate(void *ptr, size_t new_size) override {
    return heap_caps_realloc(ptr, new_size, MALLOC_CAP_SPIRAM);
  }
};

static SpiRamAllocator jsonAllocator;

// Placeholder API endpoint; will be replaced by Preferences/web UI later.
static String apiEndpoint = DEFAULT_API_ENDPOINT;

static void connectWifiIfNeeded() {
  if (WiFi.isConnected()) {
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASS);
#ifdef DEBUG
  Serial.print("Connecting to WiFi: ");
  Serial.println(DEFAULT_WIFI_SSID);
#endif
}

static void sendStatusUpdate() {
  if (!WiFi.isConnected()) {
    return;
  }

  // Use JsonDocument with a custom allocator to align with ArduinoJson v7 API
  // guidance while keeping heap usage flexible on ESP32.
  JsonDocument doc(&jsonAllocator);
  doc["state"] = flameStateToString(outboundState);
  doc["timer"] = outboundTimerMs;
  doc["timestamp"] = static_cast<uint64_t>(esp_timer_get_time());

  String payload;
  serializeJson(doc, payload);

  httpClient.begin(wifiClient, apiEndpoint);
  httpClient.addHeader("Content-Type", "application/json");

#ifdef DEBUG
  Serial.print("POST ");
  Serial.print(apiEndpoint);
  Serial.print(" -> ");
  Serial.println(payload);
#endif

  const int httpCode = httpClient.POST(payload);
  if (httpCode == HTTP_CODE_OK) {
    JsonDocument response(&jsonAllocator);
    DeserializationError err = deserializeJson(response, httpClient.getString());
    if (!err) {
      lastSuccessfulApiMs = millis();
      const char *statusStr = response["status"] | "";
      util::parseMatchStatus(statusStr, remoteStatus);
    }
  }

  httpClient.end();
}
void initNetwork() {
  lastSuccessfulApiMs = millis();
  connectWifiIfNeeded();
}

void updateNetwork() {
  connectWifiIfNeeded();

  const unsigned long now = millis();
  if (now - lastPostAttemptMs >= API_POST_INTERVAL_MS) {
    lastPostAttemptMs = now;
    sendStatusUpdate();
  }
}

bool isWifiConnected() { return WiFi.isConnected(); }

uint64_t getLastSuccessfulApiMs() { return lastSuccessfulApiMs; }

MatchStatus getRemoteMatchStatus() { return remoteStatus; }

void setOutboundStatus(FlameState state, uint32_t timerMs) {
  outboundState = state;
  outboundTimerMs = timerMs;
}

}  // namespace network
