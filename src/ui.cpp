#include "ui.h"

#include <TFT_eSPI.h>

#include "game_config.h"
#include "inputs.h"
#include "network.h"
#include "state_machine.h"
#include "util.h"

namespace {
constexpr uint8_t BACKLIGHT_PIN = 21;
constexpr uint16_t BACKGROUND_COLOR = TFT_BLACK;
constexpr uint16_t FOREGROUND_COLOR = TFT_WHITE;
constexpr uint16_t WARNING_COLOR = TFT_YELLOW;
constexpr uint16_t ERROR_COLOR = TFT_RED;

TFT_eSPI tft = TFT_eSPI();
bool screenInitialized = false;

FlameState lastStateRendered = ON;
float lastArmingProgress = -1.0f;
bool lastNetworkWarning = false;

void ensureDisplayReady() {
  if (screenInitialized) {
    return;
  }
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(BACKGROUND_COLOR);
  tft.setTextColor(FOREGROUND_COLOR, BACKGROUND_COLOR);
  screenInitialized = true;
}

void drawHeader() {
  tft.fillScreen(BACKGROUND_COLOR);
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(2);
  tft.drawString("Digital Flame", tft.width() / 2, 16);
}

void drawStatusLine(const String &status) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(2);
  tft.fillRect(0, 40, tft.width(), 24, BACKGROUND_COLOR);
  tft.drawString("Status: " + status, 4, 40);
}

void drawTimer(uint32_t ms) {
  char buffer[8] = {0};
  util::formatTimeMMSS(ms, buffer, sizeof(buffer));
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(4);
  tft.fillRect(0, 70, tft.width(), 40, BACKGROUND_COLOR);
  tft.drawString(buffer, tft.width() / 2, 80);
}

void drawArmingBar(float progress01) {
  const int16_t barWidth = 200;
  const int16_t barHeight = 16;
  const int16_t x = (tft.width() - barWidth) / 2;
  const int16_t y = 130;
  tft.drawRect(x, y, barWidth, barHeight, FOREGROUND_COLOR);
  const int16_t fill = static_cast<int16_t>(barWidth * progress01);
  tft.fillRect(x + 1, y + 1, barWidth - 2, barHeight - 2, BACKGROUND_COLOR);
  tft.fillRect(x + 1, y + 1, fill, barHeight - 2, FOREGROUND_COLOR);
}

void drawNetworkWarning(bool show) {
  if (!show) {
    tft.fillRect(0, 190, tft.width(), 24, BACKGROUND_COLOR);
    return;
  }
  tft.fillRect(0, 190, tft.width(), 24, WARNING_COLOR);
  tft.setTextColor(TFT_BLACK, WARNING_COLOR);
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(2);
  tft.drawString("Network delay", tft.width() / 2, 202);
  tft.setTextColor(FOREGROUND_COLOR, BACKGROUND_COLOR);
}

void drawErrorScreen() {
  tft.fillScreen(ERROR_COLOR);
  tft.setTextColor(TFT_WHITE, ERROR_COLOR);
  tft.setTextSize(3);
  tft.setTextDatum(TC_DATUM);
  tft.drawString("ERROR", tft.width() / 2, tft.height() / 2);
  tft.setTextColor(FOREGROUND_COLOR, BACKGROUND_COLOR);
}

void drawIrConfirmPrompt() {
  tft.fillRect(0, 160, tft.width(), 40, BACKGROUND_COLOR);
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(2);
  tft.drawString("Confirm activation", tft.width() / 2, 180);
}
}

namespace ui {
void init() { ensureDisplayReady(); }

void update() {
  ensureDisplayReady();
  const FlameState state = getState();

  if (state == ERROR_STATE) {
    drawErrorScreen();
    lastStateRendered = state;
    return;
  }

  if (state != lastStateRendered) {
    drawHeader();
    drawStatusLine(flameStateToString(state));
    drawTimer(network::getConfiguredBombDurationMs());
    lastArmingProgress = -1.0f;
    lastNetworkWarning = false;
    lastStateRendered = state;
  }

  const float progress = inputs::getArmingProgress01();
  if (state == ARMING && progress != lastArmingProgress) {
    drawArmingBar(progress);
    lastArmingProgress = progress;
  } else if (state != ARMING && lastArmingProgress >= 0.0f) {
    tft.fillRect(0, 130, tft.width(), 20, BACKGROUND_COLOR);
    lastArmingProgress = -1.0f;
  }

  if (state == ARMING && isIrConfirmWindowActive()) {
    drawIrConfirmPrompt();
  } else {
    tft.fillRect(0, 160, tft.width(), 40, BACKGROUND_COLOR);
  }

  const bool warning = network::isNetworkWarningActive();
  if (warning != lastNetworkWarning) {
    drawNetworkWarning(warning);
    lastNetworkWarning = warning;
  }
}
}  // namespace ui

