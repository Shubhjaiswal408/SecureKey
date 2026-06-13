// =============================================================
//  MaTouch ESP32-S3 AMOLED 1.8" FT3168  —  Pin Configuration
//  Source: Official Makerfabs schematic V1.0 (2024-08-05, Peter)
// =============================================================
#pragma once

#define LCD_WIDTH       368
#define LCD_HEIGHT      448
#define LCD_CS          4
#define LCD_SCK         5
#define LCD_SDIO0       7
#define LCD_SDIO1       10
#define LCD_SDIO2       11
#define LCD_SDIO3       12
#define LCD_RST         40
#define LCD_TE          6
#define LCD_EN          9

#define IIC_SDA         2
#define IIC_SCL         1
#define TP_INT          13
#define TP_RST          14
#define FT3168_ADDR     0x38

#define RGB_LED_PIN     3
#define RGB_LED_COUNT   1

#define USER_BTN_PIN    8
#define BOOT_BTN_PIN    0
