/*
 * SecureKey UI v4 — Premium Monochrome
 * ------------------------------------------------------------------
 *  Two screens only (for now):
 *
 *    WATCH  —  status bar (BLE + battery) + huge time + date
 *              swipe UP to unlock
 *
 *    PIN    —  status bar (BLE + time/date + battery)
 *              input box + 3x3 digit grid + [⌫ 0 ✓] last row
 *              correct PIN (1234) → unlock toast → back to watch
 *
 *  Files in this sketch:
 *    05_UI_Mockup.ino   — main: globals, setup, loop, dispatchers
 *    theme.h            — colors, constants, lookups
 *    gfx_lib.ino        — text, 7-seg, icons, status bar
 *    touch_input.ino    — FT3168 reader + touch state machine
 *    screen_watch.ino   — watch face draw + interactions
 *    screen_pin.ino     — PIN keypad draw + interactions
 *    pin_config.h       — board pin mapping
 *
 *  Board: MaTouch ESP32-S3 AMOLED 1.8" FT3168
 *  USB Mode: Hardware CDC and JTAG
 *  PSRAM:    OPI PSRAM (required for canvas buffer)
 * ------------------------------------------------------------------
 */

#include <Arduino.h>
#include <Wire.h>
#include <Arduino_GFX_Library.h>
#include <Adafruit_NeoPixel.h>
#include "pin_config.h"
#include "theme.h"

// ===== Hardware globals (visible to all .ino files) ========================
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
    LCD_CS, LCD_SCK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3);
Arduino_GFX *out = new Arduino_SH8601(
    bus, LCD_RST, 0, false, LCD_WIDTH, LCD_HEIGHT);
Arduino_Canvas *gfx = new Arduino_Canvas(LCD_WIDTH, LCD_HEIGHT, out);
Adafruit_NeoPixel led(RGB_LED_COUNT, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

inline void flushScreen() { gfx->flush(); }

// ===== State ===============================================================
Screen   current     = SCR_WATCH;
char     pinEntry[5] = {0};
uint8_t  pinLen      = 0;
const char *PIN_CORRECT = "1234";
uint32_t shakeUntil  = 0;     // wrong-PIN shake animation deadline
uint32_t unlockUntil = 0;     // brief "UNLOCKED" toast deadline

// Mock status indicators
uint8_t  battery     = 78;
bool     btConnected = true;

// Touch state (used by touch_input.ino)
bool     touching     = false;
uint16_t tStartX, tStartY, tCurX, tCurY;
uint32_t tStartTime;
bool     isDrag       = false;
const int16_t DRAG_THRESHOLD = 12;

// ===== Forward declarations (Arduino auto-protos sometimes miss these) =====
void drawWatch();
void drawPin();
void drawUnlocked();
void onTapPin(int16_t tx, int16_t ty);
void drawStatusBar(bool showDateTime);
void pollTouch();

// ===== Screen dispatch =====================================================
void drawAll() {
  switch (current) {
    case SCR_WATCH:    drawWatch();     break;
    case SCR_PIN:      drawPin();       break;
    case SCR_UNLOCKED: drawUnlocked();  break;
  }
}

void dispatchTap(int16_t tx, int16_t ty) {
  // Watch ignores taps — only swipe-up unlocks.
  if (current == SCR_PIN) onTapPin(tx, ty);
}

// Called by touch state machine when a drag ends.
void onSwipeEnd(int16_t totalDx, int16_t totalDy) {
  if (current == SCR_WATCH && totalDy < -40) {
    // Swipe up → PIN
    current = SCR_PIN;
    pinLen  = 0;
    drawAll();
  } else if (current == SCR_PIN && totalDy > 60) {
    // Swipe down → back to watch (cancel)
    current = SCR_WATCH;
    pinLen  = 0;
    drawAll();
  }
}

// ===== Setup ===============================================================
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n[SecureKey UI v4 — monochrome]");

  led.begin();
  led.setBrightness(40);
  led.clear(); led.show();

  pinMode(LCD_EN, OUTPUT);  digitalWrite(LCD_EN, HIGH);  delay(50);
  pinMode(TP_RST, OUTPUT);  digitalWrite(TP_RST, LOW);   delay(10);
  digitalWrite(TP_RST, HIGH); delay(100);

  if (!gfx->begin()) {
    Serial.println("ERROR: canvas init failed — check OPI PSRAM in Tools menu.");
    while (1) delay(1000);
  }
  out->Display_Brightness(160);

  Wire.begin(IIC_SDA, IIC_SCL, 400000);
  pinMode(USER_BTN_PIN, INPUT_PULLUP);

  Serial.printf("PSRAM free after canvas: %u bytes\n", ESP.getFreePsram());
  Serial.println("Default PIN: 1234");

  drawAll();
}

// ===== Loop ================================================================
void loop() {
  // 2 Hz redraw on watch face (drives colon blink + minute roll)
  static uint32_t lastWatchMs = 0;
  if (current == SCR_WATCH && millis() - lastWatchMs > 500) {
    lastWatchMs = millis();
    drawWatch();
  }

  // Shake animation on wrong PIN (~20 Hz only when active)
  static uint32_t lastShakeFrame = 0;
  if (millis() < shakeUntil && millis() - lastShakeFrame > 50) {
    lastShakeFrame = millis();
    if (current == SCR_PIN) drawPin();
  }

  // Unlock toast auto-clear → back to watch
  if (unlockUntil != 0 && millis() > unlockUntil) {
    unlockUntil = 0;
    current     = SCR_WATCH;
    drawAll();
  }

  // Physical SW2 = lock back to watch
  static bool btnLast = HIGH;
  bool btnNow = digitalRead(USER_BTN_PIN);
  if (btnLast == HIGH && btnNow == LOW) {
    if (current != SCR_WATCH) {
      current = SCR_WATCH;
      pinLen  = 0;
      drawAll();
    }
  }
  btnLast = btnNow;

  // Touch poll ~60 Hz
  static uint32_t lastPollMs = 0;
  if (millis() - lastPollMs > 16) {
    lastPollMs = millis();
    pollTouch();
  }

  delay(1);
}
