// =============================================================
//  touch_input.ino  —  FT3168 capacitive touch + state machine
// =============================================================

// ----- Low-level FT3168 register read -----
uint8_t ftReadReg(uint8_t r) {
  Wire.beginTransmission((uint8_t)FT3168_ADDR);
  Wire.write(r);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)FT3168_ADDR, (uint8_t)1);
  return Wire.available() ? Wire.read() : 0;
}

bool ftReadTouch(uint16_t &x, uint16_t &y) {
  uint8_t n = ftReadReg(0x02);
  if (n == 0 || n > 5) return false;
  Wire.beginTransmission((uint8_t)FT3168_ADDR);
  Wire.write(0x03);
  Wire.endTransmission(false);
  Wire.requestFrom((uint8_t)FT3168_ADDR, (uint8_t)4);
  if (Wire.available() < 4) return false;
  uint8_t xh = Wire.read(), xl = Wire.read();
  uint8_t yh = Wire.read(), yl = Wire.read();
  x = ((xh & 0x0F) << 8) | xl;
  y = ((yh & 0x0F) << 8) | yl;
  return true;
}

// ----- High-level state machine -----
// Distinguishes a TAP (quick, small movement) from a SWIPE (>12 px drag).
// On release:
//   - if it was a tap → dispatchTap(startX, startY)
//   - if it was a drag → onSwipeEnd(totalDx, totalDy)
void pollTouch() {
  uint16_t x, y;
  bool active = ftReadTouch(x, y);

  if (active && !touching) {
    // Touch start
    touching   = true;
    isDrag     = false;
    tStartX    = tCurX = x;
    tStartY    = tCurY = y;
    tStartTime = millis();
  }
  else if (active && touching) {
    // Touch continues — check for drag threshold
    int16_t dx = (int16_t)x - (int16_t)tStartX;
    int16_t dy = (int16_t)y - (int16_t)tStartY;
    if (!isDrag && (abs(dx) > DRAG_THRESHOLD || abs(dy) > DRAG_THRESHOLD)) {
      isDrag = true;
    }
    tCurX = x;
    tCurY = y;
  }
  else if (!active && touching) {
    // Touch released
    touching = false;
    int16_t totalDx = (int16_t)tCurX - (int16_t)tStartX;
    int16_t totalDy = (int16_t)tCurY - (int16_t)tStartY;
    if (isDrag) {
      onSwipeEnd(totalDx, totalDy);
    } else if ((millis() - tStartTime) < 600) {
      dispatchTap(tStartX, tStartY);
    }
  }
}
