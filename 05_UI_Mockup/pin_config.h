// =============================================================
//  MaTouch ESP32-S3 AMOLED 1.8" FT3168  —  Pin Configuration
//  Source: Official Makerfabs schematic V1.0 (2024-08-05, Peter)
//  Verified pin-by-pin from netlist on 2026-05-22.
// =============================================================
//
//  Board: MaTouch ESP32-S3 R8 (8MB PSRAM, 16MB Flash)
//  Display: SH8601 368×448 AMOLED via QSPI
//  Touch:   FT3168 capacitive, I2C addr 0x38
//
// =============================================================

#pragma once

// ---------------- Display SH8601 (QSPI) ------------------------------------
#define LCD_WIDTH       368
#define LCD_HEIGHT      448

#define LCD_CS          4    // O_CS
#define LCD_SCK         5    // O_CLK
#define LCD_SDIO0       7    // O_I0  (D0)
#define LCD_SDIO1       10   // O_I1  (D1)
#define LCD_SDIO2       11   // O_I2  (D2)
#define LCD_SDIO3       12   // O_I3  (D3)
#define LCD_RST         40   // O_RST  (MTDO strap pin — fine after boot)
#define LCD_TE          6    // O_TE   (tear effect, can be -1 if unused)
#define LCD_EN          9    // O_PWR_EN  *** MUST drive HIGH to power AMOLED ***

// ---------------- Capacitive Touch FT3168 (I2C) ----------------------------
#define IIC_SDA         2
#define IIC_SCL         1
#define TP_INT          13   // O_TP_INT
#define TP_RST          14   // O_TP_RST
#define FT3168_ADDR     0x38

// ---------------- WS2812 Addressable RGB LED -------------------------------
#define RGB_LED_PIN     3    // LED_2812
#define RGB_LED_COUNT   1

// ---------------- Buttons --------------------------------------------------
#define USER_BTN_PIN    8    // SW2 "Key" (active LOW with external pull-up)
#define BOOT_BTN_PIN    0    // SW4 — also strapping pin
// Reset (SW3) is hardware-only on CHIP_PU, not GPIO-accessible

// ---------------- USB (fixed on ESP32-S3, do not change) -------------------
#define USB_DM          19
#define USB_DP          20

// ---------------- J3 expansion header (2x12, 1.27 mm pitch) ----------------
// J3 pin layout — odd pins on left, even on right (or vice-versa per silk).
//  1  : VCC3.3V            2  : VBAT
//  3  : VCC3.3V            4  : NC
//  5  : IIC_SDA  (GPIO 2)  6  : GPIO16
//  7  : IIC_SCL  (GPIO 1)  8  : GPIO21
//  9  : GPIO46            10  : GPIO17
// 11  : GPIO45            12  : GPIO15
// 13  : U0RXD   (GPIO44)  14  : GPIO48
// 15  : U0TXD   (GPIO43)  16  : GPIO47
// 17  : GPIO41            18  : GPIO39
// 19  : GPIO42            20  : GPIO38
// 21  : GND               22  : GND
// 23  : GND               24  : GND
//
// Free GPIOs available for add-ons (14 total):
//   1*, 2*, 15, 16, 17, 21, 38, 39, 41, 42, 43, 44, 45, 46, 47, 48
//   (*  shared with on-board touch I2C — fine for more I2C devices)
