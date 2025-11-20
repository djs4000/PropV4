#pragma once

#include <Arduino.h>
#include <ArduinoJson.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <HTTPClient.h>

#include "state_machine.h"

namespace network {

void initNetwork();
void updateNetwork();

bool isWifiConnected();
uint64_t getLastSuccessfulApiMs();
MatchStatus getRemoteMatchStatus();

// Allow state/timer injection for payload construction (set by state machine).
void setOutboundStatus(FlameState state, uint32_t timerMs);

}  // namespace network
