// =============================================================
//  screen_list.ino  —  Passwords list with inline search
//
//  Top:    Status bar
//  Then:   Nav bar  "Passwords"    [🔍]   (toggles search)
//  Then:   Search input bar (always visible — like iOS)
//  Then:   Scrollable list (filtered by query in real time)
//  Bottom: Keyboard appears when search bar is tapped
// =============================================================

static uint16_t *listSortedIdx = nullptr;
static uint16_t  listCount     = 0;

bool             listSearchMode = false;        // keyboard up?
char             listQuery[64]  = {0};          // global (read from screen_add.ino)
bool             listFavOnly    = false;        // Favorites tile → show only hearts

// View geometry recomputed each draw — keyboard up shrinks the list
static int16_t   listTopY   = LIST_TOP;
static int16_t   listViewH  = LCD_HEIGHT - LIST_TOP;

static inline int32_t listMaxScroll() {
  int32_t total = (int32_t)listCount * LIST_ITEM_H - listViewH;
  return total > 0 ? total : 0;
}

static void listInit() {
  if (!listSortedIdx)
    listSortedIdx = (uint16_t *)ps_malloc(MAX_PASSWORDS * sizeof(uint16_t));
}

static int cmpTitle(const void *a, const void *b) {
  uint16_t ia = *(const uint16_t *)a;
  uint16_t ib = *(const uint16_t *)b;
  return strcasecmp(passwordIndex[ia].title, passwordIndex[ib].title);
}

// ── Build / filter the list ──────────────────────────────────
void buildList() {
  listInit();
  if (!listSortedIdx) return;
  listCount = 0;

  // Lowercase query
  char q[64]; int qlen = 0;
  for (; qlen < 63 && listQuery[qlen]; qlen++)
    q[qlen] = tolower(listQuery[qlen]);
  q[qlen] = 0;

  for (uint16_t i = 0; i < passwordCount; i++) {
    if (listFavOnly && !passwordIndex[i].favorite) continue;   // Favorites view
    if (qlen > 0) {
      char tlow[64];  int n = 0;
      for (; n < 63 && passwordIndex[i].title[n]; n++)
        tlow[n] = tolower(passwordIndex[i].title[n]);
      tlow[n] = 0;
      if (strstr(tlow, q) == nullptr) {
        // also check URL/folder
        char flow[24]; n = 0;
        for (; n < 23 && passwordIndex[i].folder[n]; n++)
          flow[n] = tolower(passwordIndex[i].folder[n]);
        flow[n] = 0;
        if (strstr(flow, q) == nullptr) continue;
      }
    }
    listSortedIdx[listCount++] = i;
  }
  qsort(listSortedIdx, listCount, sizeof(uint16_t), cmpTitle);
  listScrollY = 0; listVelocity = 0;
}

void listScroll(int16_t dy) {
  int32_t newY = listScrollY + (int32_t)dy;
  int32_t maxS = listMaxScroll();
  if (newY < 0)    newY = 0;
  if (newY > maxS) newY = maxS;
  listScrollY = newY;
  drawList();
}

// ── Search bar (above list) ──────────────────────────────────
#define SEARCH_BAR_Y    (STATUS_H + NAV_H + 6)
#define SEARCH_BAR_H    40
#define SEARCH_BAR_BOT  (SEARCH_BAR_Y + SEARCH_BAR_H)

static void drawSearchBar() {
  int16_t y = SEARCH_BAR_Y;
  int16_t x = SAFE_PAD;
  int16_t w = LCD_WIDTH - 2*SAFE_PAD;

  gfx->fillRoundRect(x, y, w, SEARCH_BAR_H, 10,
                     listSearchMode ? C_GRAY_2 : C_GRAY_1);
  gfx->drawRoundRect(x, y, w, SEARCH_BAR_H, 10,
                     listSearchMode ? C_BLUE : C_GRAY_2);

  // Magnifier glyph — blue while search is active
  uint16_t mc = listSearchMode ? C_BLUE : C_GRAY_4;
  int16_t cx = x + 14, cy = y + SEARCH_BAR_H/2;
  gfx->drawCircle(cx, cy - 1, 6, mc);
  gfx->drawCircle(cx, cy - 1, 5, mc);
  gfx->drawLine(cx + 4, cy + 3, cx + 9, cy + 8, mc);
  gfx->drawLine(cx + 5, cy + 3, cx + 9, cy + 7, mc);

  // Query or placeholder
  gfx->setTextSize(2);
  if (listQuery[0] == 0) {
    gfx->setTextColor(C_GRAY_3);
    gfx->setCursor(x + 32, y + 12);
    gfx->print("Search passwords");
  } else {
    gfx->setTextColor(C_WHITE);
    textClipped(x + 32, y + 12, listQuery, 2, C_WHITE, w - 60);
    // Clear (X) button on right when text present
    int16_t cxX = x + w - 18;
    int16_t cyX = y + SEARCH_BAR_H/2;
    gfx->drawLine(cxX - 5, cyX - 5, cxX + 5, cyX + 5, C_GRAY_4);
    gfx->drawLine(cxX - 5, cyX + 5, cxX + 5, cyX - 5, C_GRAY_4);
    gfx->drawLine(cxX - 4, cyX - 5, cxX + 5, cyX + 4, C_GRAY_4);
    gfx->drawLine(cxX - 4, cyX + 5, cxX + 5, cyX - 4, C_GRAY_4);
  }

  // Cursor blink when search mode active
  if (listSearchMode && (millis() / 500) % 2) {
    int16_t tx = x + 32 + (int16_t)strlen(listQuery) * 12;
    if (tx < x + w - 20) gfx->fillRect(tx + 2, y + 10, 2, 20, C_WHITE);
  }
}

// ── Draw a list row ─────────────────────────────────────────
static void drawListRow(int16_t rowY, const ListItem &item, bool pressed) {
  int16_t boxX = SAFE_PAD;
  int16_t boxW = LCD_WIDTH - 2*SAFE_PAD;
  int16_t boxH = LIST_ITEM_H - 6;

  // Corner radius 8 — matches the basic reference code (PIN box used 8).
  // The old 14 curved in far enough to clip the left avatar.
  uint16_t bg = pressed ? C_GRAY_2 : C_GRAY_1;
  gfx->fillRoundRect(boxX, rowY + 3, boxW, boxH, 8, bg);
  gfx->drawRoundRect(boxX, rowY + 3, boxW, boxH, 8, pressed ? C_BLUE : C_GRAY_2);

  // Letter avatar — first letter of the title (red ring = favorite)
  char first = item.title[0] ? item.title[0] : '?';
  drawAvatar(boxX + 34, rowY + 3 + boxH / 2, first, item.favorite != 0);

  gfx->setTextSize(2); gfx->setTextColor(C_WHITE);
  textClipped(boxX + 70, rowY + 12, item.title, 2, C_WHITE, boxW - 104);

  gfx->setTextSize(1); gfx->setTextColor(C_GRAY_4);
  textClipped(boxX + 70, rowY + 37, item.folder, 1, C_GRAY_4, boxW - 104);

  // Small red heart for favorited entries (left of the chevron)
  if (item.favorite)
    drawHeart(boxX + boxW - 44, rowY + 3 + boxH/2, 7, true, C_RED, bg);

  // Drawn chevron (thicker, cleaner than the '>' glyph)
  int16_t chx = boxX + boxW - 20, chy = rowY + 3 + boxH/2;
  for (int t = 0; t < 2; t++) {
    gfx->drawLine(chx - 4 + t, chy - 7, chx + 4 + t, chy,     C_GRAY_3);
    gfx->drawLine(chx + 4 + t, chy,     chx - 4 + t, chy + 7, C_GRAY_3);
  }
}

void drawList() {
  listInit();
  // NOTE: do NOT buildList() here — it resets listScrollY=0 and re-sorts
  // every frame, which broke scrolling and made it janky. The list is
  // (re)built only when data/query changes (pushNav, search, add/delete).

  // Compute view height — shrink if keyboard up
  listTopY  = SEARCH_BAR_BOT + 6;
  listViewH = (listSearchMode ? KB_TOP_Y - 6 : LCD_HEIGHT) - listTopY;

  gfx->fillScreen(C_BLACK);
  drawStatusBar();
  drawNavBar(listFavOnly ? "Favorites" : "Passwords", true, nullptr);
  drawSearchBar();

  if (listCount == 0) {
    bool favEmpty = (listFavOnly && !listQuery[0]);
    // Vertically center the message in the AVAILABLE area (above the keyboard
    // when search is up, otherwise the whole list area) — so it no longer
    // clings just under the search bar.
    int16_t areaBot = listSearchMode ? KB_TOP_Y : LCD_HEIGHT;
    int16_t midY    = listTopY + (areaBot - listTopY) / 2;
    if (favEmpty) {
      // Hollow heart illustration above the message
      drawHeart(LCD_WIDTH/2, midY - 44, 20, false, C_GRAY_3, C_BLACK);
      textCenter(midY, "No favorites yet", 2, C_GRAY_3);
      textCenter(midY + 26, "Tap the heart on a password", 1, C_GRAY_4);
    } else {
      textCenter(midY - 10, "No matches", 2, C_GRAY_3);
      if (listQuery[0])
        textCenter(midY + 14, listQuery, 1, C_GRAY_4);
    }
    if (listSearchMode) kbDraw(KB_TOP_Y);
    flushScreen();
    return;
  }

  int32_t firstItem = listScrollY / LIST_ITEM_H;
  int32_t lastItem  = (listScrollY + listViewH) / LIST_ITEM_H + 1;
  if (lastItem >= (int32_t)listCount) lastItem = (int32_t)listCount - 1;

  // Set clip via masked drawing (we just check bounds per row)
  for (int32_t i = firstItem; i <= lastItem; i++) {
    int16_t rowY = (int16_t)(listTopY + i * LIST_ITEM_H - listScrollY);
    if (rowY >= listTopY + listViewH) break;
    if (rowY + LIST_ITEM_H <= listTopY) continue;

    uint16_t idx = listSortedIdx[i];
    drawListRow(rowY, passwordIndex[idx], false);
  }

  // Scrollbar
  if ((int32_t)listCount * LIST_ITEM_H > listViewH) {
    int32_t calcH = (int32_t)listViewH * listViewH
                    / ((int32_t)listCount * LIST_ITEM_H);
    if (calcH < 20) calcH = 20;
    int16_t sbH = (int16_t)calcH;
    int16_t sbRange = listViewH - sbH;
    int32_t maxS = listMaxScroll();
    int16_t sbY = listTopY + (maxS > 0
                  ? (int16_t)((int32_t)sbRange * listScrollY / maxS) : 0);
    gfx->fillRect(LCD_WIDTH - 3, listTopY, 3, listViewH, C_GRAY_2);
    gfx->fillRect(LCD_WIDTH - 3, sbY,      3, sbH,        C_GRAY_4);
  }

  // Cover anything below the list view with black (keyboard area)
  if (listSearchMode) {
    gfx->fillRect(0, listTopY + listViewH, LCD_WIDTH,
                  LCD_HEIGHT - (listTopY + listViewH), C_BLACK);
    kbDrawSuggestions(KB_TOP_Y - 36);
    kbDraw(KB_TOP_Y);
  }

  flushScreen();
}

// ── Keyboard submit (Done key) ───────────────────────────────
void onKeyboardSubmit() {
  if (current == SCR_LIST) {
    // Hide keyboard, keep filter
    strncpy(listQuery, kbBuffer, sizeof(listQuery) - 1);
    listSearchMode = false;
    buildList();
    drawList();
  }
  else if (current == SCR_ADD) {
    extern void addNextField();
    addNextField();
  }
}

void onTapList(int16_t tx, int16_t ty) {
  // Back
  if (ty >= STATUS_H + 2 && ty < STATUS_H + NAV_H - 2
      && tx >= SAFE_PAD && tx < SAFE_PAD + 46) {
    if (listSearchMode) {
      listSearchMode = false;
      drawList();
    } else {
      popNav();
    }
    return;
  }

  // Search bar tap
  if (ty >= SEARCH_BAR_Y && ty < SEARCH_BAR_BOT) {
    // Tap X to clear
    if (listQuery[0] &&
        tx >= LCD_WIDTH - SAFE_PAD - 24 &&
        tx <= LCD_WIDTH - SAFE_PAD) {
      listQuery[0] = 0;
      kbBuffer[0]  = 0; kbLen = 0;
      buildList();
      drawList();
      return;
    }
    // Activate search mode
    listSearchMode = true;
    // Sync keyboard buffer with current query
    strncpy(kbBuffer, listQuery, sizeof(kbBuffer) - 1);
    kbLen = strlen(kbBuffer);
    drawList();
    return;
  }

  // Suggestion tap (above keyboard)
  if (listSearchMode && kbHandleSuggestionTap(tx, ty, KB_TOP_Y - 36)) {
    strncpy(listQuery, kbBuffer, sizeof(listQuery) - 1);
    buildList();
    drawList();
    return;
  }

  // Keyboard input
  if (listSearchMode && ty >= KB_TOP_Y) {
    if (kbHandleTap(tx, ty)) {
      // Live filter as user types
      strncpy(listQuery, kbBuffer, sizeof(listQuery) - 1);
      buildList();
      drawList();
    }
    return;
  }

  // List item tap
  if (ty < listTopY || ty >= listTopY + listViewH) return;
  if (!listSortedIdx || listCount == 0) return;

  int32_t clicked = ((int32_t)(ty - listTopY) + listScrollY) / LIST_ITEM_H;
  if (clicked < 0 || clicked >= (int32_t)listCount) return;

  uint16_t idx = listSortedIdx[clicked];
  int16_t rowY = (int16_t)(listTopY + clicked * LIST_ITEM_H - listScrollY);
  drawListRow(rowY, passwordIndex[idx], true);
  flushScreen(); delay(60);

  detailId = passwordIndex[idx].id;
  pushNav(SCR_DETAIL);
}
