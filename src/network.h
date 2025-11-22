#pragma once

#include <Arduino.h>
#include <WiFi.h>

#include "state_machine.h"

namespace network {

void beginWifi();
void updateWifi();
void updateApi();
void updateConfigPortal();

bool isWifiConnected();
bool hasWifiFailedPermanently();
bool isConfigPortalActive();
String getConfigPortalSsid();
String getConfigPortalPassword();
String getConfigPortalAddress();
String getWifiIpString();

const String &getConfiguredWifiSsid();
const String &getConfiguredApiEndpoint();
const String &getConfiguredDefuseCode();
uint32_t getConfiguredBombDurationMs();

uint64_t getLastSuccessfulApiMs();
MatchStatus getRemoteMatchStatus();
bool hasReceivedApiResponse();
bool isApiWarning();

void beginConfigPortal();

}  // namespace network
