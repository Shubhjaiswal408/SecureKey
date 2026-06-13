# SecureKey — Architecture & Feature Concepts

A deep dive into how SecureKey works, for contributors and the curious. For the high‑level pitch and setup, see the [README](../README.md).

---

## 1. The big picture

SecureKey is a single‑threaded Arduino sketch built around a **screen state machine** and a **non‑blocking main loop**. There is no RTOS scheduling in user code — instead `loop()` polls touch at ~60 Hz, runs lightweight per‑screen animation timers, and services background managers (BLE, Wi‑Fi portal, auto‑lock). The two HID stacks (USB via TinyUSB, BLE via NimBLE) run their own tasks under the hood.

```
            ┌────────────────────────── loop() @ ~60 Hz ──────────────────────────┐
            │  pollTouch() ─► tap / drag / swipe / long-press  ─►  onTapXxx()      │
            │  per-screen animation ticks (lock pulse, PIN shake, list inertia)    │
            │  BLE manager (advertise state, connect gate)                         │
            │  Wi-Fi captive portal service                                        │
            │  auto-lock / LED auto-clear / physical button                        │
            └──────────────────────────────────────────────────────────────────────┘
```

---

## 2. Screen state machine & navigation

Every screen is a pair of functions:

```c
void drawXxx();                       // render the whole screen into the canvas
void onTapXxx(int16_t x, int16_t y);  // handle a tap at (x, y)
```

`current` holds the active `Screen` enum. A small **navigation stack** (`navStack[10]`, `navTop`) gives Android‑style back behaviour:

- `pushNav(SCR_X)` — go deeper (also runs per‑screen entry hooks, e.g. `buildList()` for the list, `pinSlideIn()` for the PIN animation).
- `popNav()` — go back one level.
- `popToLock()` — collapse to the lock screen (idle timeout, physical button).

`drawAll()` and `dispatchTap()` are the two switch statements that route to the active screen. **Adding a screen = write the pair, add a `SCR_` enum, register in both switches.**

Rendering is **double‑buffered**: everything is drawn into an `Arduino_Canvas` in PSRAM, then `flushScreen()` (`gfx->flush()`) pushes the whole frame to the AMOLED over QSPI in one shot. No partial draws, no flicker, no tearing.

---

## 3. Touch: from capacitive blob to gesture

`touch_input.ino` reads the FT3168 over I²C (`ftReadTouch()` — register `0x02` for the touch count, `0x03` for the first point) and runs a small state machine in `pollTouch()`:

| Transition | Meaning | Fires |
|---|---|---|
| no‑touch → touch | finger down | records start point + time |
| touch → touch (moved > `DRAG_THRESHOLD`) | it's a drag | `onDrag()` live callback |
| touch held > 500 ms, no move | **long‑press** | `homeLongPress()` (home only) |
| touch → release, was a drag | swipe/scroll end | `onSwipeEnd()` |
| touch → release, was short & still | **tap** | `dispatchTap()` |

This single machine powers list scrolling (with inertia/friction), swipe‑back, the slide‑up PIN entrance, and the home drag‑to‑reorder — all without blocking the loop.

---

## 4. Data model & storage

### Record layout

The vault is an array of **fixed‑size 256‑byte records** in one FFat file, `db.bin`:

```c
struct __attribute__((packed)) PassRecord {  // 256 bytes
  uint16_t id;            // stable identifier
  uint8_t  deleted;       // tombstone flag
  char     folder[24];    // reused as the URL/subtitle
  char     title[40];
  char     username[64];
  char     password[48];
  char     url[60];
  char     note[16];
  uint8_t  favorite;      // heart flag
};
```

Fixed‑size records mean **O(1) addressing** and trivial append — no filesystem fragmentation, no JSON parsing on a microcontroller.

### Why a separate PSRAM index?

Re‑reading flash to render a scrolling list would be slow and wear the flash. Instead, at boot `dbLoadIndex()` builds a compact **64‑byte `ListItem`** per entry (id, favorite, title, subtitle) in **PSRAM**:

```c
struct __attribute__((packed)) ListItem {  // 64 bytes
  uint16_t id;
  uint8_t  favorite;
  char     folder[21];
  char     title[40];
};
```

Searching, sorting (`qsort` by title), and the virtual‑scroll list all operate on this in‑RAM index — flash is only touched to open a single full record (`dbLoadRecord()`) or to write.

### Operations

| Op | Strategy |
|---|---|
| Add | append one record (`dbAppend`) |
| Edit | rewrite file, replace matching `id` in place |
| Delete | rewrite file with `deleted = 1` (tombstone) |
| Toggle favorite | rewrite that record, reload index |

### Capacity

`MAX_PASSWORDS = 30000`. That's ≈ **1.9 MB** of index in the 8 MB OPI PSRAM, plus a 2‑byte‑per‑entry sort array, and ≈ **7.3 MB** of records in the 9 MB FAT partition. `uint16_t` ids/counters comfortably cover 30 000 entries.

---

## 5. HID: the device pretends to be a keyboard

Both transports advertise a standard **HID keyboard**, so the host needs no driver or software. The firmware sends USB HID **usage codes** (scancodes); the **host** maps them to characters using *its* keyboard layout.

### Why two translation units?

`USBHIDKeyboard.h` (TinyUSB) and `BleKeyboard.h` (NimBLE) both `#define KEY_TAB`, `KEY_RETURN`, etc. and both declare a `KeyReport` — they **cannot** be `#include`d in the same `.cpp`. So:

- `hid_usb.cpp` owns the USB keyboard.
- `hid_ble.cpp` owns the BLE keyboard.
- Each exposes a tiny `extern "C"` surface (`hidUsbPrint`, `hidBlePrint`, …). The `.ino` code never sees the library headers.

### Dual‑transport dispatch

`typeViaHID()` doesn't pick one transport — it sends to **every** ready one:

```
if (BLE enabled && connected && user-accepted)  hidBlePrint(s);
if (USB enabled && host has enumerated us)       hidUsbPrint(s);
```

NimBLE runs its radio work on its own task and coexists with native USB on the dual‑core S3, so "BLE on" never disables USB and vice‑versa.

### USB mount detection

USB typing only fires once `tud_mounted()` (TinyUSB) reports the host has enumerated the keyboard — that's why plugging into a *charger* (no data) won't type, and why the firmware waits briefly after `USB.begin()` for Windows to enumerate.

---

## 6. Bluetooth: pairing gate & the stuck‑key fix

### The pairing gate

A BLE *central* (phone/PC) can connect to our peripheral at any time, but SecureKey **types nothing until the user accepts on‑device** — and only once unlocked. The `loop()` BLE manager detects a new connection and, if unlocked + screen on + not in a block window, calls `bleConnectGate()`:

- **Accept** → `bleAuthorized = true`; typing allowed.
- **Reject** → keep the link up (a paired phone auto‑reconnects instantly, which would loop the prompt) but stay unauthorized, and **snooze** the prompt 20 s.
- **Block 5 min** → drop the radio entirely for 5 minutes.

The prompt shows the peer's Bluetooth address (`hidBlePeerAddr()` → `NimBLEServer::getPeerIDInfo().getAddress()`). A central doesn't transmit a friendly name to a keyboard, so the MAC is what's identifiable.

### Live on/off without reboot

The patched `BleKeyboard::end()` is a no‑op and calling `begin()` twice duplicates GATT services, so `hid_ble.cpp` calls `kb.begin()` **exactly once per boot** (`everStarted`). Turning BLE on/off afterward uses raw NimBLE: `getAdvertising()->start()/stop()` plus `server->disconnect(peer)`.

### Just‑Works pairing

The library default `setSecurityAuth(true, true, true)` demands MITM protection, but this device has no passkey entry/display — so Windows and some Androids paired but never encrypted (connected, never typed). Patched to `(true, false, false)` = **bonded "Just Works"** pairing, the correct profile for a display‑less keyboard.

### The "…gmmmmm" stuck‑key bug

A HID keystroke is a **key‑down report** followed by a **key‑up report**. `kb.write()` sends both back‑to‑back, so over BLE they can land in the *same* connection event and the host misses the release → it auto‑repeats the last key forever. The fix in `bleType()`:

```
press(key)  →  hold ~32 ms (spans a connection interval)  →  releaseAll()  →  small gap
```

Key‑down and key‑up now land in **different** connection events. We also request a tighter connection interval (`updateConnParams`) so typing is prompt, and `releaseAll()` at the start/end guarantees nothing is left held.

### The Android `@`→`"` fix

HID sends key *positions*. On a UK/Android layout, `Shift+2` is `"` and `@` lives on `Shift+'`. The **Android @ Fix** toggle makes `bleType()` send `@` as `Shift+apostrophe` (usage `0x34`) and `"` as `Shift+2` (usage `0x1F`), which produce the right glyphs on those hosts — the classic US↔UK swap, fixed in firmware. (Only enable it for Android; on a US host it would invert them.)

---

## 7. Security: PIN, lockout, auto‑lock

- **PIN** is checked on the keypad screen; correct entry jumps straight to Home.
- **Brute‑force backoff** (`pinRegisterFail`): the 3rd wrong attempt locks the keypad 30 s, then 60 s, 2 m, 5 m. The cumulative fail count is **persisted in NVS**, so a power‑cycle doesn't reset the penalty (millis() resets, but the next wrong attempt jumps straight back to a long wait).
- **Auto‑lock** after configurable idle, **double‑tap‑to‑sleep**, and the physical button → instant lock.
- **Factory reset** is **PIN‑gated**, then a second confirm, then wipes `db.bin` + NVS and reboots.

> ⚠️ **Not yet encrypted at rest.** The vault on flash is plaintext today. The roadmap is AES‑256 with a PIN‑derived key plus ESP32 Flash Encryption + Secure Boot v2. Treat SecureKey as a strong learning project, not a certified product.

---

## 8. Wi‑Fi import/export portal

Typing many entries on a touchscreen is painful, so Settings can start a **SoftAP + HTTP server** (`wifi_portal.ino`) serving a single‑page web app (`portal_html.h`):

- Join the device's Wi‑Fi, open the page, enter a one‑time **access code**.
- **Import**: paste lines or a CSV — Chrome/Google password exports are auto‑detected (`title,url,username,password` order).
- **Export**: download the vault as CSV.
- Every entry requires **title + username + password** — validated in the browser *and* server‑side in `portalSaveEntry()` (so even bulk lines can't create half‑empty records).

The portal is torn down automatically when you navigate away from the Wi‑Fi screen.

---

## 9. Home drag‑to‑reorder

`homeOrder[slot] = item` maps grid positions to menu items. A long‑press lifts a tile (`homeLongPress`), it follows the finger (`homeDrag`), and on release it drops into the nearest slot, shuffling the rest (`homeRelease`). The order is saved to NVS (`horder`) and **validated as a permutation** on load, so a corrupt value can never brick the home screen. Tap **DONE** to leave arrange mode (also forced on lock).

---

## 10. Rendering style on AMOLED

`gfx_lib.ino` is the drawing toolkit: text helpers, icons, status/nav bars, and a small **color engine** (`lerp565` blend, `glowCircle` banded radial gradient, letter‑avatar palette). Because AMOLED black is genuinely *off*, the theme uses black backgrounds with **glow halos** and a blue/red accent system, color **letter‑avatars** for entries, and iOS‑style pill toggles — all hand‑drawn, no LVGL, into the double‑buffered canvas.

---

## File map

| File | Responsibility |
|------|----------------|
| `06_PasswordManager.ino` | setup/loop, nav stack, HID dispatch, BLE gate, PIN lockout, settings persistence |
| `theme.h` | palette, layout constants, `PassRecord`/`ListItem`, capacity, `UserSettings` |
| `pin_config.h` | board pin map |
| `hid_usb.cpp` / `hid_ble.cpp` | the two isolated HID keyboards |
| `storage.ino` | FFat binary DB + seed data |
| `touch_input.ino` | FT3168 reader + gesture state machine |
| `gfx_lib.ino` | drawing helpers + color engine |
| `keyboard.ino` | on‑screen keyboard |
| `screen_*.ino` | one file per screen (`draw` + `onTap`) |
| `wifi_portal.ino` + `portal_html.h` | import/export web app |
