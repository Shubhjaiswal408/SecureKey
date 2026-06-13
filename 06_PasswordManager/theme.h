// =============================================================
//  theme.h  —  SecureKey  (pure monochrome)
// =============================================================
#pragma once
#include <Arduino.h>

// ── Monochrome palette ───────────────────────────────────────
#define C_BLACK    0x0000
#define C_GRAY_1   0x10A2   // very dark   (card bg)
#define C_GRAY_2   0x2945   // dark        (borders, separators)
#define C_GRAY_3   0x528A   // mid         (secondary text)
#define C_GRAY_4   0x8C71   // light       (status / hint text)
#define C_GRAY_5   0xC618   // very light
#define C_WHITE    0xFFFF

// One specific accent: the BLE icon turns blue when Bluetooth is on,
// so the user can tell BLE-pairing state at a glance.
#define C_BLUE     0x4D9F   // sky-blue (RGB565)

// Red accent: a favorited entry's heart fills red.
#define C_RED      0xF800   // pure red (RGB565)

// ── Layout constants ─────────────────────────────────────────
#define STATUS_H    44      // status bar height
#define NAV_H       48      // nav bar height
#define SAFE_PAD    16      // edge margin (rounded AMOLED corners)
#define LIST_TOP    (STATUS_H + NAV_H)   // 92  — where lists start

// Generic row sizes — bigger tap targets per user feedback
#define HOME_ITEM_H 54      // home menu rows (compact — fits 6 items)
#define LIST_ITEM_H 70      // password list rows
#define SET_ITEM_H  62      // settings rows

// Keyboard placement (shared between passwords + add screens)
// Grid: 200 + 5 rows*(44+4) - 4 = 436, leaving a 12px bottom margin (448px screen).
#define KB_TOP_Y    200     // y of first keyboard row (6-col grid)

// ── 7-segment (for lock-screen clock) ────────────────────────
const uint8_t SEG7[10] = {
  0x7D,0x0C,0x37,0x1F,0x4E,0x5B,0x7B,0x0D,0x7F,0x5F
};

// ── Screen states ─────────────────────────────────────────────
enum Screen {
  SCR_LOCK,            // branded lock screen
  SCR_PIN,             // 4-digit PIN
  SCR_HOME,            // home grid (Passwords / Add / Flashlight / Settings)
  SCR_LIST,            // password list
  SCR_DETAIL,          // password detail
  SCR_SEARCH,          // (vestigial — search merged into list)
  SCR_SEARCH_RES,      // (vestigial)
  SCR_ADD,             // add / edit password form
  SCR_SETTINGS,        // settings list
  SCR_CHGPIN,          // change-PIN flow
  SCR_FLASH,           // flashlight (full white)
  SCR_WIFI             // WiFi captive-portal import (shows AP credentials)
};

// ── Password record (256 bytes fixed) ─────────────────────────
struct __attribute__((packed)) PassRecord {
  uint16_t id;          //  2
  uint8_t  deleted;     //  1
  char     folder[24];  // 24   (currently unused — was folder colour)
  char     title[40];   // 40
  char     username[64];// 64
  char     password[48];// 48
  char     url[60];     // 60
  char     note[16];    // 16
  uint8_t  favorite;    //  1   (1 = favorite / heart)
};
static_assert(sizeof(PassRecord) == 256, "PassRecord must be 256 bytes");

// ── List index entry (64 bytes) ───────────────────────────────
struct __attribute__((packed)) ListItem {
  uint16_t id;          //  2
  uint8_t  favorite;    //  1
  char     folder[21];  // 21
  char     title[40];   // 40
};
static_assert(sizeof(ListItem) == 64, "ListItem must be 64 bytes");

// ── Capacity ─────────────────────────────────────────────────
//   Storage limit:   9 MB FATFS / 256 bytes    ≈ 36,000 records
//   PSRAM index :    8 MB / 64 bytes           ≈ 130,000 records
//   Cap at 30,000 — index uses 30000*64 ≈ 1.9 MB PSRAM + 30000*2
//   for the sort array ≈ 60 KB, well within the 8 MB OPI PSRAM, and
//   30000*256 = 7.3 MB fits the 9 MB FAT partition.
#define MAX_PASSWORDS   30000
#define RECORD_SIZE     256

// ── Settings stored in Preferences (NVS) ──────────────────────
struct UserSettings {
  uint8_t brightness;       // 20–250
  bool    bleEnabled;
  bool    usbHidEnabled;
  bool    doubleTapSleep;   // double-tap anywhere → screen off
  char    pin[5];           // 4 digits + null
  uint8_t autoLockSec;      // 0=off, else 15/30/60/120 s idle → lock
  bool    androidFix;       // swap @ / " keycodes for UK/Android BLE hosts
};
