/*
 * 02_TouchTest
 * ----------------------------------------------------------------
 * Test 2 of 4 — verifies the FT3168 capacitive touch over I2C.
 *
 * What you should see:
 *   - Screen shows "Touch me" prompt
 *   - Touch coordinates print to Serial Monitor (115200 baud)
 *   - A small white dot is drawn wherever you touch
 *   - Double-tap clears the screen
 *
 * The FT3168 sits on the same I2C bus you'll later use for any
 * I2C add-ons (PN532 NFC, accelerometer, etc).
 *
 * Notes:
 *   - We talk to the FT3168 directly via Wire (no extra library)
 *     because the chip is essentially a clone of FT6336 with
 *     the same register map.
 *   - For multi-touch (up to 5 points) FT3168 supports it, but
 *     here we only read the first touch point for simplicity.
 * ----------------------------------------------------------------
 */

#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include "pin_config.h"

// ----- Display (same as test 01) ------------------------------------------
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);

Arduino_GFX *gfx = new Arduino_SH8601(
    bus, LCD_RST, 0, false, LCD_WIDTH, LCD_HEIGHT);

// ----- FT3168 register map ------------------------------------------------
#define FT_REG_NUM_TOUCHES   0x02
#define FT_REG_P1_XH         0x03
#define FT_REG_P1_XL         0x04
#define FT_REG_P1_YH         0x05
#define FT_REG_P1_YL         0x06

uint32_t lastTouchMs   = 0;
uint32_t lastClearMs   = 0;
uint16_t lastX = 0, lastY = 0;

// ----- Helpers -------------------------------------------------------------
uint8_t ftReadReg(uint8_t reg) {
  Wire.beginTransmission((uint8_t)FT3168_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)FT3168_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}

bool ftReadTouch(uint16_t &x, uint16_t &y) {
  uint8_t n = ftReadReg(FT_REG_NUM_TOUCHES);
  if (n == 0 || n > 5) return false;

  // burst read 4 bytes from 0x03
  Wire.beginTransmission((uint8_t)FT3168_ADDR);
  Wire.write(FT_REG_P1_XH);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)FT3168_ADDR, (uint8_t)4);
  if (Wire.available() < 4) return false;

  uint8_t xh = Wire.read();
  uint8_t xl = Wire.read();
  uint8_t yh = Wire.read();
  uint8_t yl = Wire.read();

  x = ((xh & 0x0F) << 8) | xl;
  y = ((yh & 0x0F) << 8) | yl;
  return true;
}

// Horizontally-centered text helper (uses getTextBounds)
void centerText(const char *text, int16_t y, uint8_t size, uint16_t color) {
  int16_t x1, y1; uint16_t w, h;
  gfx->setTextSize(size);
  gfx->setTextColor(color);
  gfx->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  gfx->setCursor((LCD_WIDTH - (int16_t)w) / 2, y);
  gfx->print(text);
}

void drawPrompt() {
  gfx->fillScreen(BLACK);
  centerText("Touch me",            40, 3, WHITE);
  centerText("double-tap to clear", 90, 1, 0x7BEF);
}

// ----- Setup ---------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[02_TouchTest] starting...");

  // Power on the AMOLED panel (O_PWR_EN)
  pinMode(LCD_EN, OUTPUT);
  digitalWrite(LCD_EN, HIGH);
  delay(50);

  // Reset the FT3168 touch chip before bringing up I2C
  pinMode(TP_RST, OUTPUT);
  digitalWrite(TP_RST, LOW);
  delay(10);
  digitalWrite(TP_RST, HIGH);
  delay(100);   // FT3168 needs ~50ms after reset

  // Display
  if (!gfx->begin()) {
    Serial.println("ERROR: display init failed");
    while (1) delay(1000);
  }
  gfx->Display_Brightness(120);
  drawPrompt();

  // I2C (SDA=GPIO2, SCL=GPIO1 on this board)
  Wire.begin(IIC_SDA, IIC_SCL, 400000);
  delay(50);

  // Optional: use TP_INT as a wake / data-ready pin
  pinMode(TP_INT, INPUT_PULLUP);

  // Probe FT3168
  Wire.beginTransmission((uint8_t)FT3168_ADDR);
  if (Wire.endTransmission() == 0) {
    Serial.println("FT3168 found at 0x38 ✓");
  } else {
    Serial.println("FT3168 NOT found — check IIC_SDA/SCL pins!");
  }
}

// ----- Loop ----------------------------------------------------------------
void loop() {
  uint16_t x, y;
  if (ftReadTouch(x, y)) {
    // Debounce — only react every 30 ms
    if (millis() - lastTouchMs > 30) {
      lastTouchMs = millis();

      Serial.printf("Touch: x=%3u  y=%3u\n", x, y);

      // Draw dot
      gfx->fillCircle(x, y, 4, GREEN);

      // Double-tap detection (same spot, <400 ms apart)
      if (millis() - lastClearMs < 400 &&
          abs((int)x - lastX) < 20 && abs((int)y - lastY) < 20) {
        drawPrompt();
        Serial.println("→ cleared (double-tap)");
      }
      lastClearMs = millis();
      lastX = x;
      lastY = y;
    }
  }
  delay(5);
}
