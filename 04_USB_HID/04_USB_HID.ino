/*
 * 04_USB_HID  —  Touch-button password typer
 * ----------------------------------------------------------------
 * Tap the big "TAP TO PASTE" button on the AMOLED  →  device types
 * the payload string as a USB keyboard (visible app gets the text).
 * Physical User button (SW2) works as a backup trigger.
 *
 *  ⚠️  CRITICAL BOARD SETTINGS for THIS sketch only:
 *    Tools → USB Mode           = "USB-OTG (TinyUSB)"     ← NOT CDC+JTAG
 *    Tools → USB CDC On Boot    = Enabled
 *    Tools → Upload Mode        = UART0 / Hardware CDC
 *
 *  Switch USB Mode BACK to "Hardware CDC and JTAG" before
 *  flashing sketches 01/02/03.
 *
 *  If COM port disappears after flashing:
 *    Unplug → hold BOOT → plug in → release BOOT → reflash 01_HelloAMOLED.
 *
 *  Note on speed:
 *    USB HID protocol caps typing at ~125 reports/sec on Windows, so
 *    "Hello SecureKey!" (16 chars) takes ~250 ms — looks instant to
 *    the eye. True clipboard-paste isn't possible from HID-only.
 * ----------------------------------------------------------------
 */

#include <Arduino.h>
#include <Wire.h>
#include <USB.h>
#include <USBHIDKeyboard.h>
#include <Adafruit_NeoPixel.h>
#include <Arduino_GFX_Library.h>
#include "pin_config.h"

// ===== Payload =============================================================
const char *PAYLOAD = "Hello SecureKey!";

// ===== Hardware objects ====================================================
USBHIDKeyboard Keyboard;
Adafruit_NeoPixel led(RGB_LED_COUNT, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_GFX *gfx = new Arduino_SH8601(
    bus, LCD_RST, 0, false, LCD_WIDTH, LCD_HEIGHT);

// ===== On-screen button geometry ===========================================
const int16_t BTN_X = 30;
const int16_t BTN_Y = 170;
const int16_t BTN_W = LCD_WIDTH - 60;     // 308 px wide
const int16_t BTN_H = 160;
const int16_t BTN_R = 28;                 // corner radius

// Nice colors
const uint16_t COL_BG       = 0x0000;     // black
const uint16_t COL_TITLE    = 0x07FF;     // cyan
const uint16_t COL_BTN_FILL = 0x0500;     // dark green
const uint16_t COL_BTN_EDGE = 0x07E0;     // bright green
const uint16_t COL_BTN_LABEL= 0xFFFF;     // white
const uint16_t COL_BTN_PRESS_FILL = 0xFD20; // orange
const uint16_t COL_BTN_PRESS_EDGE = 0xFFE0; // yellow
const uint16_t COL_STATUS_OK= 0x07E0;     // green
const uint16_t COL_GREY     = 0x7BEF;

// ===== Helpers =============================================================
void centerText(const char *text, int16_t y, uint8_t size, uint16_t color) {
  int16_t x1, y1; uint16_t w, h;
  gfx->setTextSize(size);
  gfx->setTextColor(color);
  gfx->getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
  gfx->setCursor((LCD_WIDTH - (int16_t)w) / 2, y);
  gfx->print(text);
}

void drawButton(bool pressed) {
  uint16_t fill = pressed ? COL_BTN_PRESS_FILL : COL_BTN_FILL;
  uint16_t edge = pressed ? COL_BTN_PRESS_EDGE : COL_BTN_EDGE;

  gfx->fillRoundRect(BTN_X, BTN_Y, BTN_W, BTN_H, BTN_R, fill);
  gfx->drawRoundRect(BTN_X, BTN_Y, BTN_W, BTN_H, BTN_R, edge);
  gfx->drawRoundRect(BTN_X + 1, BTN_Y + 1, BTN_W - 2, BTN_H - 2, BTN_R - 1, edge);

  // 2-line label centered inside button
  gfx->setTextSize(3);
  gfx->setTextColor(COL_BTN_LABEL);
  int16_t x1, y1; uint16_t tw, th;

  const char *L1 = "TAP TO";
  const char *L2 = "PASTE";
  gfx->getTextBounds(L1, 0, 0, &x1, &y1, &tw, &th);
  gfx->setCursor((LCD_WIDTH - (int16_t)tw) / 2, BTN_Y + 35);
  gfx->print(L1);
  gfx->getTextBounds(L2, 0, 0, &x1, &y1, &tw, &th);
  gfx->setCursor((LCD_WIDTH - (int16_t)tw) / 2, BTN_Y + 90);
  gfx->print(L2);
}

void drawScreen(const char *status, uint16_t statusColor) {
  gfx->fillScreen(COL_BG);
  centerText("SecureKey",         40,  3, COL_TITLE);
  centerText(status,              105, 2, statusColor);
  drawButton(false);
  centerText("or press User btn", 360, 1, COL_GREY);
}

// ----- Non-blocking LED ----------------------------------------------------
uint32_t ledClearAt = 0;
void ledSet(uint32_t color, uint32_t holdMs = 250) {
  led.setPixelColor(0, color); led.show();
  ledClearAt = millis() + holdMs;
}
void ledTick() {
  if (ledClearAt && millis() >= ledClearAt) {
    led.clear(); led.show();
    ledClearAt = 0;
  }
}

// ----- FT3168 raw I2C touch reader -----------------------------------------
uint8_t ftReadReg(uint8_t reg) {
  Wire.beginTransmission((uint8_t)FT3168_ADDR);
  Wire.write(reg);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)FT3168_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}

bool ftReadTouch(uint16_t &x, uint16_t &y) {
  uint8_t n = ftReadReg(0x02);              // number of touches
  if (n == 0 || n > 5) return false;
  Wire.beginTransmission((uint8_t)FT3168_ADDR);
  Wire.write(0x03);                          // start of point 1
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)FT3168_ADDR, (uint8_t)4);
  if (Wire.available() < 4) return false;
  uint8_t xh = Wire.read(), xl = Wire.read();
  uint8_t yh = Wire.read(), yl = Wire.read();
  x = ((xh & 0x0F) << 8) | xl;
  y = ((yh & 0x0F) << 8) | yl;
  return true;
}

bool isInsideButton(uint16_t x, uint16_t y) {
  return (x >= BTN_X) && (x < BTN_X + BTN_W) &&
         (y >= BTN_Y) && (y < BTN_Y + BTN_H);
}

// ===== Trigger =============================================================
const uint32_t COOLDOWN_MS = 600;
uint32_t cooldownUntil = 0;
uint32_t statusResetAt = 0;

void sendPayload(const char *source) {
  Serial.printf("[%s] typing: %s\n", source, PAYLOAD);

  // Visual feedback FIRST — feels instant
  drawButton(true);
  ledSet(0xFFFF00, 100);

  // *** Type — single shot, no blocking calls before/after ***
  Keyboard.print(PAYLOAD);
  Keyboard.write(KEY_RETURN);

  // Done — repaint with "Sent!" status, queue revert
  drawScreen("Sent!", COL_STATUS_OK);
  ledSet(0x00FF00, 400);

  cooldownUntil  = millis() + COOLDOWN_MS;
  statusResetAt  = millis() + 1500;       // revert "Sent!" → "Ready" after 1.5s
}

// ===== Button (physical) debounce ==========================================
const uint32_t DEBOUNCE_MS = 30;
bool     btnStable    = HIGH;
bool     btnLastRaw   = HIGH;
uint32_t btnLastChange = 0;

void handlePhysicalButton() {
  bool raw = digitalRead(USER_BTN_PIN);
  if (raw != btnLastRaw) {
    btnLastRaw = raw;
    btnLastChange = millis();
  }
  if ((millis() - btnLastChange) > DEBOUNCE_MS && raw != btnStable) {
    btnStable = raw;
    if (btnStable == LOW && millis() > cooldownUntil) {
      sendPayload("BTN");
    }
  }
}

// ===== Touch handler =======================================================
bool touchWasActive = false;

void handleTouch() {
  uint16_t tx, ty;
  bool active = ftReadTouch(tx, ty);

  // Edge: touch just started
  if (active && !touchWasActive) {
    touchWasActive = true;
    Serial.printf("touch @ %u,%u\n", tx, ty);
    if (isInsideButton(tx, ty) && millis() > cooldownUntil) {
      sendPayload("TAP");
    }
  } else if (!active && touchWasActive) {
    touchWasActive = false;
  }
}

// ===== Setup ===============================================================
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[04_USB_HID] starting...");

  // LED
  led.begin();
  led.setBrightness(40);
  led.clear(); led.show();

  // Power on AMOLED
  pinMode(LCD_EN, OUTPUT);
  digitalWrite(LCD_EN, HIGH);
  delay(50);

  // Touch reset
  pinMode(TP_RST, OUTPUT);
  digitalWrite(TP_RST, LOW);  delay(10);
  digitalWrite(TP_RST, HIGH); delay(100);

  // Display
  if (!gfx->begin()) {
    Serial.println("ERROR: display init failed");
    while (1) delay(1000);
  }
  gfx->Display_Brightness(140);
  drawScreen("Booting...", COL_GREY);

  // I2C for FT3168
  Wire.begin(IIC_SDA, IIC_SCL, 400000);
  pinMode(TP_INT, INPUT_PULLUP);

  // Physical button
  pinMode(USER_BTN_PIN, INPUT_PULLUP);

  // Safety window — CDC stays clean for 3 seconds so a panic re-upload works
  Serial.println("HID will start in 3 seconds...");
  for (int i = 3; i > 0; i--) {
    char tmp[24]; snprintf(tmp, sizeof(tmp), "Start in %d", i);
    drawScreen(tmp, 0xFFE0 /*yellow*/);
    delay(1000);
  }

  // USB HID
  Keyboard.begin();
  USB.begin();

  drawScreen("Ready", COL_STATUS_OK);
  Serial.println("USB HID started. Tap the button or press SW2.");
}

// ===== Loop ================================================================
uint32_t lastTouchPollMs = 0;

void loop() {
  ledTick();
  handlePhysicalButton();

  // Touch poll at ~50 Hz (every 20 ms) — saves I2C bandwidth
  if (millis() - lastTouchPollMs > 20) {
    lastTouchPollMs = millis();
    handleTouch();
  }

  // Auto-revert "Sent!" → "Ready"
  if (statusResetAt && millis() > statusResetAt) {
    drawScreen("Ready", COL_STATUS_OK);
    statusResetAt = 0;
  }

  delay(2);
}
