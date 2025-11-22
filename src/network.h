#pragma once

#include <Arduino.h>

#include "state_machine.h"

namespace network {
void beginWifi();
void updateWifi();
void updateApi();
void updateConfigPortal();

bool isWifiConnected();
bool hasWifiFailedPermanently();
bool isConfigPortalActive();

const String &getConfiguredWifiSsid();
const String &getConfiguredDefuseCode();
const String &getConfiguredApiEndpoint();
uint32_t getConfiguredBombDurationMs();

String getConfigPortalSsid();
String getConfigPortalPassword();
String getConfigPortalAddress();
String getDisplayIpString();
String getWifiIpString();

uint64_t getLastSuccessfulApiMs();
MatchStatus getRemoteMatchStatus();
bool hasReceivedApiResponse();
bool isNetworkWarningActive();
}  // namespace network

