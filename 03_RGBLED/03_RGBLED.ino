/*
 * 03_RGBLED
 * ----------------------------------------------------------------
 * Test 3 of 4 — verifies the on-board WS2812 RGB LED.
 *
 * What you should see:
 *   - LED smoothly cycles through the rainbow
 *   - Brightness pulses
 *   - Status text printed every 500 ms on Serial Monitor
 *
 * Library: "Adafruit NeoPixel" (install from Library Manager)
 *
 * Later, this LED will be used as a SecureKey status indicator:
 *   GREEN  = idle / locked
 *   BLUE   = USB host enumerating
 *   YELLOW = waiting for user confirmation
 *   RED    = wrong PIN / auth failed
 *   PURPLE = secure operation in progress
 * ----------------------------------------------------------------
 */

#include <Arduino.h>
#include <Adafruit_NeoPixel.h>
#include "pin_config.h"

Adafruit_NeoPixel led(RGB_LED_COUNT, RGB_LED_PIN, NEO_GRB + NEO_KHZ800);

// HSV → RGB helper (h: 0..65535, s/v: 0..255)
uint32_t hsv(uint16_t h, uint8_t s = 255, uint8_t v = 255) {
  return led.gamma32(led.ColorHSV(h, s, v));
}

uint16_t hue       = 0;
uint8_t  pulseDir  = 1;
uint8_t  brightness = 30;       // keep modest — WS2812 at full white is ~60mA
uint32_t lastLogMs = 0;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[03_RGBLED] starting...");

  led.begin();
  led.setBrightness(brightness);
  led.show();   // ensure off at start

  // Quick power-on flash sequence (R → G → B) so you can confirm wiring
  uint32_t colors[] = {0xFF0000, 0x00FF00, 0x0000FF};
  for (int i = 0; i < 3; i++) {
    led.setPixelColor(0, colors[i]);
    led.show();
    delay(250);
  }
  led.clear(); led.show();
  Serial.println("Power-on flash done.");
}

void loop() {
  // Rainbow hue
  hue += 256;
  led.setPixelColor(0, hsv(hue));

  // Breathing brightness
  if (millis() % 30 == 0) {
    brightness += pulseDir;
    if (brightness >= 80 || brightness <= 5) pulseDir = -pulseDir;
    led.setBrightness(brightness);
  }

  led.show();

  // Heartbeat log every 500 ms
  if (millis() - lastLogMs > 500) {
    lastLogMs = millis();
    Serial.printf("hue=%5u  brightness=%3u\n", hue, brightness);
  }

  delay(10);
}
