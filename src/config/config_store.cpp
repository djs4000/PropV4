#include "config/config_store.h"

#include <Preferences.h>

#include "game_config.h"
#include "wifi_config.h"

namespace config_store {
namespace {
Preferences preferences;
bool preferencesInitialized = false;
RuntimeConfig runtimeConfig = {String(DEFAULT_WIFI_SSID), String(DEFAULT_WIFI_PASS),
                               String(DEFAULT_DEFUSE_CODE), DEFAULT_BOMB_DURATION_MS,
                               String(DEFAULT_API_ENDPOINT)};
EffectsConfig effectsConfig = {255, true};
UiThemeConfig uiThemeConfig = {0xFFFF, 0x07E0, 0x0000};

void ensurePreferences() {
  if (!preferencesInitialized) {
    preferences.begin("digital_flame", false);
    preferencesInitialized = true;
  }
}

RuntimeConfig sanitizeRuntimeConfig(RuntimeConfig config) {
  if (config.wifiSsid.isEmpty()) {
    config.wifiSsid = DEFAULT_WIFI_SSID;
  }
  if (config.apiEndpoint.isEmpty()) {
    config.apiEndpoint = DEFAULT_API_ENDPOINT;
  }
  if (config.defuseCode.isEmpty()) {
    config.defuseCode = DEFAULT_DEFUSE_CODE;
  }
  if (config.bombDurationMs == 0) {
    config.bombDurationMs = DEFAULT_BOMB_DURATION_MS;
  }
  return config;
}

void loadRuntimeConfigFromPrefs() {
  ensurePreferences();
  runtimeConfig.wifiSsid = preferences.getString("wifi_ssid", DEFAULT_WIFI_SSID);
  runtimeConfig.wifiPass = preferences.getString("wifi_pass", DEFAULT_WIFI_PASS);
  runtimeConfig.defuseCode = preferences.getString("defuse_code", DEFAULT_DEFUSE_CODE);
  runtimeConfig.apiEndpoint = preferences.getString("api_endpoint", DEFAULT_API_ENDPOINT);
  runtimeConfig.bombDurationMs = preferences.getUInt("bomb_duration_ms", DEFAULT_BOMB_DURATION_MS);

  runtimeConfig = sanitizeRuntimeConfig(runtimeConfig);
}

void loadEffectsConfigFromPrefs() {
  ensurePreferences();
  effectsConfig.ledBrightness = preferences.getUChar("effects_bright", 255);
  effectsConfig.audioEnabled = preferences.getBool("effects_audio", true);
}

void loadUiThemeConfigFromPrefs() {
  ensurePreferences();
  uiThemeConfig.primaryColor565 = preferences.getUShort("ui_primary", 0xFFFF);
  uiThemeConfig.accentColor565 = preferences.getUShort("ui_accent", 0x07E0);
  uiThemeConfig.backgroundColor565 = preferences.getUShort("ui_bg", 0x0000);
}

void persistRuntimeConfig(const RuntimeConfig &config) {
  ensurePreferences();
  preferences.putString("wifi_ssid", config.wifiSsid);
  preferences.putString("wifi_pass", config.wifiPass);
  preferences.putString("defuse_code", config.defuseCode);
  preferences.putUInt("bomb_duration_ms", config.bombDurationMs);
  preferences.putString("api_endpoint", config.apiEndpoint);
}

void persistEffectsConfig(const EffectsConfig &config) {
  ensurePreferences();
  preferences.putUChar("effects_bright", config.ledBrightness);
  preferences.putBool("effects_audio", config.audioEnabled);
}

void persistUiThemeConfig(const UiThemeConfig &config) {
  ensurePreferences();
  preferences.putUShort("ui_primary", config.primaryColor565);
  preferences.putUShort("ui_accent", config.accentColor565);
  preferences.putUShort("ui_bg", config.backgroundColor565);
}

}  // namespace

void begin() {
  loadRuntimeConfigFromPrefs();
  loadEffectsConfigFromPrefs();
  loadUiThemeConfigFromPrefs();
}

const RuntimeConfig &getRuntimeConfig() { return runtimeConfig; }

const EffectsConfig &getEffectsConfig() { return effectsConfig; }

const UiThemeConfig &getUiThemeConfig() { return uiThemeConfig; }

const String &getWifiSsid() { return runtimeConfig.wifiSsid; }

const String &getWifiPassword() { return runtimeConfig.wifiPass; }

const String &getApiEndpoint() { return runtimeConfig.apiEndpoint; }

const String &getDefuseCode() { return runtimeConfig.defuseCode; }

uint32_t getBombDurationMs() { return runtimeConfig.bombDurationMs; }

void saveRuntimeConfig(const RuntimeConfig &updated) {
  runtimeConfig = sanitizeRuntimeConfig(updated);
  persistRuntimeConfig(runtimeConfig);
}

void saveEffectsConfig(const EffectsConfig &updated) {
  effectsConfig = updated;
  persistEffectsConfig(effectsConfig);
}

void saveUiThemeConfig(const UiThemeConfig &updated) {
  uiThemeConfig = updated;
  persistUiThemeConfig(uiThemeConfig);
}

}  // namespace config_store

