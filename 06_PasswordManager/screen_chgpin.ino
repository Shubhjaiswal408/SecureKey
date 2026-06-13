// =============================================================
//  screen_chgpin.ino  —  Change-PIN flow (3 steps)
//
//     Enter current PIN  →  Enter new PIN  →  Confirm new PIN
//
//  Self-contained keypad (doesn't reuse screen_pin's PKEYS, since
//  Arduino concatenates files alphabetically and this one comes
//  first — file-scope vars wouldn't be visible yet).
// =============================================================

static uint8_t  cpStep = 0;          // 0=current, 1=new, 2=confirm
static char     cpBuf[5]  = {0};
static uint8_t  cpLen     = 0;
static char     cpNew[5]  = {0};
static uint32_t cpShake   = 0;

void chgPinInit() {
  cpStep = 0; cpLen = 0; cpBuf[0] = 0; cpNew[0] = 0; cpShake = 0;
}

// ── Keypad geometry ───────────────────────────────────────────
static const char *CP_KEYS[12] = {
  "1","2","3","4","5","6","7","8","9","BS","0","OK"
};
#define CP_W   96
#define CP_H   54
#define CP_G   8
#define CP_X0  ((LCD_WIDTH - (3*CP_W + 2*CP_G)) / 2)
#define CP_Y0  158
static int16_t cpkX(uint8_t i) { return CP_X0 + (i % 3) * (CP_W + CP_G); }
static int16_t cpkY(uint8_t i) { return CP_Y0 + (i / 3) * (CP_H + CP_G); }

static void drawCpKey(uint8_t i, bool pressed) {
  int16_t x = cpkX(i), y = cpkY(i);
  const char *k = CP_KEYS[i];
  bool isOK = (strcmp(k, "OK") == 0);
  uint16_t bg = (pressed || isOK) ? C_WHITE  : C_GRAY_1;
  uint16_t fg = (pressed || isOK) ? C_BLACK  : C_WHITE;
  gfx->fillRoundRect(x, y, CP_W, CP_H, 8, bg);
  if (!pressed && !isOK) gfx->drawRoundRect(x, y, CP_W, CP_H, 8, C_GRAY_2);

  const char *lbl = (strcmp(k, "BS") == 0) ? "<" : k;
  uint8_t fs = (strlen(lbl) <= 1) ? 3 : 2;
  gfx->setTextSize(fs); gfx->setTextColor(fg);
  int16_t x1, y1; uint16_t tw, th;
  gfx->getTextBounds(lbl, 0, 0, &x1, &y1, &tw, &th);
  gfx->setCursor(x + (CP_W - (int16_t)tw) / 2, y + (CP_H - (int16_t)th) / 2);
  gfx->print(lbl);
}

void drawChgPin() {
  gfx->fillScreen(C_BLACK);
  drawStatusBar();
  drawNavBar("Change PIN", true, nullptr);

  const char *titles[3] = {
    "Enter current PIN", "Enter new PIN", "Confirm new PIN"
  };
  textCenter(STATUS_H + NAV_H + 8, titles[cpStep], 2, C_WHITE);

  // 4 dots
  int16_t cx = LCD_WIDTH / 2;
  int16_t dy = STATUS_H + NAV_H + 40;
  int16_t shake = (millis() < cpShake) ? ((millis() / 40) % 2 ? -8 : 8) : 0;
  for (int i = 0; i < 4; i++) {
    int16_t x = cx - 54 + i * 36 + shake;
    if (i < cpLen)
      gfx->fillCircle(x, dy, 9, (millis() < cpShake) ? C_GRAY_3 : C_WHITE);
    else
      gfx->drawCircle(x, dy, 9, C_GRAY_3);
  }

  for (uint8_t i = 0; i < 12; i++) drawCpKey(i, false);
  flushScreen();
}

// Process a completed 4-digit entry for the current step.
static void cpProcess() {
  cpBuf[4] = 0;
  if (cpStep == 0) {                       // verify current
    if (strcmp(cpBuf, settings.pin) == 0) {
      cpStep = 1; cpLen = 0; cpBuf[0] = 0;
    } else {
      cpShake = millis() + 500; cpLen = 0; cpBuf[0] = 0; ledSet(0xFF0000, 400);
    }
  } else if (cpStep == 1) {                // capture new
    strcpy(cpNew, cpBuf); cpStep = 2; cpLen = 0; cpBuf[0] = 0;
  } else {                                 // confirm
    if (strcmp(cpBuf, cpNew) == 0) {
      strncpy(settings.pin, cpNew, 4); settings.pin[4] = 0;
      saveSettings();
      gfx->fillScreen(C_BLACK); drawStatusBar();
      textCenter(LCD_HEIGHT/2 - 16, "PIN CHANGED", 3, C_WHITE);
      flushScreen(); ledSet(0x00FF00, 500); delay(1200);
      popNav(); return;
    } else {                               // mismatch — restart at "new"
      cpShake = millis() + 500;
      cpStep = 1; cpLen = 0; cpBuf[0] = 0; cpNew[0] = 0;
      ledSet(0xFF0000, 400);
    }
  }
  drawChgPin();
}

void onTapChgPin(int16_t tx, int16_t ty) {
  // Back
  if (ty >= STATUS_H + 2 && ty < STATUS_H + NAV_H - 2
      && tx >= SAFE_PAD && tx < SAFE_PAD + 46) {
    popNav(); return;
  }
  for (uint8_t i = 0; i < 12; i++) {
    int16_t x = cpkX(i), y = cpkY(i);
    if (tx < x || tx >= x + CP_W) continue;
    if (ty < y || ty >= y + CP_H) continue;

    drawCpKey(i, true); flushScreen(); delay(50);

    const char *k = CP_KEYS[i];
    if (strcmp(k, "BS") == 0) {
      if (cpLen) cpLen--;
      cpBuf[cpLen] = 0;
    } else if (strcmp(k, "OK") == 0) {
      if (cpLen == 4) { cpProcess(); return; }
    } else if (cpLen < 4) {
      cpBuf[cpLen++] = k[0]; cpBuf[cpLen] = 0;
      if (cpLen == 4) { cpProcess(); return; }
    }
    drawChgPin();
    return;
  }
}
