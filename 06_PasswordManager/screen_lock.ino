// =============================================================
//  screen_lock.ino  —  Branded lock screen (no clock)
//
//  The watch faces / date / time were removed by request. The lock
//  screen is now a simple branded splash: a padlock, the SecureKey
//  wordmark, and a "tap to unlock" pulse. One tap opens the PIN pad.
// =============================================================

// One-tap to unlock — skip the swipe gesture.
void onTapLock(int16_t /*tx*/, int16_t /*ty*/) {
  pushNav(SCR_PIN);
}

// ── Big padlock glyph centered at (cx, cy) ───────────────────
//  maskCol = the colour "behind" the glyph, used for the shackle mask
//  and keyhole cut-out so it sits cleanly on any background disc.
static void drawLockGlyph(int16_t cx, int16_t cy, uint16_t maskCol) {
  // Shackle (thick arc)
  for (int t = 0; t < 5; t++) gfx->drawCircle(cx, cy - 30, 24 - t, C_WHITE);
  gfx->fillRect(cx - 26, cy - 30, 52, 26, maskCol);  // mask lower half of ring
  // Body
  gfx->fillRoundRect(cx - 34, cy - 6, 68, 52, 10, C_WHITE);
  // Keyhole
  gfx->fillCircle(cx, cy + 14, 7, maskCol);
  gfx->fillRect(cx - 3, cy + 14, 6, 18, maskCol);
}

void drawLock() {
  gfx->fillScreen(C_BLACK);
  drawStatusBar();

  int16_t cx = LCD_WIDTH / 2;
  int16_t cy = LCD_HEIGHT / 2 - 52;

  // Breathing blue halo — phase advances with the 2 Hz lock repaint.
  // On the pure-black AMOLED this looks like the badge is softly lit.
  uint8_t  ph     = (millis() / 600) % 4;
  uint8_t  breath = (ph == 0) ? 90 : (ph == 1) ? 140 : (ph == 2) ? 180 : 140;
  glowCircle(cx, cy + 4, 98, 68, lerp565(C_BLACK, C_BLUE, breath));

  // App-icon style badge: dark filled disc with a 2px blue ring
  gfx->fillCircle(cx, cy + 4, 66, C_GRAY_1);
  gfx->drawCircle(cx, cy + 4, 67, C_BLUE);
  gfx->drawCircle(cx, cy + 4, 68, C_BLUE);

  drawLockGlyph(cx, cy, C_GRAY_1);

  textCenter(cy + 120, "SecureKey", 4, C_WHITE);

  // Subtle "tap to unlock" pulse
  uint8_t pulse = (millis() / 600) % 4;
  uint16_t col = pulse == 0 ? C_GRAY_3
               : pulse == 1 ? C_GRAY_5
               : pulse == 2 ? C_GRAY_4
                            : C_GRAY_2;
  textCenter(LCD_HEIGHT - 48, "tap to unlock", 2, col);

  flushScreen();
}
