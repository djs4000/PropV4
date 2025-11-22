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

constexpr int16_t HEADER_Y = 10;
constexpr int16_t TIMER_Y = 40;
constexpr int16_t STATUS_Y = 110;
constexpr int16_t BAR_Y = 150;
constexpr int16_t CODE_Y = 190;
constexpr int16_t IP_Y = 220;

TFT_eSPI tft = TFT_eSPI();
bool screenInitialized = false;

FlameState lastStateRendered = ON;
bool firstRender = true;
float lastArmingProgress = -1.0f;
bool lastNetworkWarning = false;
String lastStatusText;
String lastIpText;
uint32_t lastTimerMs = 0;

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
  tft.drawString("Digital Flame", tft.width() / 2, HEADER_Y);
}

void drawStatusLine(const String &status) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextSize(2);
  tft.fillRect(0, STATUS_Y - 4, tft.width(), 26, BACKGROUND_COLOR);
  tft.drawString("Status: " + status, 4, STATUS_Y - 2);
}

void drawTimer(uint32_t ms) {
  char buffer[8] = {0};
  util::formatTimeMMSS(ms, buffer, sizeof(buffer));
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(5);
  tft.fillRect(0, TIMER_Y, tft.width(), 60, BACKGROUND_COLOR);
  tft.drawString(buffer, tft.width() / 2, TIMER_Y + 28);
}

void drawArmingBar(float progress01) {
  const int16_t barWidth = 200;
  const int16_t barHeight = 16;
  const int16_t x = (tft.width() - barWidth) / 2;
  const int16_t y = BAR_Y;
  tft.drawRect(x, y, barWidth, barHeight, FOREGROUND_COLOR);
  const int16_t fill = static_cast<int16_t>(barWidth * progress01);
  tft.fillRect(x + 1, y + 1, barWidth - 2, barHeight - 2, BACKGROUND_COLOR);
  tft.fillRect(x + 1, y + 1, fill, barHeight - 2, FOREGROUND_COLOR);
}

void drawNetworkWarning(bool show) {
  if (!show) {
    tft.fillRect(0, BAR_Y + 24, tft.width(), 20, BACKGROUND_COLOR);
    return;
  }
  tft.fillRect(0, BAR_Y + 24, tft.width(), 20, WARNING_COLOR);
  tft.setTextColor(TFT_BLACK, WARNING_COLOR);
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(2);
  tft.drawString("Network delay", tft.width() / 2, BAR_Y + 34);
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
  tft.fillRect(0, BAR_Y + 40, tft.width(), 32, BACKGROUND_COLOR);
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(2);
  tft.drawString("Confirm activation", tft.width() / 2, BAR_Y + 52);
}

void drawDefuseEntry(bool show) {
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(2);
  tft.fillRect(0, CODE_Y - 6, tft.width(), 24, BACKGROUND_COLOR);
  if (!show) {
    return;
  }

  char line[2 * DEFUSE_CODE_LENGTH + 1] = {0};
  uint8_t offset = 0;
  for (uint8_t i = 0; i < DEFUSE_CODE_LENGTH; ++i) {
    line[offset++] = (i < inputs::getDefuseLength()) ? inputs::getDefuseBuffer()[i]
                                                     : '_';
    if (i < DEFUSE_CODE_LENGTH - 1) {
      line[offset++] = ' ';
    }
  }
  line[offset] = '\0';
  tft.drawString(line, tft.width() / 2, CODE_Y);
}

void drawIp(const String &ip) {
  tft.setTextDatum(TC_DATUM);
  tft.setTextSize(1);
  tft.fillRect(0, IP_Y - 8, tft.width(), 16, BACKGROUND_COLOR);
  if (ip.isEmpty()) {
    return;
  }
  tft.drawString("IP: " + ip, tft.width() / 2, IP_Y);
}
}  // namespace

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

  if (firstRender || state != lastStateRendered) {
    drawHeader();
    drawStatusLine(flameStateToString(state));
    drawTimer(network::getConfiguredBombDurationMs());
    drawArmingBar(0.0f);
    drawDefuseEntry(false);
    drawNetworkWarning(false);
    drawIp(String(""));
    lastArmingProgress = -1.0f;
    lastNetworkWarning = false;
    lastStatusText = flameStateToString(state);
    lastIpText = String("");
    lastTimerMs = 0;
    lastStateRendered = state;
    firstRender = false;
  }

  const String status = flameStateToString(state);
  if (status != lastStatusText) {
    drawStatusLine(status);
    lastStatusText = status;
  }

  const uint32_t timerMs = network::getConfiguredBombDurationMs();
  if (timerMs != lastTimerMs) {
    drawTimer(timerMs);
    lastTimerMs = timerMs;
  }

  const float progress = inputs::getArmingProgress01();
  if (progress != lastArmingProgress || state != lastStateRendered) {
    const float renderProgress = (state == ARMING) ? progress : 0.0f;
    drawArmingBar(renderProgress);
    lastArmingProgress = renderProgress;
  }

  if (state == ARMING && isIrConfirmWindowActive()) {
    drawIrConfirmPrompt();
  } else {
    tft.fillRect(0, BAR_Y + 40, tft.width(), 32, BACKGROUND_COLOR);
  }

  const bool warning = network::isNetworkWarningActive();
  if (warning != lastNetworkWarning) {
    drawNetworkWarning(warning);
    lastNetworkWarning = warning;
  }

  const bool showCode = state == ARMED;
  drawDefuseEntry(showCode);

  const String ip = network::getDisplayIpString();
  if (ip != lastIpText) {
    drawIp(ip);
    lastIpText = ip;
  }
}
}  // namespace ui
