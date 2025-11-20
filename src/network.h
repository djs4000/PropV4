#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "state_machine.h"

namespace network {

// Starts the first WiFi connection attempt using credentials from wifi_config.h.
void beginWifi();

// Non-blocking WiFi state machine. Call this from loop() to advance retries.
void updateWifi();

bool isWifiConnected();
bool hasWifiFailedPermanently();
String getWifiIpString();
uint64_t getLastSuccessfulApiMs();
MatchStatus getRemoteMatchStatus();

// Allow state/timer injection for payload construction (set by state machine).
void setOutboundStatus(FlameState state, uint32_t timerMs);

// Periodic API POST handler; must be called from loop() once WiFi is connected.
void updateApi();

}  // namespace network
