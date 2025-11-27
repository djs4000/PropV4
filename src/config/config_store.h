#pragma once

#include <Arduino.h>

namespace config_store {

struct RuntimeConfig {
  String wifiSsid;
  String wifiPass;
  String defuseCode;
  uint32_t bombDurationMs;
  String apiEndpoint;
};

struct EffectsConfig {
  uint8_t ledBrightness;
  bool audioEnabled;
};

struct UiThemeConfig {
  uint16_t primaryColor565;
  uint16_t accentColor565;
  uint16_t backgroundColor565;
};

void begin();

const RuntimeConfig &getRuntimeConfig();
const EffectsConfig &getEffectsConfig();
const UiThemeConfig &getUiThemeConfig();

// Convenience accessors for commonly used runtime fields.
const String &getWifiSsid();
const String &getWifiPassword();
const String &getApiEndpoint();
const String &getDefuseCode();
uint32_t getBombDurationMs();

void saveRuntimeConfig(const RuntimeConfig &updated);
void saveEffectsConfig(const EffectsConfig &updated);
void saveUiThemeConfig(const UiThemeConfig &updated);

}  // namespace config_store

