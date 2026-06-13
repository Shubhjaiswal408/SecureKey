# Contributing to SecureKey

Thanks for your interest! This is a hobby/learning hardware project — issues, fixes, hardware ports, and docs are all welcome.

## Ground rules

- **Be kind.** Assume good intent.
- **One change per PR.** Small, focused PRs get merged fast.
- **The build must stay green.** CI compiles the firmware on every push (see [`.github/workflows/build.yml`](.github/workflows/build.yml)). If CI is red, the PR isn't ready.

## Dev setup

You need the exact toolchain from the [README](README.md#-quick-start):

- Arduino-ESP32 core **2.0.16** (not 3.x)
- NimBLE-Arduino **1.4.3** (not 2.x)
- GFX Library for Arduino **1.3.7**
- ESP32 BLE Keyboard (T-vK) + the two one-line patches in the README
- Adafruit NeoPixel

Build locally before pushing:

```bash
arduino-cli compile \
  --fqbn "esp32:esp32:esp32s3:PartitionScheme=app3M_fat9M_16MB,PSRAM=opi,FlashSize=16M,USBMode=default,CDCOnBoot=cdc" \
  06_PasswordManager
```

## Code style

- Match the surrounding code — comment density, naming (`camelCase` funcs, `C_*` colors, `SCR_*` screens), and 2-space indent.
- Keep USB and BLE keyboard code in their **separate** `.cpp` files — the libraries share `KEY_*` macros and cannot co-exist in one translation unit.
- New screens follow the `drawXxx()` / `onTapXxx()` pair pattern and register in `drawAll()` / `dispatchTap()`.
- Touch zones must stay byte-accurate — verify hit-testing against the 368×448 grid.

## Good first issues

- 📷 Add real device photos/GIFs to `docs/img/` and wire them into the README.
- 🔐 AES-256 vault encryption (key derived from the PIN).
- 🌍 More host keyboard layouts beyond the US↔UK `@` swap.
- 🧪 A host-side script to generate large test vaults for the 30k path.

## Reporting bugs

Open an issue with: board revision, core/library versions, the exact USB Mode / CDC setting, and serial log output if you have it.
