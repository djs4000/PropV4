#include "ui.h"

#include <TFT_eSPI.h>

#include "state_machine.h"

namespace {
constexpr uint8_t BACKLIGHT_PIN = 21;
constexpr uint16_t BACKGROUND_COLOR = TFT_BLACK;
constexpr uint16_t TITLE_COLOR = TFT_ORANGE;
constexpr uint16_t TEXT_COLOR = TFT_WHITE;

TFT_eSPI tft = TFT_eSPI();

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
  tft.setRotation(2);  // Rotate display 90 degrees relative to previous orientation

  drawSplashBase();
  renderStatus(getState());
}

void renderStatus(FlameState state) {
  // Small status area to show current state without redrawing the whole screen.
  constexpr int16_t statusY = 80;
  constexpr int16_t statusHeight = 30;
  tft.fillRect(0, statusY, tft.width(), statusHeight, BACKGROUND_COLOR);
  tft.setTextColor(TEXT_COLOR, BACKGROUND_COLOR);
  tft.setTextSize(2);
  tft.setTextDatum(TL_DATUM);

  String statusText = "State: ";
  statusText += flameStateToString(state);
  tft.drawString(statusText, 10, statusY + 5);
}

void initUI() {
  // Additional UI initialization (web/touch) will be added later.
}

void updateUI() {
  // Non-blocking UI updates and touch handling will be implemented later.
}
}  // namespace ui
