// =============================================================
//  theme.h  —  SecureKey monochrome theme (white / black / gray)
// =============================================================
#pragma once

#include <Arduino.h>

// ---------------- Monochrome palette ----------------
// Pure B/W/Gray — no chroma. Premium minimalist.
#define C_BLACK    0x0000
#define C_GRAY_1   0x10A2    // very dark (cards, ghost segments)
#define C_GRAY_2   0x2945    // dark grey (borders, separators)
#define C_GRAY_3   0x528A    // mid grey (secondary text)
#define C_GRAY_4   0x8C71    // light grey (status text)
#define C_GRAY_5   0xC618    // very light grey
#define C_WHITE    0xFFFF

#define LCD_GHOST  C_GRAY_1  // dim "off" 7-segment

// ---------------- Layout ----------------
#define STATUS_H   44       // top status bar height
#define SAFE_PAD   16       // safe-area margin from each edge (rounded corners)

// ---------------- Mock display values (frozen, like Apple's 9:41) -----------
#define MOCK_HOUR  10
#define MOCK_MIN   8
#define MOCK_DAY_NAME   "FRI"
#define MOCK_DATE       22
#define MOCK_MONTH_NAME "MAY"

// ---------------- Screen enum ----------------
enum Screen { SCR_WATCH, SCR_PIN, SCR_UNLOCKED };

// ---------------- 7-segment digit map ----------------
// bit0=a top, 1=g mid, 2=b TR, 3=c BR, 4=d bot, 5=e BL, 6=f TL
const uint8_t SEG7[10] = {
  0x7D, 0x0C, 0x37, 0x1F, 0x4E, 0x5B, 0x7B, 0x0D, 0x7F, 0x5F
};

// ---------------- Day / Month names ----------------
const char *DAYS[7]    = {"SUN","MON","TUE","WED","THU","FRI","SAT"};
const char *MONTHS[12] = {"JAN","FEB","MAR","APR","MAY","JUN","JUL",
                          "AUG","SEP","OCT","NOV","DEC"};
