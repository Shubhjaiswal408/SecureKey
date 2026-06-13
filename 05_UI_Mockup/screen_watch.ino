// =============================================================
//  screen_watch.ino  —  Lock screen / wallpaper
//
//  Layout (368×448, with SAFE_PAD margin for rounded corners):
//
//    ┌──────────────────────────────────┐
//    │                       ⓑ ▭▭▭▭ ▭ │ ← BLE + battery (right cluster)
//    │  ────────────────────────────    │
//    │                                  │
//    │                                  │
//    │                                  │
//    │           10:08                  │ ← BIG centered time
//    │                                  │   (vertically + horizontally)
//    │       FRI · 22 · MAY             │ ← date directly below
//    │                                  │
//    │                                  │
//    │              ─                   │
//    │              ↑                   │
//    │          SWIPE UP                │ ← unlock hint
//    └──────────────────────────────────┘
// =============================================================

void drawWatch() {
  gfx->fillScreen(C_BLACK);

  // ----- Top status bar (just BLE + battery, no time/date) -----
  drawStatusBar(false);

  // ============================================================
  //   BIG HH:MM 7-segment, perfectly centered both axes
  // ============================================================
  bool colonOn = (millis() / 500) % 2;

  // Digit metrics
  const int16_t DW = 54;       // digit width
  const int16_t DH = 110;      // digit height
  const int16_t DG = 10;       // gap between adjacent elements
  const int16_t CW = 16;       // colon column width

  // True total width of HH:MM layout
  //   [D][gap][D][gap][:][gap][D][gap][D]
  //   = 4*DW + 4*DG + CW
  const int16_t totalW = 4 * DW + 4 * DG + CW;     // 216 + 40 + 16 = 272

  // Horizontally centered
  int16_t baseX = (LCD_WIDTH - totalW) / 2;        // (368 - 272) / 2 = 48

  // Vertically centered in available area (between status bar and hint zone)
  // Usable: STATUS_H..390   center = (44+390)/2 = 217
  // Time digit center = baseY + DH/2 → baseY = 217 - 55 = 162
  int16_t baseY = 162;

  // HH (with shared step)
  int16_t step = DW + DG;
  draw7Seg(baseX + 0*step, baseY, DW, DH, MOCK_HOUR / 10, C_WHITE, LCD_GHOST);
  draw7Seg(baseX + 1*step, baseY, DW, DH, MOCK_HOUR % 10, C_WHITE, LCD_GHOST);

  // Colon (1 Hz blink)
  draw7SegColon(baseX + 2*step + (CW - 4)/2, baseY + 10, DH - 20, C_WHITE, colonOn);

  // MM (offset by colon column + one gap)
  int16_t mX = baseX + 2*step + CW + DG;
  draw7Seg(mX + 0*step, baseY, DW, DH, MOCK_MIN / 10, C_WHITE, LCD_GHOST);
  draw7Seg(mX + 1*step, baseY, DW, DH, MOCK_MIN % 10, C_WHITE, LCD_GHOST);

  // ============================================================
  //   Date — perfectly centered below time
  // ============================================================
  char dateStr[24];
  snprintf(dateStr, sizeof(dateStr), "%s   %u   %s",
           MOCK_DAY_NAME, MOCK_DATE, MOCK_MONTH_NAME);
  textCenter(baseY + DH + 22, dateStr, 2, C_GRAY_4);   // y = 294

  // ============================================================
  //   Bottom — subtle separator + pulsing chevron + hint
  // ============================================================
  // Symmetric to the top status bar separator
  gfx->drawFastHLine(LCD_WIDTH/2 - 24, 348, 48, C_GRAY_2);

  // Up-arrow chevron (gentle pulse)
  uint8_t pulse = (millis() / 600) % 3;
  int16_t ax = LCD_WIDTH / 2;
  int16_t ay = 376 + pulse;
  for (int i = 0; i < 2; i++) {
    gfx->drawLine(ax - 10 + i, ay + 9, ax,           ay,     C_GRAY_3);
    gfx->drawLine(ax,          ay,     ax + 10 - i, ay + 9, C_GRAY_3);
  }

  // Hint text, centered
  textCenter(410, "SWIPE UP", 2, C_GRAY_4);

  flushScreen();
}

// ----- "Unlocked" toast (after correct PIN, before returning to watch) -----
void drawUnlocked() {
  gfx->fillScreen(C_BLACK);
  drawStatusBar(false);

  textCenter(190, "UNLOCKED", 4, C_WHITE);

  // Big checkmark centered below text
  int16_t cx = LCD_WIDTH / 2, cy = 270;
  for (int t = 0; t < 4; t++) {
    gfx->drawLine(cx - 26, cy + t,        cx - 6, cy + 18 + t, C_WHITE);
    gfx->drawLine(cx - 6,  cy + 18 + t,   cx + 26, cy - 14 + t, C_WHITE);
  }

  flushScreen();
}
