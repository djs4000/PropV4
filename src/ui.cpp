#include "ui.h"

#include <TFT_eSPI.h>

#include "state_machine.h"

namespace {
constexpr uint8_t BACKLIGHT_PIN = 21;
constexpr uint16_t BACKGROUND_COLOR = TFT_BLACK;
constexpr uint16_t TITLE_COLOR = TFT_ORANGE;
constexpr uint16_t TEXT_COLOR = TFT_WHITE;

TFT_eSPI tft = TFT_eSPI();

String formatTimer(uint32_t ms) {
  const uint32_t totalSeconds = ms / 1000;
  const uint32_t minutes = totalSeconds / 60;
  const uint32_t seconds = totalSeconds % 60;

  char buffer[16];
  snprintf(buffer, sizeof(buffer), "%02u:%02u", static_cast<unsigned>(minutes), static_cast<unsigned>(seconds));
  return String(buffer);
}

void drawSplashBase() {
  tft.fillScreen(BACKGROUND_COLOR);
  tft.setTextColor(TITLE_COLOR, BACKGROUND_COLOR);
  tft.setTextSize(2);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Digital Flame", 10, 10);
  tft.setTextColor(TEXT_COLOR, BACKGROUND_COLOR);
  tft.drawString("Booting...", 10, 40);
}
}

namespace ui {
void initDisplay() {
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);

  tft.init();
  tft.setRotation(0);  // Rotate display 90 degrees relative to previous orientation - use 0 or 2 for portrait

  drawSplashBase();
  renderStatus(getState(), false, false, "", WaitingOnStart, 0);
}

void renderStatus(FlameState state, bool wifiConnected, bool wifiError, const String &wifiIp,
                  MatchStatus matchStatus, uint32_t matchTimerMs) {
  static bool hasRendered = false;
  static FlameState lastRenderedState = ON;
  static bool lastWifiConnected = false;
  static bool lastWifiError = false;
  static String lastIp = "";
  static MatchStatus lastMatchStatus = WaitingOnStart;
  static uint32_t lastMatchTimerBucket = 0;

  const uint32_t timerBucket = matchTimerMs / 1000;  // Only update UI when visible seconds change.

  if (hasRendered && state == lastRenderedState && wifiConnected == lastWifiConnected &&
      wifiError == lastWifiError && wifiIp == lastIp && matchStatus == lastMatchStatus &&
      timerBucket == lastMatchTimerBucket) {
    // Avoid unnecessary redraws that can cause visible flicker when nothing changed.
    return;
  }

  // Small status area to show current state without redrawing the whole screen.
  constexpr int16_t statusY = 80;
  constexpr int16_t statusHeight = 140;
  tft.fillRect(0, statusY, tft.width(), statusHeight, BACKGROUND_COLOR);
  tft.setTextColor(TEXT_COLOR, BACKGROUND_COLOR);
  tft.setTextSize(2);
  tft.setTextDatum(TL_DATUM);

  String stateText = "State: ";
  stateText += flameStateToString(state);
  tft.drawString(stateText, 10, statusY + 5);

  String wifiText = "WiFi: ";
  if (wifiConnected) {
    wifiText += "Connected";
  } else if (wifiError) {
    wifiText += "Error";
  } else {
    wifiText += "Connecting...";
  }
  tft.drawString(wifiText, 10, statusY + 30);

  if (wifiConnected) {
    String ipText = "IP: ";
    ipText += wifiIp;
    tft.setTextSize(1);
    tft.drawString(ipText, 10, statusY + 55);
    tft.setTextSize(2);
  }

  // Show clear recovery instructions when WiFi failures push us into ERROR_STATE.
  if (wifiError) {
    tft.setTextSize(1);
    tft.drawString("WiFi Error - Hold both buttons to reset", 10, statusY + 50);
  }

  String matchText = "Match: ";
  matchText += matchStatusToString(matchStatus);
  tft.drawString(matchText, 10, statusY + 70);

  String timerText = "Timer: ";
  timerText += formatTimer(timerBucket * 1000);
  tft.drawString(timerText, 10, statusY + 95);

  hasRendered = true;
  lastRenderedState = state;
  lastWifiConnected = wifiConnected;
  lastWifiError = wifiError;
  lastIp = wifiIp;
  lastMatchStatus = matchStatus;
  lastMatchTimerBucket = timerBucket;
}

void initUI() {
  // Additional UI initialization (web/touch) will be added later.
}

void updateUI() {
  // Non-blocking UI updates and touch handling will be implemented later.
}
}  // namespace ui
