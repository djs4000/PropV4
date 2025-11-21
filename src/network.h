#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "state_machine.h"

namespace network {

// Starts the first WiFi connection attempt using credentials from NVS (with
// defaults from wifi_config.h as a fallback).
void beginWifi();

// Non-blocking WiFi state machine. Call this from loop() to advance retries.
void updateWifi();

// Accessors for currently loaded configuration values.
const String &getConfiguredWifiSsid();
const String &getConfiguredApiEndpoint();
const String &getConfiguredDefuseCode();
uint32_t getConfiguredBombDurationMs();

bool isWifiConnected();
bool hasWifiFailedPermanently();  // True only when config portal is not running.
bool isConfigPortalActive();
String getConfigPortalSsid();
String getConfigPortalPassword();
String getConfigPortalAddress();
String getWifiIpString();
uint64_t getLastSuccessfulApiMs();
MatchStatus getRemoteMatchStatus();
uint32_t getRemoteRemainingTimeMs();
bool hasReceivedApiResponse();

// Allow state/timer injection for payload construction (set by state machine).
void setOutboundStatus(FlameState state, uint32_t timerMs);

// Periodic API POST handler; must be called from loop() once WiFi is connected.
void updateApi();

// SoftAP configuration portal used when WiFi station connection fails.
void beginConfigPortal();
void updateConfigPortal();

}  // namespace network
