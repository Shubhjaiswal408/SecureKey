// =============================================================
//  gfx_lib.ino  —  Shared drawing helpers (text, 7-seg, icons, status bar)
// =============================================================

// ===== Text ===============================================================
void textAt(int16_t x, int16_t y, const char *s, uint8_t sz, uint16_t col) {
  gfx->setTextSize(sz);
  gfx->setTextColor(col);
  gfx->setCursor(x, y);
  gfx->print(s);
}

void textCenter(int16_t y, const char *s, uint8_t sz, uint16_t col,
                int16_t cx = LCD_WIDTH/2) {
  int16_t x1, y1; uint16_t w, h;
  gfx->setTextSize(sz);
  gfx->setTextColor(col);
  gfx->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  gfx->setCursor(cx - (int16_t)w / 2, y);
  gfx->print(s);
}

// ===== 7-segment digit ====================================================
void draw7Seg(int16_t x, int16_t y, int16_t w, int16_t h, int8_t digit,
              uint16_t on, uint16_t off) {
  uint8_t mask = (digit >= 0 && digit <= 9) ? SEG7[(uint8_t)digit] : 0;
  int16_t t = max((int16_t)3, (int16_t)(h / 10));     // segment thickness
  int16_t g = 1;                                       // micro gap

  uint16_t a   = (mask & 0x01) ? on : off;
  uint16_t mid = (mask & 0x02) ? on : off;
  uint16_t b   = (mask & 0x04) ? on : off;
  uint16_t c   = (mask & 0x08) ? on : off;
  uint16_t d   = (mask & 0x10) ? on : off;
  uint16_t e   = (mask & 0x20) ? on : off;
  uint16_t f   = (mask & 0x40) ? on : off;

  // a: top
  gfx->fillRect(x + t + g, y, w - 2*t - 2*g, t, a);
  // d: bottom
  gfx->fillRect(x + t + g, y + h - t, w - 2*t - 2*g, t, d);
  // g: middle
  gfx->fillRect(x + t + g, y + h/2 - t/2, w - 2*t - 2*g, t, mid);
  // f: top-left
  gfx->fillRect(x, y + t + g, t, h/2 - t - g - t/2, f);
  // e: bottom-left
  gfx->fillRect(x, y + h/2 + t/2 + g, t, h/2 - t - g - t/2, e);
  // b: top-right
  gfx->fillRect(x + w - t, y + t + g, t, h/2 - t - g - t/2, b);
  // c: bottom-right
  gfx->fillRect(x + w - t, y + h/2 + t/2 + g, t, h/2 - t - g - t/2, c);
}

void draw7SegColon(int16_t x, int16_t y, int16_t h, uint16_t on, bool blink_on) {
  int16_t t = max((int16_t)4, (int16_t)(h / 10));
  uint16_t c = blink_on ? on : LCD_GHOST;
  gfx->fillRect(x, y + h/3 - t/2,     t, t, c);
  gfx->fillRect(x, y + 2*h/3 - t/2,   t, t, c);
}

// ===== BLE / Bluetooth icon ===============================================
void drawBleIcon(int16_t x, int16_t y, uint16_t col) {
  // 14×20 stylised "B" shape (Bluetooth logo)
  int16_t cx = x + 7;
  // vertical center stroke
  gfx->drawFastVLine(cx,     y,     20, col);
  gfx->drawFastVLine(cx + 1, y,     20, col);
  // top loop: cx,y → cx+6,y+5 → cx-3,y+10
  gfx->drawLine(cx,      y,        cx + 6,  y + 5,  col);
  gfx->drawLine(cx + 6,  y + 5,    cx - 3,  y + 10, col);
  gfx->drawLine(cx,      y + 1,    cx + 6,  y + 6,  col);
  // bottom loop: cx-3,y+10 → cx+6,y+15 → cx,y+20
  gfx->drawLine(cx - 3,  y + 10,   cx + 6,  y + 15, col);
  gfx->drawLine(cx + 6,  y + 15,   cx,      y + 20, col);
  gfx->drawLine(cx - 3,  y + 9,    cx + 6,  y + 14, col);
}

// ===== Battery icon =======================================================
void drawBatteryIcon(int16_t x, int16_t y, uint8_t pct, uint16_t col) {
  // 32×14 horizontal battery
  gfx->drawRoundRect(x, y, 32, 14, 2, col);
  gfx->fillRect(x + 32, y + 4, 3, 6, col);                   // nub
  uint8_t fillW = (uint8_t)((pct * 28) / 100);
  // empty inner
  gfx->fillRect(x + 2, y + 2, 28, 10, C_BLACK);
  // fill bar
  gfx->fillRect(x + 2, y + 2, fillW, 10, col);
}

// ===== Top status bar =====================================================
// Right cluster: BLE icon + battery icon, right-aligned within safe area.
// Left side: empty on watch, time+date on PIN.
void drawStatusBar(bool showDateTime) {
  // ----- Right cluster (BLE adjacent to battery) -----
  const int16_t BAT_W  = 32, BAT_H = 14;
  const int16_t BLE_W  = 14, BLE_H = 20;
  int16_t cluster_right = LCD_WIDTH - SAFE_PAD - 2;

  // Battery: rightmost
  int16_t bat_x = cluster_right - BAT_W;
  int16_t bat_y = (STATUS_H - BAT_H) / 2;
  drawBatteryIcon(bat_x, bat_y, battery, C_WHITE);

  // BLE: directly left of battery
  int16_t ble_x = bat_x - BLE_W - 10;
  int16_t ble_y = (STATUS_H - BLE_H) / 2;
  drawBleIcon(ble_x, ble_y, btConnected ? C_WHITE : C_GRAY_3);

  // ----- Left side: time + date (PIN screen only) -----
  // Text at size 2 (visible), vertically centered against icons.
  // At size 2, default font is 14 px tall → y = (STATUS_H - 14) / 2.
  if (showDateTime) {
    char buf[28];
    snprintf(buf, sizeof(buf),
             "%02u:%02u  %s %u %s",
             MOCK_HOUR, MOCK_MIN, MOCK_DAY_NAME, MOCK_DATE, MOCK_MONTH_NAME);
    textAt(SAFE_PAD, (STATUS_H - 14) / 2, buf, 2, C_WHITE);
  }

  // bottom separator (kept away from rounded corners)
  gfx->drawFastHLine(SAFE_PAD, STATUS_H - 1,
                     LCD_WIDTH - 2*SAFE_PAD, C_GRAY_2);
}
