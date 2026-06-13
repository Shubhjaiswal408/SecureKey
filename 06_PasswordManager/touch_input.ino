// =============================================================
//  touch_input.ino  —  FT3168 capacitive touch + state machine
//  Extended from 05_UI_Mockup to add live drag callback.
// =============================================================

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

void pollTouch() {
  static bool longFired = false;   // long-press already handled this touch?
  uint16_t x, y;
  bool active = ftReadTouch(x, y);

  if (active && !touching) {
    // ── Touch start ──────────────────────────────────────
    touching   = true;
    isDrag     = false;
    longFired  = false;
    tStartX    = tCurX = x;
    tStartY    = tCurY = y;
    tStartTime = millis();
    listVelocity = 0;   // kill inertia on new touch
    lastActivityMs = millis();   // refresh idle timer for auto-lock
  }
  else if (active && touching) {
    // ── Touch continues ──────────────────────────────────
    int16_t dx = (int16_t)x - (int16_t)tCurX;
    int16_t dy = (int16_t)y - (int16_t)tCurY;
    int16_t totalDx = (int16_t)x - (int16_t)tStartX;
    int16_t totalDy = (int16_t)y - (int16_t)tStartY;

    if (!isDrag && (abs(totalDx) > DRAG_THRESHOLD ||
                   abs(totalDy) > DRAG_THRESHOLD)) {
      isDrag = true;
    }

    // Long-press on the Home screen (stationary hold) → pick up a tile and
    // enter drag-to-reorder mode.
    if (!isDrag && !longFired && current == SCR_HOME &&
        (millis() - tStartTime) > 500) {
      longFired = true;
      homeLongPress((int16_t)tStartX, (int16_t)tStartY);
    }

    if (current == SCR_HOME && homeIsDragging()) {
      homeDrag(x, y);                 // a tile is following the finger
    } else if (isDrag) {
      onDrag(tCurX, tCurY, dx, dy);   // ← live scroll callback
    }
    tCurX = x;
    tCurY = y;
  }
  else if (!active && touching) {
    // ── Touch released ────────────────────────────────────
    touching = false;
    int16_t totalDx = (int16_t)tCurX - (int16_t)tStartX;
    int16_t totalDy = (int16_t)tCurY - (int16_t)tStartY;

    if (current == SCR_HOME && homeIsDragging()) {
      homeRelease((int16_t)tCurX, (int16_t)tCurY);   // drop the tile
    } else if (isDrag) {
      onSwipeEnd(totalDx, totalDy);
    } else if ((millis() - tStartTime) < 600) {
      dispatchTap((int16_t)tStartX, (int16_t)tStartY);
    }
  }
}
