# =====================================================================
#  SecureKey — one-click library setup  (Windows PowerShell)
# ---------------------------------------------------------------------
#  Copies the two bundled, hard-to-get libraries from this repo into
#  your Arduino libraries folder so the firmware compiles immediately
#  on a fresh machine:
#       libraries\Arduino_GFX         (Makerfabs SH8601 fork, trimmed)
#       libraries\ESP32_BLE_Keyboard  (patched: NimBLE + Just-Works)
#
#  Run it by right-clicking -> "Run with PowerShell", or:
#       powershell -ExecutionPolicy Bypass -File setup.ps1
#
#  After this, still install from Boards/Library Manager:
#    - esp32 board package  2.0.16   (Espressif)
#    - NimBLE-Arduino        1.4.3
#    - Adafruit NeoPixel     (latest)
#  See SETUP.md for the exact Tools-menu board settings.
# =====================================================================

$ErrorActionPreference = "Stop"
$repo = Split-Path -Parent $MyInvocation.MyCommand.Path
$dest = Join-Path ([Environment]::GetFolderPath("MyDocuments")) "Arduino\libraries"

Write-Host "SecureKey library setup" -ForegroundColor Cyan
Write-Host "  from : $repo\libraries"
Write-Host "  to   : $dest`n"

New-Item -ItemType Directory -Force -Path $dest | Out-Null

foreach ($lib in @("Arduino_GFX", "ESP32_BLE_Keyboard")) {
    $src = Join-Path $repo "libraries\$lib"
    $dst = Join-Path $dest $lib
    if (-not (Test-Path $src)) { Write-Host "  ! missing $src — skipped" -ForegroundColor Yellow; continue }

    # Back up any stock copy already there (e.g. GFX_Library_for_Arduino)
    if (Test-Path $dst) {
        $bak = "$dst._backup_{0}" -f (Get-Date -Format "yyyyMMdd_HHmmss")
        Move-Item $dst $bak
        Write-Host "  moved existing $lib -> $(Split-Path $bak -Leaf)" -ForegroundColor DarkYellow
    }
    Copy-Item $src $dst -Recurse
    Write-Host "  installed $lib" -ForegroundColor Green
}

# A stock "GFX Library for Arduino" from Library Manager will clash with our
# fork (same header name). Warn if one is present.
$stock = Join-Path $dest "GFX_Library_for_Arduino"
if (Test-Path $stock) {
    Write-Host "`n  ! Found a stock GFX_Library_for_Arduino — move it OUT of" -ForegroundColor Red
    Write-Host "    the libraries folder or it will clash with our SH8601 fork." -ForegroundColor Red
}

Write-Host "`nDone. Next:" -ForegroundColor Cyan
Write-Host "  1. Boards Manager  -> esp32 by Espressif = 2.0.16"
Write-Host "  2. Library Manager -> NimBLE-Arduino 1.4.3 + Adafruit NeoPixel"
Write-Host "  3. Open 06_PasswordManager, set Tools per SETUP.md, Upload."
