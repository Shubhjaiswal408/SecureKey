// =============================================================
//  gfx_lib.ino  —  Shared drawing helpers  (monochrome)
// =============================================================

// ── Colour engine ────────────────────────────────────────────
//  lerp565: blend two RGB565 colours, t = 0..255 (0 → a, 255 → b).
uint16_t lerp565(uint16_t a, uint16_t b, uint8_t t) {
  int16_t ar = (a >> 11) & 0x1F, ag = (a >> 5) & 0x3F, ab = a & 0x1F;
  int16_t br = (b >> 11) & 0x1F, bg = (b >> 5) & 0x3F, bb = b & 0x1F;
  uint16_t r = ar + ((br - ar) * t) / 255;
  uint16_t g = ag + ((bg - ag) * t) / 255;
  uint16_t bl = ab + ((bb - ab) * t) / 255;
  return (r << 11) | (g << 5) | bl;
}

//  Radial glow: concentric filled bands fading from black (outer) to
//  `col` (inner).  On the pure-black AMOLED this reads as a light halo.
void glowCircle(int16_t cx, int16_t cy, int16_t rOuter, int16_t rInner,
                uint16_t col) {
  const int BANDS = 8;
  for (int i = 0; i <= BANDS; i++) {
    int16_t r = rOuter - (int32_t)(rOuter - rInner) * i / BANDS;
    uint16_t c = lerp565(C_BLACK, col, (uint8_t)((int32_t)i * 255 / BANDS));
    gfx->fillCircle(cx, cy, r, c);
  }
}

//  Curated avatar palette — vivid but balanced on black (RGB565).
static const uint16_t AV_COLS[6] = {
  0x4D9F,   // sky blue
  0x2DD5,   // teal
  0x9ADE,   // purple
  0xF4E5,   // orange
  0x362B,   // green
  0xF2CD,   // coral
};
uint16_t avatarColor(char letter) {
  return AV_COLS[(uint8_t)toupper(letter) % 6];
}

// ── Text helpers ─────────────────────────────────────────────
void textAt(int16_t x, int16_t y, const char *s,
            uint8_t sz, uint16_t col) {
  gfx->setTextSize(sz); gfx->setTextColor(col);
  gfx->setCursor(x, y); gfx->print(s);
}

void textCenter(int16_t y, const char *s, uint8_t sz,
                uint16_t col, int16_t cx) {
  int16_t x1, y1; uint16_t w, h;
  gfx->setTextSize(sz); gfx->setTextColor(col);
  gfx->getTextBounds(s, 0, 0, &x1, &y1, &w, &h);
  gfx->setCursor(cx - (int16_t)w / 2, y);
  gfx->print(s);
}

// Clip text to maxW pixels; append "~" if truncated
void textClipped(int16_t x, int16_t y, const char *s,
                 uint8_t sz, uint16_t col, uint16_t maxW) {
  gfx->setTextSize(sz); gfx->setTextColor(col);
  uint16_t charW = 6 * sz;
  int maxChars = maxW / charW;
  if ((int)strlen(s) <= maxChars) {
    gfx->setCursor(x, y); gfx->print(s);
  } else {
    char buf[64];
    int n = min(maxChars - 1, 62);
    strncpy(buf, s, n); buf[n] = 0;
    strcat(buf, "~");
    gfx->setCursor(x, y); gfx->print(buf);
  }
}

// ── 7-segment digit ──────────────────────────────────────────
void draw7Seg(int16_t x, int16_t y, int16_t w, int16_t h,
              int8_t digit, uint16_t on, uint16_t off) {
  uint8_t mask = (digit >= 0 && digit <= 9) ? SEG7[(uint8_t)digit] : 0;
  int16_t t = max((int16_t)3, (int16_t)(h / 10));
  int16_t g = 1;
  uint16_t a   = (mask & 0x01) ? on : off;
  uint16_t mid = (mask & 0x02) ? on : off;
  uint16_t b   = (mask & 0x04) ? on : off;
  uint16_t c   = (mask & 0x08) ? on : off;
  uint16_t d   = (mask & 0x10) ? on : off;
  uint16_t e   = (mask & 0x20) ? on : off;
  uint16_t f   = (mask & 0x40) ? on : off;

  gfx->fillRect(x + t + g,     y,              w - 2*t - 2*g, t,   a);
  gfx->fillRect(x + t + g,     y + h - t,      w - 2*t - 2*g, t,   d);
  gfx->fillRect(x + t + g,     y + h/2 - t/2,  w - 2*t - 2*g, t,   mid);
  gfx->fillRect(x,              y + t + g,      t, h/2 - t - g - t/2, f);
  gfx->fillRect(x,              y + h/2 + t/2 + g, t, h/2 - t - g - t/2, e);
  gfx->fillRect(x + w - t,     y + t + g,      t, h/2 - t - g - t/2, b);
  gfx->fillRect(x + w - t,     y + h/2 + t/2 + g, t, h/2 - t - g - t/2, c);
}

void draw7SegColon(int16_t x, int16_t y, int16_t h,
                   uint16_t on, bool blink_on) {
  int16_t t = max((int16_t)4, (int16_t)(h / 10));
  uint16_t c = blink_on ? on : C_GRAY_2;
  gfx->fillRect(x, y + h/3 - t/2,   t, t, c);
  gfx->fillRect(x, y + 2*h/3 - t/2, t, t, c);
}

// ── Key icon (42×42, used by list rows) ──────────────────────
void drawKeyIcon(int16_t x, int16_t y) {
  // Background rounded square
  gfx->fillRoundRect(x, y, 42, 42, 10, C_GRAY_1);
  gfx->drawRoundRect(x, y, 42, 42, 10, C_GRAY_2);
  // Key ring (3 px thick circle)
  int16_t cx = x + 14, cy = y + 21;
  gfx->drawCircle(cx, cy, 7, C_WHITE);
  gfx->drawCircle(cx, cy, 6, C_WHITE);
  gfx->drawCircle(cx, cy, 5, C_WHITE);
  // Key shaft
  gfx->fillRect(cx + 5, cy - 2, 19, 5, C_WHITE);
  // Key teeth
  gfx->fillRect(cx + 17, cy - 7, 4, 5, C_WHITE);
  gfx->fillRect(cx + 23, cy - 7, 4, 5, C_WHITE);
}

// ── Heart icon ───────────────────────────────────────────────
//  filled=true  → solid heart (used red for a favorite)
//  filled=false → outline only (hollowed out with the bg colour)
static void heartFill(int16_t cx, int16_t cy, int16_t r, uint16_t col) {
  gfx->fillCircle(cx - r/2, cy - r/3, (r + 1)/2, col);
  gfx->fillCircle(cx + r/2, cy - r/3, (r + 1)/2, col);
  gfx->fillTriangle(cx - r, cy - r/5, cx + r, cy - r/5, cx, cy + r, col);
}
void drawHeart(int16_t cx, int16_t cy, int16_t r, bool filled,
               uint16_t col, uint16_t bg) {
  heartFill(cx, cy, r, col);
  if (!filled) heartFill(cx, cy, r - 3, bg);   // hollow → outline
}

// ── BLE icon ─────────────────────────────────────────────────
void drawBleIcon(int16_t x, int16_t y, uint16_t col) {
  int16_t cx = x + 7;
  gfx->drawFastVLine(cx,     y,     20, col);
  gfx->drawFastVLine(cx + 1, y,     20, col);
  gfx->drawLine(cx,     y,       cx + 6, y + 5,   col);
  gfx->drawLine(cx + 6, y + 5,   cx - 3, y + 10,  col);
  gfx->drawLine(cx,     y + 1,   cx + 6, y + 6,   col);
  gfx->drawLine(cx - 3, y + 10,  cx + 6, y + 15,  col);
  gfx->drawLine(cx + 6, y + 15,  cx,     y + 20,  col);
  gfx->drawLine(cx - 3, y + 9,   cx + 6, y + 14,  col);
}

// ── Status bar (SecureKey wordmark on left, BLE + WiFi on right) ──────
//  No clock, no battery — both removed by request.  The only right-side
//  indicators are the Bluetooth icon and a live "WiFi" tag during import.
void drawStatusBar() {
  const int16_t BLE_W = 14, BLE_H = 20;
  int16_t right = LCD_WIDTH - SAFE_PAD;

  // BLE icon — grey when Bluetooth is off, blue when on but not paired,
  // white when a host is connected.
  int16_t ble_x = right - BLE_W;
  int16_t ble_y = (STATUS_H - BLE_H) / 2;
  uint16_t bleCol = settings.bleEnabled
                    ? (btConnected ? C_WHITE : C_BLUE)
                    : C_GRAY_3;
  drawBleIcon(ble_x, ble_y, bleCol);

  // WiFi indicator — blue "WiFi" when the import portal is live.
  if (wifiPortalActive()) {
    gfx->setTextSize(1); gfx->setTextColor(C_BLUE);
    gfx->setCursor(ble_x - 38, (STATUS_H - 8) / 2);
    gfx->print("WiFi");
  }

  // Left: blue brand dot + wordmark (hidden on the lock screen,
  // which has its own big branding).
  if (current != SCR_LOCK) {
    gfx->fillCircle(SAFE_PAD + 4, STATUS_H / 2 - 1, 4, C_BLUE);
    gfx->setTextSize(1); gfx->setTextColor(C_GRAY_5);
    gfx->setCursor(SAFE_PAD + 15, (STATUS_H - 8) / 2);
    gfx->print("SecureKey");
  }

  // Bottom separator (gray, no accent line)
  gfx->drawFastHLine(SAFE_PAD, STATUS_H - 1,
                     LCD_WIDTH - 2 * SAFE_PAD, C_GRAY_2);
}

// ── Letter avatar (password list rows) ───────────────────────
//  46 px circle, colour picked from the letter (like contact apps),
//  white letter, red ring when favorited.
void drawAvatar(int16_t cx, int16_t cy, char letter, bool fav) {
  uint16_t col = avatarColor(letter);
  // Letter colour darkened toward black for the disc, full colour ring —
  // gives a subtle "duotone" depth instead of a flat blob.
  gfx->fillCircle(cx, cy, 23, lerp565(C_BLACK, col, 110));
  gfx->drawCircle(cx, cy, 23, fav ? C_RED : col);
  gfx->drawCircle(cx, cy, 22, fav ? C_RED : col);
  char b[2] = { (char)toupper(letter), 0 };
  gfx->setTextSize(3); gfx->setTextColor(C_WHITE);
  int16_t x1, y1; uint16_t w, h;
  gfx->getTextBounds(b, 0, 0, &x1, &y1, &w, &h);
  gfx->setCursor(cx - (int16_t)w / 2, cy - (int16_t)h / 2);
  gfx->print(b);
}

// ── Nav bar  (back chevron + title + optional right glyph) ────
void drawNavBar(const char *title, bool showBack,
                const char *rightLabel) {
  int16_t y = STATUS_H;

  if (showBack) {
    // Round back button with a real drawn chevron (not the "<" glyph)
    int16_t bcx = SAFE_PAD + 17, bcy = y + 23;
    gfx->fillCircle(bcx, bcy, 17, C_GRAY_1);
    gfx->drawCircle(bcx, bcy, 17, C_GRAY_2);
    for (int t = 0; t < 3; t++) {
      gfx->drawLine(bcx + 4 - t, bcy - 8, bcx - 5 - t, bcy,     C_WHITE);
      gfx->drawLine(bcx - 5 - t, bcy,     bcx + 4 - t, bcy + 8, C_WHITE);
    }
  }

  int16_t titleX = showBack ? SAFE_PAD + 46 : SAFE_PAD;
  int16_t titleW = LCD_WIDTH - titleX - SAFE_PAD
                   - (rightLabel ? 44 : 0);
  gfx->setTextSize(2); gfx->setTextColor(C_WHITE);
  gfx->setCursor(titleX, y + 14);
  uint16_t cw = 12;
  if (strlen(title) * cw > (uint16_t)titleW) {
    char buf[32];
    int n = min((int)(titleW / cw), 31);
    strncpy(buf, title, n); buf[n] = 0;
    gfx->print(buf);
  } else {
    gfx->print(title);
  }

  if (rightLabel) {
    gfx->fillRoundRect(LCD_WIDTH - SAFE_PAD - 36, y + 6, 36, 34, 6, C_GRAY_1);
    gfx->drawRoundRect(LCD_WIDTH - SAFE_PAD - 36, y + 6, 36, 34, 6, C_GRAY_2);
    gfx->setTextSize(2); gfx->setTextColor(C_WHITE);
    gfx->setCursor(LCD_WIDTH - SAFE_PAD - 28, y + 14);
    gfx->print(rightLabel);
  }

  // Bottom separator
  gfx->drawFastHLine(SAFE_PAD, y + NAV_H - 1,
                     LCD_WIDTH - 2 * SAFE_PAD, C_GRAY_2);
}
