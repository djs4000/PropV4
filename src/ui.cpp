#include "ui.h"

#include <TFT_eSPI.h>

#include "effects.h"
#include "inputs.h"
#include "network.h"
#include "state_machine.h"
#include "util.h"

namespace {
constexpr uint8_t BACKLIGHT_PIN = 21;
constexpr uint16_t BACKGROUND_COLOR = TFT_BLACK;
constexpr uint16_t FOREGROUND_COLOR = TFT_WHITE;
constexpr uint16_t WARNING_COLOR = TFT_RED;

TFT_eSPI tft;
bool screenInitialized = false;
bool mainLayoutDrawn = false;
bool configScreenDrawn = false;

FlameState lastState = ON;
String lastTimerText;
String lastStatusText;
int lastProgressPx = -1;
bool lastWarningVisible = false;
bool lastConfirmVisible = false;

int16_t barX() { return (tft.width() - 200) / 2; }
int16_t barY() { return 200; }

void ensureDisplay() {
  if (screenInitialized) return;
  pinMode(BACKLIGHT_PIN, OUTPUT);
  digitalWrite(BACKLIGHT_PIN, HIGH);
  tft.init();
  tft.setRotation(0);
  tft.fillScreen(BACKGROUND_COLOR);
  tft.setTextColor(FOREGROUND_COLOR, BACKGROUND_COLOR);
  screenInitialized = true;
}

void drawMainLayout() {
  if (mainLayoutDrawn) return;
  tft.fillScreen(BACKGROUND_COLOR);
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(2);
  tft.drawString("Digital Flame", tft.width() / 2, 20);

  // progress outline
  tft.drawRect(barX(), barY(), 200, 16, FOREGROUND_COLOR);
  mainLayoutDrawn = true;
}

void drawTimer(uint32_t ms) {
  const String timerText = util::formatTimeMMSS(ms);
  if (timerText == lastTimerText) return;
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(4);
  tft.fillRect(0, 60, tft.width(), 40, BACKGROUND_COLOR);
  tft.drawString(timerText, tft.width() / 2, 70);
  lastTimerText = timerText;
}

void drawStatus(const String &text) {
  if (text == lastStatusText) return;
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(2);
  tft.fillRect(0, 120, tft.width(), 24, BACKGROUND_COLOR);
  tft.drawString(text, tft.width() / 2, 130);
  lastStatusText = text;
}

void drawProgress(float progress) {
  const int innerWidth = 200 - 2;
  const int fill = static_cast<int>(innerWidth * constrain(progress, 0.0f, 1.0f));
  if (fill == lastProgressPx) return;
  tft.fillRect(barX() + 1, barY() + 1, innerWidth, 14, BACKGROUND_COLOR);
  if (fill > 0) {
    tft.fillRect(barX() + 1, barY() + 1, fill, 14, FOREGROUND_COLOR);
  }
  lastProgressPx = fill;
}

void drawWarning(bool visible) {
  if (visible == lastWarningVisible) return;
  const int y = tft.height() - 20;
  tft.fillRect(0, y, tft.width(), 20, BACKGROUND_COLOR);
  if (visible) {
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(1);
    tft.setTextColor(WARNING_COLOR, BACKGROUND_COLOR);
    tft.drawString("Network delay", tft.width() / 2, y + 10);
    tft.setTextColor(FOREGROUND_COLOR, BACKGROUND_COLOR);
  }
  lastWarningVisible = visible;
}

void drawConfirmPrompt(bool visible) {
  if (visible == lastConfirmVisible) return;
  tft.fillRect(0, 150, tft.width(), 40, BACKGROUND_COLOR);
  if (visible) {
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(2);
    tft.drawString("Confirm activation", tft.width() / 2, 170);
  }
  lastConfirmVisible = visible;
}

void drawConfigScreen() {
  if (configScreenDrawn) return;
  tft.fillScreen(BACKGROUND_COLOR);
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(2);
  tft.drawString("Config Portal", 10, 10);
  tft.setTextSize(1);
  tft.drawString("SSID: " + network::getConfigPortalSsid(), 10, 40);
  tft.drawString("Pass: " + network::getConfigPortalPassword(), 10, 55);
  tft.drawString("Visit " + network::getConfigPortalAddress(), 10, 70);
  configScreenDrawn = true;
  mainLayoutDrawn = false;
}

void resetCaches() {
  lastTimerText = "";
  lastStatusText = "";
  lastProgressPx = -1;
  lastWarningVisible = false;
  lastConfirmVisible = false;
}
}  // namespace

namespace ui {

void initUI() { ensureDisplay(); }

void updateUI() {
  ensureDisplay();

  if (network::isConfigPortalActive()) {
    drawConfigScreen();
    return;
  }
  configScreenDrawn = false;

  drawMainLayout();

  const FlameState state = state_machine::getState();
  const uint32_t timerMs = state_machine::getArmedTimerMs();
  drawTimer(timerMs == 0 ? network::getConfiguredBombDurationMs() : timerMs);

  String status = String("Status: ") + state_machine::flameStateToString(state);
  drawStatus(status);

  const float progress = (state == ARMING) ? inputs::armingProgress01() : 0.0f;
  drawProgress(progress);

  drawConfirmPrompt(state == ARMING && state_machine::isIrConfirmationPending());

  const bool warn = state == ACTIVE && network::isApiWarning();
  drawWarning(warn);

  if (state == ERROR_STATE && lastState != ERROR_STATE) {
    tft.fillScreen(BACKGROUND_COLOR);
    tft.setTextDatum(TC_DATUM);
    tft.setTextSize(3);
    tft.setTextColor(WARNING_COLOR, BACKGROUND_COLOR);
    tft.drawString("ERROR", tft.width() / 2, tft.height() / 2);
    tft.setTextColor(FOREGROUND_COLOR, BACKGROUND_COLOR);
    mainLayoutDrawn = false;
    resetCaches();
  }

  if (state != lastState && state != ERROR_STATE) {
    resetCaches();
    mainLayoutDrawn = false;
    drawMainLayout();
  }

  lastState = state;
}

}  // namespace ui
