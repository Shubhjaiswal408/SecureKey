// =============================================================
//  screen_wifi.ino  —  WiFi captive-portal import screen
//
//  Shows the AP name, the fresh WPA2 password, and the 6-digit
//  code the user types on the page. The portal itself (SoftAP +
//  DNS + web server) lives in wifi_portal.ino and is serviced
//  from loop(). Leaving this screen (back / Turn-Off / nav away)
//  stops the portal.
// =============================================================

void drawWifi() {
  gfx->fillScreen(C_BLACK);
  drawStatusBar();
  drawNavBar("WiFi Import", true, nullptr);

  if (!wifiPortalActive()) {
    textCenter(STATUS_H + NAV_H + 60, "Starting WiFi...", 2, C_GRAY_4);
    flushScreen();
    return;
  }

  int16_t y = STATUS_H + NAV_H + 8;

  textCenter(y, "1. Join this WiFi", 1, C_GRAY_4);   y += 22;
  textCenter(y, wifiPortalSsid(), 3, C_WHITE);       y += 42;

  textCenter(y, "2. WiFi password", 1, C_GRAY_4);    y += 22;
  textCenter(y, wifiPortalPass(), 3, C_WHITE);       y += 44;

  textCenter(y, "3. Open page, enter code", 1, C_GRAY_4);  y += 24;
  textCenter(y, wifiPortalCode(), 4, C_BLUE);        y += 48;

  char buf[40];
  snprintf(buf, sizeof(buf), "Imported: %d", wifiPortalCount());
  textCenter(y, buf, 2, C_WHITE);

  // Turn-off button
  const int16_t bw = 220, bh = 56, bx = (LCD_WIDTH - bw) / 2, by = LCD_HEIGHT - 70;
  gfx->fillRoundRect(bx, by, bw, bh, 14, C_GRAY_1);
  gfx->drawRoundRect(bx, by, bw, bh, 14, C_WHITE);
  textCenter(by + 18, "Turn Off WiFi", 2, C_WHITE);

  flushScreen();
}

void onTapWifi(int16_t tx, int16_t ty) {
  // Back arrow OR the "Turn Off WiFi" button → stop portal and leave.
  bool back = (ty >= STATUS_H + 2 && ty < STATUS_H + NAV_H - 2
               && tx >= SAFE_PAD && tx < SAFE_PAD + 46);
  const int16_t bh = 56, by = LCD_HEIGHT - 70;
  bool stop = (ty >= by && ty < by + bh);
  if (back || stop) {
    wifiPortalStop();
    popNav();
  }
}
