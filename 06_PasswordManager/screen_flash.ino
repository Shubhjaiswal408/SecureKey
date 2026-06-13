// =============================================================
//  screen_flash.ino  —  Flashlight (full-white torch)
//
//  Fills the AMOLED white at max brightness.  Any tap exits and
//  restores the user's brightness setting.
// =============================================================

void drawFlash() {
  gfx->fillScreen(C_WHITE);

  // Faint hint so it's clear how to leave.
  const char *hint = "tap to exit";
  gfx->setTextSize(2); gfx->setTextColor(C_GRAY_2);
  int16_t x1, y1; uint16_t tw, th;
  gfx->getTextBounds(hint, 0, 0, &x1, &y1, &tw, &th);
  gfx->setCursor((LCD_WIDTH - (int16_t)tw) / 2, LCD_HEIGHT - 44);
  gfx->print(hint);

  flushScreen();
  out->Display_Brightness(255);     // full blast
}

void onTapFlash(int16_t /*tx*/, int16_t /*ty*/) {
  out->Display_Brightness(settings.brightness);   // restore
  popNav();
}
