// =============================================================
//  screen_home.ino  —  Home as a 2-per-row big-icon grid
//
//   ┌──────────┬──────────┐
//   │ Passwords│  Add New │
//   ├──────────┼──────────┤
//   │Favorites │ Settings │
//   └──────────┴──────────┘
//
//  Long-press any tile to enter REORDER mode (like arranging apps on a
//  phone): the tile lifts under your finger, drag it to another cell and
//  release — the rest shuffle to make room. Tap DONE to finish. The order
//  is saved to NVS so it survives reboots.
// =============================================================

struct HomeMenuItem { const char *label; Screen target; uint8_t iconType; };

#define HOME_N 4
const HomeMenuItem HOME_ITEMS[HOME_N] = {
  { "Passwords",  SCR_LIST,     0 },
  { "Add New",    SCR_ADD,      2 },
  { "Favorites",  SCR_LIST,     6 },   // iconType 6 → opens list in favorites-only mode
  { "Settings",   SCR_SETTINGS, 3 },
};

// Per-ITEM accent colour (follows the item as it moves between slots).
static const uint16_t HOME_ACCENT[HOME_N] = { 0x4D9F, 0x362B, 0xF2CD, 0x9ADE };

// ── Slot ↔ item mapping (the user-arranged order) ────────────
static uint8_t homeOrder[HOME_N] = { 0, 1, 2, 3 };   // homeOrder[slot] = item

// Reorder (drag-and-drop) state
static bool    homeReorder  = false;   // arrange mode on?
static int8_t  homeDragSlot = -1;      // slot currently picked up (-1 = none)
static int16_t homeDragX = 0, homeDragY = 0;   // finger position while dragging

// ── Grid geometry (2 columns x 2 rows, header above) ─────────
#define HOME_COLS    2
#define HOME_GAP     14
#define HOME_TOP     (STATUS_H + 62)     // leaves room for the header
#define HOME_TILE_W  ((LCD_WIDTH - 2*SAFE_PAD - HOME_GAP) / 2)   // 161
#define HOME_TILE_H  158
#define HOME_BIG     64       // icon footprint inside a tile

// DONE button (top-right, only in reorder mode)
#define HOME_DONE_W  72
#define HOME_DONE_H  32
#define HOME_DONE_X  (LCD_WIDTH - SAFE_PAD - HOME_DONE_W)
#define HOME_DONE_Y  (STATUS_H + 10)

static int16_t homeTileX(uint8_t slot) { return SAFE_PAD + (slot % HOME_COLS) * (HOME_TILE_W + HOME_GAP); }
static int16_t homeTileY(uint8_t slot) { return HOME_TOP + (slot / HOME_COLS) * (HOME_TILE_H + HOME_GAP); }

// ── Persistence ──────────────────────────────────────────────
void homeLoadOrder() {
  prefs.begin("skset", true);
  size_t got = prefs.getBytes("horder", homeOrder, HOME_N);
  prefs.end();
  // Validate: must be a permutation of 0..HOME_N-1, else reset to default.
  bool seen[HOME_N] = { false, false, false, false };
  bool ok = (got == HOME_N);
  for (uint8_t i = 0; ok && i < HOME_N; i++) {
    if (homeOrder[i] >= HOME_N || seen[homeOrder[i]]) ok = false;
    else seen[homeOrder[i]] = true;
  }
  if (!ok) for (uint8_t i = 0; i < HOME_N; i++) homeOrder[i] = i;
}
static void homeSaveOrder() {
  prefs.begin("skset", false);
  prefs.putBytes("horder", homeOrder, HOME_N);
  prefs.end();
}

// ── Big icon drawer (centered at cx,cy on a coloured disc) ───
//  bg = the disc colour behind the icon; all cut-outs use it so the
//  glyphs sit cleanly on the disc (no black boxes).
static void drawHomeIcon(int16_t cx, int16_t cy, uint8_t type, uint16_t bg) {
  switch (type) {
    case 0: { // padlock
      gfx->fillRoundRect(cx - 22, cy - 6, 44, 32, 6, C_WHITE);
      for (int t = 0; t < 4; t++) {
        gfx->drawCircle(cx, cy - 12, 15 - t, C_WHITE);   // shackle (thick arc)
      }
      gfx->fillRect(cx - 15, cy - 12, 30, 14, bg);       // mask lower half of ring
      gfx->fillCircle(cx, cy + 6, 4, bg);                 // keyhole
      gfx->fillRect(cx - 2, cy + 6, 4, 11, bg);
      break;
    }
    case 2: { // plus
      gfx->fillRoundRect(cx - 24, cy - 6, 48, 12, 3, C_WHITE);
      gfx->fillRoundRect(cx - 6, cy - 24, 12, 48, 3, C_WHITE);
      break;
    }
    case 3: { // gear
      for (int i = 0; i < 8; i++) {
        float a = i * 0.7854f;                            // 45° steps
        int16_t dx = (int16_t)(cos(a) * 24);
        int16_t dy = (int16_t)(sin(a) * 24);
        gfx->fillRect(cx + dx - 4, cy + dy - 4, 8, 8, C_WHITE);
      }
      gfx->fillCircle(cx, cy, 19, C_WHITE);
      gfx->fillCircle(cx, cy, 8,  bg);
      break;
    }
    case 5: { // flashlight (torch) — kept for completeness, no longer on home
      gfx->fillRect(cx - 8, cy - 4, 16, 28, C_WHITE);                       // handle
      gfx->fillTriangle(cx - 18, cy - 22, cx + 18, cy - 22, cx + 8, cy - 4, C_WHITE);
      gfx->fillTriangle(cx - 18, cy - 22, cx - 8, cy - 4, cx + 8, cy - 4, C_WHITE);
      gfx->fillRect(cx - 5, cy - 26, 10, 5, C_GRAY_4);                      // lens hint
      break;
    }
    case 6: { // favorites — big solid heart
      drawHeart(cx, cy, 22, true, C_WHITE, bg);
      break;
    }
  }
}

// ── Draw one tile at an explicit position ────────────────────
static void drawHomeTileAt(int16_t x, int16_t y, uint8_t item,
                           bool pressed, bool editable) {
  uint16_t ac = HOME_ACCENT[item];
  uint16_t bg = pressed ? C_GRAY_2 : C_GRAY_1;
  uint16_t bd = (pressed || editable) ? ac : C_GRAY_2;
  gfx->fillRoundRect(x, y, HOME_TILE_W, HOME_TILE_H, 20, bg);
  gfx->drawRoundRect(x, y, HOME_TILE_W, HOME_TILE_H, 20, bd);
  if (editable) gfx->drawRoundRect(x + 1, y + 1, HOME_TILE_W - 2, HOME_TILE_H - 2, 19, ac);

  // Icon on a duotone disc tinted with the item's accent colour
  int16_t icx = x + HOME_TILE_W / 2;
  int16_t icy = y + HOME_TILE_H / 2 - 16;
  uint16_t disc = lerp565(C_BLACK, ac, pressed ? 150 : 100);
  gfx->fillCircle(icx, icy, 42, disc);
  gfx->drawCircle(icx, icy, 42, ac);
  drawHomeIcon(icx, icy, HOME_ITEMS[item].iconType, disc);

  // Label centered near the bottom + short accent underline
  textCenter(y + HOME_TILE_H - 40, HOME_ITEMS[item].label, 2, C_WHITE,
             x + HOME_TILE_W / 2);
  gfx->fillRoundRect(icx - 14, y + HOME_TILE_H - 16, 28, 3, 1, ac);

  // Editable hint — three "grip" dots in the top-left corner
  if (editable) {
    for (int i = 0; i < 3; i++)
      gfx->fillCircle(x + 14 + i * 7, y + 14, 1, ac);
  }
}

// Convenience: draw the tile occupying a slot (maps slot → item + position).
static void drawHomeTileSlot(uint8_t slot, bool pressed) {
  drawHomeTileAt(homeTileX(slot), homeTileY(slot), homeOrder[slot],
                 pressed, homeReorder);
}

// Empty drop-target placeholder for the slot whose tile is being dragged.
static void drawHomePlaceholder(uint8_t slot) {
  int16_t x = homeTileX(slot), y = homeTileY(slot);
  gfx->fillRoundRect(x, y, HOME_TILE_W, HOME_TILE_H, 20, C_BLACK);
  // Dashed-ish border (short segments) to read as "drop here"
  for (int16_t i = 0; i < HOME_TILE_W; i += 12) {
    gfx->drawFastHLine(x + i, y, 6, C_GRAY_3);
    gfx->drawFastHLine(x + i, y + HOME_TILE_H - 1, 6, C_GRAY_3);
  }
  for (int16_t i = 0; i < HOME_TILE_H; i += 12) {
    gfx->drawFastVLine(x, y + i, 6, C_GRAY_3);
    gfx->drawFastVLine(x + HOME_TILE_W - 1, y + i, 6, C_GRAY_3);
  }
}

void drawHome() {
  gfx->fillScreen(C_BLACK);
  drawStatusBar();

  if (homeReorder) {
    textAt(SAFE_PAD, STATUS_H + 14, "Arrange", 3, C_WHITE);
    textAt(SAFE_PAD, STATUS_H + 42, "hold & drag tiles", 1, C_GRAY_4);
    // DONE button
    gfx->fillRoundRect(HOME_DONE_X, HOME_DONE_Y, HOME_DONE_W, HOME_DONE_H, 8, C_BLUE);
    textCenter(HOME_DONE_Y + HOME_DONE_H/2 - 8, "DONE", 2, C_BLACK,
               HOME_DONE_X + HOME_DONE_W/2);
  } else {
    textAt(SAFE_PAD, STATUS_H + 14, "My Vault", 3, C_WHITE);
    char sub[32];
    snprintf(sub, sizeof(sub), "%u passwords secured", passwordCount);
    textAt(SAFE_PAD, STATUS_H + 42, sub, 1, C_GRAY_4);
  }

  // Static tiles (skip the one being dragged — draw a placeholder instead)
  for (uint8_t slot = 0; slot < HOME_N; slot++) {
    if (homeReorder && slot == homeDragSlot) drawHomePlaceholder(slot);
    else                                     drawHomeTileSlot(slot, false);
  }

  // Floating tile under the finger, drawn last so it sits on top
  if (homeReorder && homeDragSlot >= 0) {
    int16_t fx = homeDragX - HOME_TILE_W / 2;
    int16_t fy = homeDragY - HOME_TILE_H / 2;
    if (fx < 0) fx = 0;
    if (fx > LCD_WIDTH - HOME_TILE_W) fx = LCD_WIDTH - HOME_TILE_W;
    if (fy < STATUS_H) fy = STATUS_H;
    if (fy > LCD_HEIGHT - HOME_TILE_H) fy = LCD_HEIGHT - HOME_TILE_H;
    drawHomeTileAt(fx, fy, homeOrder[homeDragSlot], true, false);
  }

  flushScreen();
}

// ── Reorder helpers (called from the touch layer) ────────────
bool homeInReorder()  { return homeReorder; }
bool homeIsDragging() { return homeReorder && homeDragSlot >= 0; }

// Which slot contains a point (returns -1 if none).
static int8_t homeSlotAt(int16_t tx, int16_t ty) {
  for (uint8_t slot = 0; slot < HOME_N; slot++) {
    int16_t x = homeTileX(slot), y = homeTileY(slot);
    if (tx >= x && tx < x + HOME_TILE_W && ty >= y && ty < y + HOME_TILE_H)
      return (int8_t)slot;
  }
  return -1;
}

// Nearest slot to a point (by centre distance) — used when dropping.
static uint8_t homeNearestSlot(int16_t tx, int16_t ty) {
  uint8_t best = 0; int32_t bestD = INT32_MAX;
  for (uint8_t slot = 0; slot < HOME_N; slot++) {
    int16_t cxs = homeTileX(slot) + HOME_TILE_W / 2;
    int16_t cys = homeTileY(slot) + HOME_TILE_H / 2;
    int32_t dx = tx - cxs, dy = ty - cys;
    int32_t d = dx * dx + dy * dy;
    if (d < bestD) { bestD = d; best = slot; }
  }
  return best;
}

// Long-press picked up a tile → enter reorder mode and lift that tile.
void homeLongPress(int16_t tx, int16_t ty) {
  int8_t slot = homeSlotAt(tx, ty);
  if (slot < 0) return;             // not on a tile — ignore
  homeReorder  = true;
  homeDragSlot = slot;
  homeDragX = tx; homeDragY = ty;
  ledSet(0x0000FF, 150);
  drawHome();
}

// Finger moved while holding a tile.
void homeDrag(int16_t tx, int16_t ty) {
  if (homeDragSlot < 0) return;
  homeDragX = tx; homeDragY = ty;
  drawHome();
}

// Released → drop into the nearest slot, shuffling the rest to make room.
void homeRelease(int16_t tx, int16_t ty) {
  if (homeDragSlot < 0) return;
  uint8_t from = (uint8_t)homeDragSlot;
  uint8_t to   = homeNearestSlot(tx, ty);
  if (to != from) {
    uint8_t moved = homeOrder[from];
    if (from < to) for (uint8_t i = from; i < to; i++) homeOrder[i] = homeOrder[i + 1];
    else           for (uint8_t i = from; i > to; i--) homeOrder[i] = homeOrder[i - 1];
    homeOrder[to] = moved;
    homeSaveOrder();
    ledSet(0x00FF00, 150);
  }
  homeDragSlot = -1;
  drawHome();                        // stay in reorder mode for more moves
}

// Leave reorder mode (DONE button, or forced on navigate-away / lock).
void homeExitReorder() {
  if (!homeReorder && homeDragSlot < 0) return;
  bool wasOn = homeReorder;
  homeReorder  = false;
  homeDragSlot = -1;
  homeSaveOrder();
  if (wasOn && current == SCR_HOME) drawHome();
}

// ── Tap dispatch ─────────────────────────────────────────────
void onTapHome(int16_t tx, int16_t ty) {
  // In reorder mode, the only tap target is DONE; everything else is ignored
  // (tiles are moved by long-press + drag, not by tapping).
  if (homeReorder) {
    if (tx >= HOME_DONE_X && tx < HOME_DONE_X + HOME_DONE_W &&
        ty >= HOME_DONE_Y && ty < HOME_DONE_Y + HOME_DONE_H) {
      homeExitReorder();
    }
    return;
  }

  int8_t slot = homeSlotAt(tx, ty);
  if (slot < 0) return;
  uint8_t item = homeOrder[slot];

  drawHomeTileSlot(slot, true);
  flushScreen(); delay(60);

  if (HOME_ITEMS[item].target == SCR_ADD) { editingId = 0; addInit(); }
  // Passwords vs Favorites both open SCR_LIST; the heart tile (iconType 6)
  // switches the list into favorites-only mode. Set BEFORE pushNav, which
  // calls buildList().
  if (HOME_ITEMS[item].target == SCR_LIST)
    listFavOnly = (HOME_ITEMS[item].iconType == 6);
  pushNav(HOME_ITEMS[item].target);
}
