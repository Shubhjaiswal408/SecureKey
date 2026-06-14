// =============================================================
//  screen_boot.ino  —  Apple-style splash screen
//
//  Draws the techiesms IC-chip logo on a black AMOLED background,
//  holds it for ~1.6 s, then returns so setup() can continue.
// =============================================================

// Light-blue chip color (matches the logo image)
#define LOGO_BLUE  ((uint16_t)0x4EDF)

void drawBootLogo() {
  gfx->fillScreen(C_BLACK);

  // ── IC body ──────────────────────────────────────────────────
  const int16_t ICW = 182, ICH = 182;
  const int16_t ICX = (LCD_WIDTH  - ICW) / 2;   // 93
  const int16_t ICY = (LCD_HEIGHT - ICH) / 2;   // 133
  const int16_t ICR = 30;

  // ── Pins (4 per side) ────────────────────────────────────────
  // Each pin is a short rectangle + a thin crossbar to give the
  // classic through-hole DIP look.
  const int16_t PH     = 14;   // pin height
  const int16_t PW     = 22;   // pin length (sticking out)
  const int16_t PG     = 13;   // gap between pins
  const int16_t PTOTAL = 4 * PH + 3 * PG;         // 98 px
  const int16_t PY0    = ICY + (ICH - PTOTAL) / 2;

  for (int i = 0; i < 4; i++) {
    int16_t py = PY0 + i * (PH + PG);
    // Left pin body
    gfx->fillRect(ICX - PW, py, PW, PH, LOGO_BLUE);
    // Left pin — thin crossbar (the + tip)
    gfx->fillRect(ICX - PW - 4, py + (PH - 4) / 2, PW + 4, 4, LOGO_BLUE);
    // Right pin body
    gfx->fillRect(ICX + ICW, py, PW, PH, LOGO_BLUE);
    // Right pin — thin crossbar
    gfx->fillRect(ICX + ICW, py + (PH - 4) / 2, PW + 4, 4, LOGO_BLUE);
  }

  // IC body (drawn over pin-bases so the join looks clean)
  gfx->fillRoundRect(ICX, ICY, ICW, ICH, ICR, LOGO_BLUE);

  // Pin-1 indicator — small black circle, inside top-left
  gfx->fillCircle(ICX + 20, ICY + 20, 9, C_BLACK);

  // ── "t" letter (white, hand-drawn for crispness) ─────────────
  // Vertical stroke
  const int16_t VX  = ICX + ICW / 2 - 9;
  const int16_t VY0 = ICY + 18;
  const int16_t VYB = ICY + ICH - 18;
  gfx->fillRect(VX, VY0, 18, VYB - VY0, C_WHITE);
  // Crossbar (at ~30 % from top of IC)
  const int16_t CBY  = ICY + ICH * 30 / 100;
  const int16_t CBXM = 24;
  gfx->fillRect(ICX + CBXM, CBY, ICW - 2 * CBXM, 20, C_WHITE);
  // Small rounded foot (serif-style bottom cap on the vertical)
  gfx->fillRoundRect(VX - 7, VYB - 8, 32, 8, 4, C_WHITE);

  // ── "®" — tiny, just outside top-right of IC body ────────────
  gfx->drawCircle(ICX + ICW + 11, ICY + 2, 8, LOGO_BLUE);
  gfx->setTextSize(1); gfx->setTextColor(LOGO_BLUE);
  gfx->setCursor(ICX + ICW + 7, ICY - 2);
  gfx->print("R");

  // ── Brand name ────────────────────────────────────────────────
  textCenter(ICY + ICH + 26, "techiesms", 2, LOGO_BLUE);

  flushScreen();
  delay(1600);   // hold splash for 1.6 s (similar to Apple boot logo)
}
