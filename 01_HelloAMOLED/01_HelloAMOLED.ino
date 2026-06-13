/*
 * 01_HelloAMOLED  (v2 — centered + rounded-corner safe)
 * ----------------------------------------------------------------
 * Test 1 of 4 — verifies the SH8601 AMOLED panel.
 *
 * Hardware: MaTouch ESP32-S3 AMOLED 1.8" FT3168
 * Display:  368x448 SH8601 over QSPI (rounded corners ~30 px)
 *
 * What you should see now:
 *   - All text TRULY centered on screen
 *   - A rounded border that follows the panel's curved corners
 *     instead of getting clipped
 *   - Color cycles every 2 seconds
 * ----------------------------------------------------------------
 */

#include <Arduino.h>
#include <Arduino_GFX_Library.h>
#include "pin_config.h"

// --- QSPI bus to SH8601 ----------------------------------------------------
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);

Arduino_GFX *gfx = new Arduino_SH8601(
    bus, LCD_RST, 0 /*rot*/, false /*IPS*/, LCD_WIDTH, LCD_HEIGHT);

// --- Layout constants ------------------------------------------------------
// The 1.8" AMOLED has rounded corners. Keep drawing inside this safe inset
// or the curve will clip your pixels.
const int16_t CORNER_RADIUS = 32;   // ~px, tune if your unit clips differently
const int16_t SAFE_INSET    = 10;   // outer keep-out

// --- Color palette ---------------------------------------------------------
uint16_t colors[] = {RED, GREEN, BLUE, YELLOW, MAGENTA, CYAN, WHITE};
const uint8_t numColors = sizeof(colors) / sizeof(colors[0]);
uint8_t colorIdx = 0;

// --- Helper: print text horizontally centered at the given Y --------------
void centerText(const char *text, int16_t y, uint8_t size, uint16_t color) {
  int16_t x1, y1;
  uint16_t w, h;
  gfx->setTextSize(size);
  gfx->setTextColor(color);
  gfx->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  int16_t x = (LCD_WIDTH - (int16_t)w) / 2;
  gfx->setCursor(x, y);
  gfx->print(text);
}

void drawStaticUI() {
  gfx->fillScreen(BLACK);

  // Title — true center horizontally.
  // Vertical layout: title near 35% of height, subtitle near 50%, stamp near 62%
  centerText("SecureKey",   (int16_t)(LCD_HEIGHT * 0.34f), 4, WHITE);
  centerText("Hello AMOLED",(int16_t)(LCD_HEIGHT * 0.50f), 2, YELLOW);

  // resolution stamp
  char stamp[32];
  snprintf(stamp, sizeof(stamp), "%d x %d  SH8601", LCD_WIDTH, LCD_HEIGHT);
  centerText(stamp,         (int16_t)(LCD_HEIGHT * 0.62f), 1, 0x7BEF);
}

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[01_HelloAMOLED v2] starting...");

  // Power on the AMOLED panel
  pinMode(LCD_EN, OUTPUT);
  digitalWrite(LCD_EN, HIGH);
  delay(50);

  if (!gfx->begin()) {
    Serial.println("ERROR: gfx->begin() failed.");
    while (true) delay(1000);
  }

  gfx->Display_Brightness(120);
  drawStaticUI();
  Serial.println("Display init OK.");
}

void loop() {
  uint16_t c = colors[colorIdx];

  // Rounded border, 3 px thick, drawn INSIDE the safe area so the curved
  // corners of the panel don't eat the rectangle.
  for (int i = 0; i < 3; i++) {
    gfx->drawRoundRect(
        SAFE_INSET + i,
        SAFE_INSET + i,
        LCD_WIDTH  - 2 * (SAFE_INSET + i),
        LCD_HEIGHT - 2 * (SAFE_INSET + i),
        CORNER_RADIUS - i,
        c);
  }

  colorIdx = (colorIdx + 1) % numColors;
  delay(2000);
}
