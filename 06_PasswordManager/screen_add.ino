// =============================================================
//  screen_add.ino  —  Add / edit a password (multi-step form)
//
//  5 fields, one at a time:
//     TITLE → USER → PASS → URL → NOTE
//
//  Each "OK" on the keyboard advances to the next field.
//  After NOTE: appends a new PassRecord to FFat, reloads
//  the index, and returns directly to the Passwords list with
//  the new entry visible.
//
//  Critical: addInit() must be called by pushNav(SCR_ADD) to
//  reset state — without it the static record from a previous
//  session would leak into the new one.
// =============================================================

static uint8_t    addField = 0;
static PassRecord addRec;

static const char *FIELD_LABELS[5] = {
  "TITLE", "USERNAME", "PASSWORD", "URL", "NOTE"
};
static const char *FIELD_HINTS[5] = {
  "e.g. Amazon", "user@example.com", "your password",
  "www.example.com", "optional"
};

// ── Reset state before entering Add screen ───────────────────
void addInit() {
  memset(&addRec, 0, sizeof(addRec));
  addField = 0;
  kbReset();
}

// ── Enter Add screen pre-filled for editing an existing record ───
//  Caller sets editingId = rec.id first; the save path then rewrites that
//  record in place instead of appending a new one.
void addEditInit(const PassRecord &rec) {
  addRec   = rec;
  addField = 0;
  kbReset();
  loadFromField(0);     // prefill the first field (TITLE) into the kb buffer
}

// ── Save kb input into addRec for the current field ──────────
static void copyToField(uint8_t f, const char *src) {
  switch (f) {
    case 0: strncpy(addRec.title,    src, sizeof(addRec.title)    - 1); break;
    case 1: strncpy(addRec.username, src, sizeof(addRec.username) - 1); break;
    case 2: strncpy(addRec.password, src, sizeof(addRec.password) - 1); break;
    case 3: { strncpy(addRec.url,    src, sizeof(addRec.url)      - 1);
              strncpy(addRec.folder, src, sizeof(addRec.folder)   - 1);
              break; }
    case 4: strncpy(addRec.note,     src, sizeof(addRec.note)     - 1); break;
  }
}

// ── Load existing field value into kb buffer (for editing) ───
static void loadFromField(uint8_t f) {
  const char *src = "";
  switch (f) {
    case 0: src = addRec.title;    break;
    case 1: src = addRec.username; break;
    case 2: src = addRec.password; break;
    case 3: src = addRec.url;      break;
    case 4: src = addRec.note;     break;
  }
  strncpy(kbBuffer, src, KB_MAX_LEN);
  kbBuffer[KB_MAX_LEN] = 0;
  kbLen = strlen(kbBuffer);
}

// ── Save the new record and return to list ───────────────────
static void saveNewRecord() {
  if (editingId == 0) {
    // New — pick next id
    uint16_t maxId = 0;
    for (uint16_t i = 0; i < passwordCount; i++)
      if (passwordIndex[i].id > maxId) maxId = passwordIndex[i].id;
    addRec.id      = maxId + 1;
    addRec.deleted = 0;
    Serial.printf("[ADD] Saving new id=%u title='%s' user='%s' pass='%s' url='%s'\n",
                  addRec.id, addRec.title, addRec.username,
                  addRec.password, addRec.url);
    dbAppend(addRec);
  } else {
    // Edit — rewrite db replacing the matching record
    File src = FFat.open("/db.bin", "r");
    File dst = FFat.open("/db_tmp.bin", "w");
    PassRecord rec;
    while (src.available() >= RECORD_SIZE) {
      src.read((uint8_t *)&rec, RECORD_SIZE);
      if (rec.id == editingId) {
        addRec.id      = editingId;
        addRec.deleted = 0;
        dst.write((uint8_t *)&addRec, RECORD_SIZE);
      } else {
        dst.write((uint8_t *)&rec, RECORD_SIZE);
      }
    }
    src.close(); dst.close();
    FFat.remove("/db.bin");
    FFat.rename("/db_tmp.bin", "/db.bin");
  }
  dbLoadIndex();
  ledSet(0x00FF00, 350);
}

// ── Success splash (brief checkmark) ─────────────────────────
static void drawSavedSplash() {
  gfx->fillScreen(C_BLACK);
  drawStatusBar();
  int16_t cx = LCD_WIDTH/2, cy = LCD_HEIGHT/2 + 40;
  // Green-glow success check (banded halo + tick) — drawn BEFORE the
  // text so the glow can never paint over the "SAVED" glyphs.
  glowCircle(cx, cy + 4, 56, 34, lerp565(C_BLACK, 0x362B, 140));
  gfx->fillCircle(cx, cy + 4, 32, lerp565(C_BLACK, 0x362B, 110));
  gfx->drawCircle(cx, cy + 4, 32, 0x362B);
  for (int t = 0; t < 4; t++) {
    gfx->drawLine(cx - 14, cy + t,      cx - 4,  cy + 12 + t, C_WHITE);
    gfx->drawLine(cx - 4,  cy + 12 + t, cx + 16, cy - 8  + t, C_WHITE);
  }
  textCenter(LCD_HEIGHT/2 - 30, "SAVED", 4, C_WHITE);
  flushScreen();
}

// ── Called by keyboard.ino → onKeyboardSubmit (OK key) ───────
void addNextField() {
  // Required-field validation: TITLE (0), USER (1), PASS (2) cannot be
  // empty.  URL (3) and NOTE (4) are optional.
  bool isRequired = (addField <= 2);
  if (isRequired && kbLen == 0) {
    // No nag popup. The OK button simply stays white (not blue) to show the
    // field isn't ready yet — just ignore the tap.
    return;
  }

  copyToField(addField, kbBuffer);
  addField++;

  if (addField >= 5) {
    saveNewRecord();
    drawSavedSplash();
    delay(700);

    addField = 0;
    memset(&addRec, 0, sizeof(addRec));
    kbReset();

    // Return to passwords list (replace ADD on the nav stack)
    fadeOut();
    navTop = 1;
    navStack[0] = SCR_HOME;
    navStack[1] = SCR_LIST;
    current = SCR_LIST;
    listSearchMode = false;
    listQuery[0]   = 0;
    buildList();
    drawAll();
    fadeIn();
    return;
  }

  loadFromField(addField);
  drawAdd();
}

// ── Draw ──────────────────────────────────────────────────────
void drawAdd() {
  gfx->fillScreen(C_BLACK);
  drawStatusBar();

  char navT[32];
  snprintf(navT, sizeof(navT), "%s  %u/5",
           editingId == 0 ? "Add New" : "Edit", addField + 1);
  drawNavBar(navT, true, "OK");

  // OK button: white when the field is empty, blue once you've typed
  // something — a clear "ready" cue instead of a nagging popup.
  if (kbLen > 0) {
    int16_t bx = LCD_WIDTH - SAFE_PAD - 36, by = STATUS_H + 6;
    gfx->fillRoundRect(bx, by, 36, 34, 6, C_GRAY_1);
    gfx->drawRoundRect(bx, by, 36, 34, 6, C_BLUE);
    gfx->setTextSize(2); gfx->setTextColor(C_BLUE);
    gfx->setCursor(bx + 8, by + 8);
    gfx->print("OK");
  }

  // Field label
  textAt(SAFE_PAD, STATUS_H + NAV_H + 6, FIELD_LABELS[addField], 1, C_GRAY_3);

  // Input box
  int16_t y = STATUS_H + NAV_H + 22;
  int16_t x = SAFE_PAD;
  int16_t w = LCD_WIDTH - 2*SAFE_PAD;
  int16_t h = 44;

  gfx->fillRoundRect(x, y, w, h, 10, C_GRAY_1);
  gfx->drawRoundRect(x, y, w, h, 10, C_WHITE);

  // Value
  gfx->setTextSize(2);
  if (kbLen == 0) {
    gfx->setTextColor(C_GRAY_3);
    gfx->setCursor(x + 12, y + 14);
    gfx->print(FIELD_HINTS[addField]);
  } else {
    gfx->setTextColor(C_WHITE);
    textClipped(x + 12, y + 14, kbBuffer, 2, C_WHITE, w - 24);
  }
  // Cursor blink
  if ((millis() / 500) % 2) {
    int16_t tx = x + 12 + (int16_t)kbLen * 12;
    if (tx < x + w - 8) gfx->fillRect(tx + 2, y + 12, 2, 22, C_WHITE);
  }

  // (progress dots removed — the nav bar already shows the "N/5" step,
  //  and the taller grid keyboard now needs that vertical space)

  // Suggestions just above keyboard (only for non-password fields)
  if (addField != 2) kbDrawSuggestions(KB_TOP_Y - 36);

  kbDraw(KB_TOP_Y);
  flushScreen();
}

void onTapAdd(int16_t tx, int16_t ty) {
  // Back
  if (ty >= STATUS_H + 2 && ty < STATUS_H + NAV_H - 2
      && tx >= SAFE_PAD && tx < SAFE_PAD + 46) {
    addInit();
    popNav();
    return;
  }
  // "OK" in nav bar (alternate Done)
  if (ty >= STATUS_H + 2 && ty < STATUS_H + NAV_H - 2
      && tx >= LCD_WIDTH - SAFE_PAD - 40) {
    addNextField();
    return;
  }
  // Keyboard
  // Suggestion tap (above the keyboard) — only non-password fields
  if (addField != 2 && kbHandleSuggestionTap(tx, ty, KB_TOP_Y - 36)) {
    drawAdd();
    return;
  }
  if (kbHandleTap(tx, ty)) drawAdd();
}
