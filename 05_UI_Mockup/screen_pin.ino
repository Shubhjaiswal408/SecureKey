// =============================================================
//  screen_pin.ino  —  PIN entry screen
//
//  Layout:
//    ┌──────────────────────────────────┐
//    │  BLE  14:23 FRI 22 MAY   78% ▭▭ │  ← status (with time/date)
//    │  ──────────────────────────────  │
//    │                                  │
//    │     ┌───────────────────────┐    │
//    │     │      ● ● ● ○          │    │  ← PIN input box
//    │     └───────────────────────┘    │
//    │                                  │
//    │     ┌────┐ ┌────┐ ┌────┐         │
//    │     │ 1  │ │ 2  │ │ 3  │         │
//    │     └────┘ └────┘ └────┘         │
//    │     ┌────┐ ┌────┐ ┌────┐         │  ← 3x3 grid
//    │     │ 4  │ │ 5  │ │ 6  │         │
//    │     └────┘ └────┘ └────┘         │
//    │     ┌────┐ ┌────┐ ┌────┐         │
//    │     │ 7  │ │ 8  │ │ 9  │         │
//    │     └────┘ └────┘ └────┘         │
//    │     ┌────┐ ┌────┐ ┌────┐         │
//    │     │ ⌫  │ │ 0  │ │ ✓  │         │  ← last row
//    │     └────┘ └────┘ └────┘         │
//    └──────────────────────────────────┘
// =============================================================

// Keypad geometry
struct PinKey { const char *label; uint8_t row, col; };
const PinKey PKEYS[12] = {
  {"1",0,0},{"2",0,1},{"3",0,2},
  {"4",1,0},{"5",1,1},{"6",1,2},
  {"7",2,0},{"8",2,1},{"9",2,2},
  {"BS",3,0},{"0",3,1},{"OK",3,2},
};

// Keypad geometry — recalculated to fit within 448px height
//   STATUS_H (44) + gap + input box (56) + gap + 4 rows (60) + 3 gaps (8) = 408
//   Leaves 40px bottom margin for rounded corners.
const int16_t PK_W  = 96;
const int16_t PK_H  = 60;
const int16_t PK_G  = 8;
const int16_t PK_X0 = (LCD_WIDTH - (3*PK_W + 2*PK_G)) / 2;   // = 32
const int16_t PK_Y0 = 144;                                    // moved up

int16_t pkX(uint8_t c) { return PK_X0 + c * (PK_W + PK_G); }
int16_t pkY(uint8_t r) { return PK_Y0 + r * (PK_H + PK_G); }

// ----- PIN input box -----
void drawPinInputBox() {
  int16_t bw = 280;
  int16_t bh = 56;
  int16_t bx = (LCD_WIDTH - bw) / 2;
  int16_t y  = STATUS_H + 18;

  // Double outline for premium feel
  gfx->drawRoundRect(bx,     y,     bw,     bh,     8, C_GRAY_2);
  gfx->drawRoundRect(bx + 1, y + 1, bw - 2, bh - 2, 7, C_GRAY_2);

  // PIN dots (with shake on wrong PIN)
  int16_t cx = LCD_WIDTH / 2;
  int16_t shake = (millis() < shakeUntil) ? ((millis() / 40) % 2 ? -8 : 8) : 0;
  for (int i = 0; i < 4; i++) {
    int16_t x  = cx - 60 + i * 40 + shake;
    int16_t cy = y + bh / 2;
    if (i < pinLen) {
      uint16_t col = (millis() < shakeUntil) ? C_GRAY_5 : C_WHITE;
      gfx->fillCircle(x, cy, 9, col);
      gfx->fillCircle(x, cy, 8, col);
    } else {
      gfx->drawCircle(x, cy, 9, C_GRAY_3);
    }
  }
}

// ----- Single keypad key -----
void drawPinKey(uint8_t i, bool pressed) {
  int16_t x = pkX(PKEYS[i].col);
  int16_t y = pkY(PKEYS[i].row);
  bool isOK = (strcmp(PKEYS[i].label, "OK") == 0);
  bool isBS = (strcmp(PKEYS[i].label, "BS") == 0);

  uint16_t bg, fg, border;
  if (pressed) {
    bg = C_WHITE;  fg = C_BLACK;  border = C_WHITE;
  } else if (isOK) {
    // OK = primary (inverted to stand out)
    bg = C_WHITE;  fg = C_BLACK;  border = C_WHITE;
  } else {
    bg = C_GRAY_1; fg = C_WHITE;  border = C_GRAY_2;
  }

  gfx->fillRoundRect(x, y, PK_W, PK_H, 6, bg);
  if (!pressed && !isOK) {
    gfx->drawRoundRect(x, y, PK_W, PK_H, 6, border);
  }

  // ---- Label / icon ----
  if (isBS) {
    // Backspace glyph: arrow body + "X" inside (white on dark)
    int16_t cx = x + PK_W/2;
    int16_t cy = y + PK_H/2;
    // arrow: triangle + rect tail
    gfx->fillTriangle(cx - 18, cy, cx - 6, cy - 11, cx - 6, cy + 11, fg);
    gfx->fillRect(cx - 6, cy - 7, 18, 14, fg);
    // crossbars (X)
    int16_t bg2 = bg;
    gfx->drawLine(cx - 2, cy - 5, cx + 8, cy + 5, bg2);
    gfx->drawLine(cx - 2, cy + 5, cx + 8, cy - 5, bg2);
    gfx->drawLine(cx - 2, cy - 4, cx + 8, cy + 6, bg2);
    gfx->drawLine(cx - 2, cy + 4, cx + 8, cy - 6, bg2);
  }
  else if (isOK) {
    // Checkmark
    int16_t cx = x + PK_W/2;
    int16_t cy = y + PK_H/2;
    for (int t = 0; t < 4; t++) {
      gfx->drawLine(cx - 16, cy + t,       cx - 4, cy + 12 + t, fg);
      gfx->drawLine(cx - 4,  cy + 12 + t,  cx + 18, cy - 8 + t, fg);
    }
  }
  else {
    // Digit
    gfx->setTextSize(4);
    gfx->setTextColor(fg);
    int16_t x1, y1; uint16_t w, h;
    gfx->getTextBounds(PKEYS[i].label, 0, 0, &x1, &y1, &w, &h);
    gfx->setCursor(x + (PK_W - (int16_t)w) / 2,
                   y + (PK_H - (int16_t)h) / 2);
    gfx->print(PKEYS[i].label);
  }
}

// ----- Full PIN screen -----
void drawPin() {
  gfx->fillScreen(C_BLACK);
  drawStatusBar(true);            // with time + date
  drawPinInputBox();
  for (uint8_t i = 0; i < 12; i++) drawPinKey(i, false);
  flushScreen();
}

// ----- Tap handler -----
void onTapPin(int16_t tx, int16_t ty) {
  for (uint8_t i = 0; i < 12; i++) {
    int16_t x = pkX(PKEYS[i].col), y = pkY(PKEYS[i].row);
    if (tx < x || tx >= x + PK_W) continue;
    if (ty < y || ty >= y + PK_H) continue;

    // Visual press feedback
    drawPinKey(i, true);
    flushScreen();
    delay(70);

    const char *k = PKEYS[i].label;
    if (strcmp(k, "BS") == 0) {
      if (pinLen) pinLen--;
    }
    else if (strcmp(k, "OK") == 0) {
      pinEntry[pinLen] = 0;
      if (strcmp(pinEntry, PIN_CORRECT) == 0) {
        // SUCCESS
        led.setPixelColor(0, 0x00FF00); led.show();
        pinLen = 0;
        current = SCR_UNLOCKED;
        unlockUntil = millis() + 900;
        drawAll();
        return;
      } else {
        // WRONG
        shakeUntil = millis() + 500;
        pinLen = 0;
        led.setPixelColor(0, 0xFF0000); led.show();
      }
    }
    else if (pinLen < 4) {
      pinEntry[pinLen++] = k[0];
    }

    drawPin();
    return;
  }
}
