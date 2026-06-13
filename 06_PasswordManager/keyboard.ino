// =============================================================
//  keyboard.ino  —  Big single-tap GRID keyboard (monochrome)
//
//  Replaces the cramped 10-wide QWERTY.  Now a 6-column grid:
//  every letter is its own ~56px-wide key (was 33px), one tap
//  each — far easier to hit on the 1.8" screen.
//
//      a b c d e f
//      g h i j k l
//      m n o p q r
//      s t u v w x
//      y z [Aa] [__] [del] [OK]
//
//  Three layers cycled by the [Aa] key:  lower -> UPPER -> 123/sym.
//  The layer key's label shows what you'll GET if you tap it.
//
//  Public surface (unchanged — screen_add / screen_list rely on it):
//      kbReset()                    — clear buffer + layer
//      kbDraw(int16_t topY)         — render at given y
//      kbHandleTap(tx, ty)          — returns true if consumed; calls
//                                     onKeyboardSubmit() on OK
//      char     kbBuffer[];         — current text (NUL-terminated)
//      uint8_t  kbLen;
// =============================================================

// ── Grid geometry ─────────────────────────────────────────────
#define KB_COLS    6
#define KB_ROWS    5
#define KB_KEY_W   56          // was 33 — the width is the real fix
#define KB_KEY_H   44
#define KB_GAP     4
#define KB_X0      ((LCD_WIDTH - (KB_COLS*KB_KEY_W + (KB_COLS-1)*KB_GAP)) / 2)
#define KB_MAX_LEN 80

// Cell indices 0..25 = letters, 26 = layer, 27 = space, 28 = del, 29 = OK
#define KB_CELL_LAYER  26
#define KB_CELL_SPACE  27
#define KB_CELL_DEL    28
#define KB_CELL_OK     29

uint8_t kbLayer  = 0;       // 0=lower, 1=UPPER, 2=symbols
char    kbBuffer[KB_MAX_LEN + 1] = {0};
uint8_t kbLen    = 0;
int16_t kbTopY   = 0;       // set by kbDraw()

// Forward declaration of the host's submit handler (per screen).
void onKeyboardSubmit();

// ── Layer character maps (up to 26 cells each) ────────────────
//  Four layers, cycled by the layer key:
//    lower → UPPER → symbols 1/2 → symbols 2/2 → lower …
//  Between the two symbol pages, every printable ASCII symbol is
//  reachable (no more missing characters when building a password).
//  Shorter layers leave the trailing grid cells blank.
static const char *LAYER_CHARS[4] = {
  "abcdefghijklmnopqrstuvwxyz",     // 26
  "ABCDEFGHIJKLMNOPQRSTUVWXYZ",     // 26
  "1234567890@#$%&*-_+=()!?.,",     // 26  (symbols page 1)
  "/\\|:;'\"<>[]{}^~`",             // 16  (symbols page 2)
};
#define KB_LAYERS 4
// Label on the layer key = what you GET if you tap it (next layer).
static const char *LAYER_NEXT_LABEL[4] = { "ABC", "#+=", "2/2", "abc" };

void kbReset() {
  kbLayer = 0;
  kbBuffer[0] = 0;
  kbLen = 0;
}

// ── Suggestion dictionary ─────────────────────────────────────
// Common usernames/sites/URLs for one-tap autocomplete.
static const char *KB_DICT[] = {
  "amazon", "amazon.com", "amazon.in",
  "apple", "apple.com", "appleid",
  "dropbox", "dropbox.com",
  "epicgames", "epicgames.com",
  "facebook", "facebook.com",
  "flipkart", "flipkart.com",
  "github", "github.com",
  "gmail", "gmail.com",
  "google", "google.com",
  "hdfc", "hdfcbank.com",
  "icloud", "icloud.com",
  "instagram", "instagram.com",
  "jira", "jira.atlassian.com",
  "linkedin", "linkedin.com",
  "microsoft", "microsoft.com",
  "myntra", "myntra.com",
  "netflix", "netflix.com",
  "notion", "notion.so",
  "outlook", "outlook.com",
  "paypal", "paypal.com",
  "paytm", "paytm.com",
  "phonepe", "phonepe.com",
  "reddit", "reddit.com",
  "slack", "slack.com",
  "snapchat", "snapchat.com",
  "spotify", "spotify.com",
  "steam", "steampowered.com",
  "swiggy", "swiggy.com",
  "telegram", "t.me",
  "tinder", "tinder.com",
  "twitch", "twitch.tv",
  "twitter", "twitter.com",
  "uber", "uber.com",
  "whatsapp", "whatsapp.com",
  "x.com",
  "yahoo", "yahoo.com",
  "youtube", "youtube.com",
  "zerodha", "zerodha.com",
  "zomato", "zomato.com",
  "zoom", "zoom.us",
  "admin", "user", "test", "root", "shubh",
  "demo", "guest", "support",
  "shubh@gmail.com", "shubh@outlook.com",
  "www.", "https://", "http://",
};
static const int KB_DICT_SIZE = sizeof(KB_DICT) / sizeof(KB_DICT[0]);

const char *kbSuggestions[3] = {nullptr, nullptr, nullptr};
uint8_t     kbSuggestionCount = 0;

void kbUpdateSuggestions() {
  kbSuggestionCount = 0;
  kbSuggestions[0] = kbSuggestions[1] = kbSuggestions[2] = nullptr;
  if (kbLen == 0) return;
  for (int i = 0; i < KB_DICT_SIZE && kbSuggestionCount < 3; i++) {
    if (strncasecmp(KB_DICT[i], kbBuffer, kbLen) == 0
        && strlen(KB_DICT[i]) > (size_t)kbLen) {
      kbSuggestions[kbSuggestionCount++] = KB_DICT[i];
    }
  }
}

// Draw suggestions row immediately ABOVE the keyboard
void kbDrawSuggestions(int16_t y) {
  if (kbSuggestionCount == 0) return;
  int16_t totalW = LCD_WIDTH - 2*SAFE_PAD;
  int16_t pillW = (totalW - 2*8) / 3;   // 3 pills, 8 px gaps
  for (uint8_t i = 0; i < kbSuggestionCount; i++) {
    int16_t x = SAFE_PAD + i * (pillW + 8);
    gfx->fillRoundRect(x, y, pillW, 30, 6, C_GRAY_1);
    gfx->drawRoundRect(x, y, pillW, 30, 6, C_WHITE);
    gfx->setTextSize(1); gfx->setTextColor(C_WHITE);
    int16_t x1,y1; uint16_t tw,th;
    int maxChars = (pillW - 8) / 6;
    char buf[32];
    if ((int)strlen(kbSuggestions[i]) > maxChars && maxChars > 1) {
      strncpy(buf, kbSuggestions[i], maxChars - 1);
      buf[maxChars - 1] = 0;
      strcat(buf, "~");
      gfx->getTextBounds(buf, 0, 0, &x1, &y1, &tw, &th);
      gfx->setCursor(x + (pillW - (int16_t)tw)/2, y + 11);
      gfx->print(buf);
    } else {
      gfx->getTextBounds(kbSuggestions[i], 0, 0, &x1, &y1, &tw, &th);
      gfx->setCursor(x + (pillW - (int16_t)tw)/2, y + 11);
      gfx->print(kbSuggestions[i]);
    }
  }
}

bool kbHandleSuggestionTap(int16_t tx, int16_t ty, int16_t sugY) {
  if (ty < sugY || ty > sugY + 30) return false;
  int16_t totalW = LCD_WIDTH - 2*SAFE_PAD;
  int16_t pillW = (totalW - 2*8) / 3;
  for (uint8_t i = 0; i < kbSuggestionCount; i++) {
    int16_t x = SAFE_PAD + i * (pillW + 8);
    if (tx >= x && tx < x + pillW) {
      strncpy(kbBuffer, kbSuggestions[i], KB_MAX_LEN);
      kbBuffer[KB_MAX_LEN] = 0;
      kbLen = strlen(kbBuffer);
      kbUpdateSuggestions();
      return true;
    }
  }
  return false;
}

// ── Single key ────────────────────────────────────────────────
static void drawKey(int16_t x, int16_t y, int16_t w, int16_t h,
                    const char *label, bool emphasize) {
  uint16_t bg = emphasize ? C_WHITE  : C_GRAY_1;
  uint16_t fg = emphasize ? C_BLACK  : C_WHITE;
  gfx->fillRoundRect(x, y, w, h, 6, bg);
  gfx->drawRoundRect(x, y, w, h, 6, C_GRAY_2);

  if (!label || !label[0]) return;     // blank (space bar drawn by caller)

  uint8_t fs = (strlen(label) <= 1) ? 3 : 2;   // big single chars, smaller words
  gfx->setTextSize(fs); gfx->setTextColor(fg);
  int16_t x1,y1; uint16_t tw,th;
  gfx->getTextBounds(label, 0, 0, &x1, &y1, &tw, &th);
  gfx->setCursor(x + (w - (int16_t)tw)/2, y + (h - (int16_t)th)/2);
  gfx->print(label);
}

void kbDraw(int16_t topY) {
  kbTopY = topY;
  const char *chars = LAYER_CHARS[kbLayer];
  int clen = (int)strlen(chars);

  for (int cell = 0; cell < KB_COLS * KB_ROWS; cell++) {
    int c = cell % KB_COLS, r = cell / KB_COLS;
    int16_t x = KB_X0 + c * (KB_KEY_W + KB_GAP);
    int16_t y = topY  + r * (KB_KEY_H + KB_GAP);

    if (cell < 26) {
      // Blank cell when this layer has fewer than 26 characters.
      char buf[2] = { (cell < clen) ? chars[cell] : '\0', 0 };
      drawKey(x, y, KB_KEY_W, KB_KEY_H, buf, false);
    } else if (cell == KB_CELL_LAYER) {
      drawKey(x, y, KB_KEY_W, KB_KEY_H, LAYER_NEXT_LABEL[kbLayer], false);
    } else if (cell == KB_CELL_SPACE) {
      drawKey(x, y, KB_KEY_W, KB_KEY_H, "", false);
      gfx->fillRect(x + 12, y + KB_KEY_H/2 - 1, KB_KEY_W - 24, 3, C_WHITE);
    } else if (cell == KB_CELL_DEL) {
      drawKey(x, y, KB_KEY_W, KB_KEY_H, "del", false);
    } else { // OK — emphasized (white)
      drawKey(x, y, KB_KEY_W, KB_KEY_H, "OK", true);
    }
  }
}

bool kbHandleTap(int16_t tx, int16_t ty) {
  if (ty < kbTopY) return false;
  int row = (ty - kbTopY) / (KB_KEY_H + KB_GAP);
  if (row < 0 || row >= KB_ROWS) return false;          // below grid
  int col = (tx - KB_X0) / (KB_KEY_W + KB_GAP);
  if (col < 0) col = 0; if (col > KB_COLS - 1) col = KB_COLS - 1;  // snap edges

  int cell = row * KB_COLS + col;
  char emit = 0;

  if (cell < 26) {
    const char *chars = LAYER_CHARS[kbLayer];
    if (cell >= (int)strlen(chars)) return true;   // blank cell — ignore
    emit = chars[cell];
  } else if (cell == KB_CELL_LAYER) {
    kbLayer = (kbLayer + 1) % KB_LAYERS;  // lower -> UPPER -> sym1 -> sym2
    kbUpdateSuggestions();
    return true;
  } else if (cell == KB_CELL_SPACE) {
    emit = ' ';
  } else if (cell == KB_CELL_DEL) {
    if (kbLen > 0) { kbBuffer[--kbLen] = 0; }
    kbUpdateSuggestions();
    return true;
  } else { // OK
    onKeyboardSubmit();
    return true;
  }

  if (emit && kbLen < KB_MAX_LEN) {
    kbBuffer[kbLen++] = emit;
    kbBuffer[kbLen] = 0;
  }
  kbUpdateSuggestions();
  return true;
}
