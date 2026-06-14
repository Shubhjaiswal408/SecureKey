# SecureKey — New-Machine Setup

Everything you need to compile + flash SecureKey on a **fresh laptop**. The two
libraries that aren't in the Arduino Library Manager (a display-driver fork and a
patched BLE keyboard) are **bundled in this repo** under [`libraries/`](libraries/),
so you won't hit the "GFX / SH8601 not found" wall again.

---

## 1. Install the libraries (the part that used to break)

### Easy way — one click (Windows)
Right-click [`setup.ps1`](setup.ps1) → **Run with PowerShell**.
It copies the two bundled libraries into your `Documents\Arduino\libraries\`
folder (backing up anything already there).

### Manual way (any OS)
Copy both folders from this repo's `libraries/` into your Arduino libraries
folder (`Documents/Arduino/libraries/` on Windows, `~/Documents/Arduino/libraries/`
on Mac):

- `libraries/Arduino_GFX`         → the Makerfabs **SH8601** AMOLED fork (adds
  `Arduino_SH8601` + `Display_Brightness()` — the stock Library-Manager GFX lacks both)
- `libraries/ESP32_BLE_Keyboard`  → T-vK's BLE keyboard, **already patched**
  (`#define USE_NIMBLE` + `setSecurityAuth(true,false,true)` for Mac/iOS pairing)

> ⚠️ If a stock **GFX Library for Arduino** is already installed, move it OUT of
> the libraries folder — two libraries with the same header name clash and cause
> a black screen / compile error.

---

## 2. Install from Boards / Library Manager

| What | Where | Exact version |
|---|---|---|
| **esp32** board package | Boards Manager | **2.0.16** (NOT 3.x — breaks the GFX fork) |
| **NimBLE-Arduino** | Library Manager | **1.4.3** (NOT 2.x) |
| **Adafruit NeoPixel** | Library Manager | latest |

Boards Manager URL (Preferences → Additional Boards Manager URLs):
```
https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
```

---

## 3. Board settings (Tools menu) — all must match

| Setting | Value |
|---|---|
| Board | **ESP32S3 Dev Module** |
| **PSRAM** | **OPI PSRAM** ← most important; wrong = black screen |
| Flash Size | **16MB (128Mb)** |
| Partition Scheme | **16M Flash (3MB APP / 9.9MB FATFS)** |
| USB Mode | **USB-OTG (TinyUSB)** |
| USB CDC On Boot | **Enabled** |
| Upload Speed | 921600 |

> If the canvas can't get PSRAM the firmware now shows a red **"PSRAM CANVAS FAIL —
> set PSRAM = OPI PSRAM"** screen instead of staying black, so the fix is obvious.

---

## 4. Flash

Open [`06_PasswordManager/06_PasswordManager.ino`](06_PasswordManager/), pick the
COM port, **Upload**. First boot lands on the lock screen (default PIN **1234**).

---

## Troubleshooting

| Symptom | Cause → Fix |
|---|---|
| `Arduino_SH8601 ... not found` | Stock GFX in use → install the bundled `libraries/Arduino_GFX`, remove the stock one |
| Black screen, HelloAMOLED works | PSRAM not enabled → Tools → **PSRAM: OPI PSRAM** |
| Red "PSRAM CANVAS FAIL" screen | Same as above — set PSRAM = OPI |
| BLE types `'` instead of `@` on Android | Settings → **Android** toggle ON |
| Mac won't pair | bundled BLE keyboard already patched; if you replaced it, re-apply `setSecurityAuth(true,false,true)` |

Full build toolchain is also reproduced in CI:
[`.github/workflows/build.yml`](.github/workflows/build.yml).
