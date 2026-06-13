// =============================================================
//  hid_ble.cpp  —  Isolated BLE-HID wrapper
//
//  Pairs hid_usb.cpp.  Owns the BleKeyboard library (T-vK's
//  "ESP32 BLE Keyboard", configured here for the NimBLE backend so
//  it coexists with native USB HID on the ESP32-S3).  Lives in its
//  own translation unit so its KEY_xxx macros can't collide with
//  USBHIDKeyboard's.
//
//  Disable here if the library isn't installed:
//      set HID_BLE_ENABLE 0
// =============================================================
#define HID_BLE_ENABLE 1

#include <Arduino.h>

#if HID_BLE_ENABLE
  #include <BleKeyboard.h>
  #include <NimBLEDevice.h>
  static BleKeyboard kb("SecureKey", "techiesms", 100);
  static bool everStarted = false;   // kb.begin() may only EVER run once/boot
  static bool active      = false;   // advertising / accepting connections?
  static bool androidFix  = false;   // UK/Android layout: swap @ <-> " keycodes

  // BLE HID timing. The KEY fix for "stuck / repeating keys": a key-down and
  // its key-up MUST land in DIFFERENT BLE connection events, otherwise the
  // host can miss the release and auto-repeat the last key forever (the
  // "...gmmmmm" bug). kb.write() sends press+release back-to-back (same
  // event), so we DON'T use it — we press, hold past one connection interval,
  // then send a clean "all keys up" report, then a small gap before the next.
  static const uint16_t BLE_HOLD_MS = 32;   // key held (> typical 15-30ms interval)
  static const uint16_t BLE_GAP_MS  = 12;   // quiet gap before the next key

  // Type one character. In "Android fix" mode, '@' and '"' are sent as the
  // raw keycodes that produce them on a UK/Android host (where Shift+2 = "
  // and Shift+' = @), instead of the US asciimap (Shift+2 = @). This is the
  // classic US<->UK swap that makes @ come out as " on many Android phones.
  static void bleType(char c) {
    if (androidFix && c == '@') {
      kb.press(KEY_LEFT_SHIFT); kb.press('\'');   // @ on UK/Android
    } else if (androidFix && c == '"') {
      kb.press(KEY_LEFT_SHIFT); kb.press('2');     // " on UK/Android
    } else {
      kb.press((uint8_t)c);
    }
    delay(BLE_HOLD_MS);
    kb.releaseAll();                                // clean key-up report
    delay(BLE_GAP_MS);
  }

  // Same press/hold/release discipline for a raw key (Tab, Enter, ...).
  static void bleRawKey(uint8_t k) {
    kb.press(k);
    delay(BLE_HOLD_MS);
    kb.releaseAll();
    delay(BLE_GAP_MS);
  }

  // Ask the peer for a tighter connection interval so each key report is
  // delivered promptly (and the hold above reliably spans an interval).
  static void bleFastConn() {
    NimBLEServer *srv = NimBLEDevice::getServer();
    if (!srv) return;
    std::vector<uint16_t> peers = srv->getPeerDevices();
    if (peers.empty()) return;
    // min 15ms, max 30ms, latency 0, supervision timeout 4s
    srv->updateConnParams(peers[0], 12, 24, 0, 400);
  }
#endif

extern "C" {

// Toggle the UK/Android @ <-> " keycode swap (called when the Settings
// "Android @ Fix" toggle changes, and once at boot from loadSettings()).
void hidBleSetAndroidFix(int on) {
#if HID_BLE_ENABLE
  androidFix = on ? true : false;
#else
  (void)on;
#endif
}

// IMPORTANT: this library's BleKeyboard::end() is EMPTY, and calling
// kb.begin() twice duplicates the HID GATT services (breaks pairing).
// So: kb.begin() exactly once per boot; after that, on/off is done with
// the real NimBLE radio calls — stop/start advertising + drop peers.
void hidBleBegin() {
#if HID_BLE_ENABLE
  if (!everStarted) {
    Serial.println("[BLE] kb.begin() (first start)");
    kb.setDelay(5);     // 5 ms between key reports (library internal)
    kb.begin();
    everStarted = true;
    active = true;
    Serial.println("[BLE] advertising as 'SecureKey'");
  } else if (!active) {
    Serial.println("[BLE] resume advertising");
    NimBLEDevice::getAdvertising()->start();
    active = true;
  }
#endif
}

void hidBleEnd() {
#if HID_BLE_ENABLE
  if (!everStarted || !active) { active = false; return; }
  Serial.println("[BLE] stop: drop peers + stop advertising");
  NimBLEServer *srv = NimBLEDevice::getServer();
  if (srv) {
    std::vector<uint16_t> peers = srv->getPeerDevices();
    for (uint16_t id : peers) srv->disconnect(id);
    if (!peers.empty()) delay(400);  // let onDisconnect fire (it restarts adv)
  }
  NimBLEDevice::getAdvertising()->stop();
  active = false;
#endif
}

int hidBleConnected() {
#if HID_BLE_ENABLE
  return everStarted && active && kb.isConnected() ? 1 : 0;
#else
  return 0;
#endif
}

void hidBlePrint(const char *s) {
#if HID_BLE_ENABLE
  Serial.printf("[BLE] hidBlePrint('%s')  active=%d  connected=%d\n",
                s, active, everStarted ? (int)kb.isConnected() : 0);
  if (!active) { Serial.println("[BLE]   not active — begin()"); hidBleBegin(); delay(500); }
  if (!kb.isConnected()) {
    Serial.println("[BLE]   isConnected()=false — aborting print");
    return;
  }
  bleFastConn();   // request a snappy connection interval
  delay(80);       // let the host fully ready its HID stack
  int n = strlen(s);
  Serial.printf("[BLE]   sending %d chars... (androidFix=%d)\n", n, androidFix);
  kb.releaseAll();                 // make sure nothing is held before we start
  for (int i = 0; i < n; i++) bleType(s[i]);
  kb.releaseAll();                 // final safety key-up
  Serial.println("[BLE]   done");
#else
  (void)s;
#endif
}

// One-tap login over BLE: username, Tab, password, Enter.
void hidBleQuickFill(const char *user, const char *pass) {
#if HID_BLE_ENABLE
  if (!active || !kb.isConnected()) {
    Serial.println("[BLE] quickFill aborted — not connected");
    return;
  }
  bleFastConn();
  delay(80);
  kb.releaseAll();
  for (const char *p = user; *p; p++) bleType(*p);
  bleRawKey(KEY_TAB);
  for (const char *p = pass; *p; p++) bleType(*p);
  bleRawKey(KEY_RETURN);
  kb.releaseAll();
  Serial.println("[BLE]   quickFill done");
#else
  (void)user; (void)pass;
#endif
}

int hidBleCompiled() {
#if HID_BLE_ENABLE
  return 1;
#else
  return 0;
#endif
}

// Identify the connected peer for the on-device Accept/Reject prompt.
// NOTE: a BLE central (phone/PC) does NOT hand its friendly name ("iPhone",
// "Microsoft") to the keyboard it connects to — only its Bluetooth address
// is available — so we surface the MAC address. Writes a short string like
// "AA:BB:CC:DD:EE:FF" into out, or "unknown" if no peer / not compiled.
void hidBlePeerAddr(char *out, int n) {
  if (!out || n <= 0) return;
  out[0] = 0;
#if HID_BLE_ENABLE
  NimBLEServer *srv = NimBLEDevice::getServer();
  if (srv) {
    std::vector<uint16_t> peers = srv->getPeerDevices();
    if (!peers.empty()) {
      std::string a = srv->getPeerIDInfo(peers[0]).getAddress().toString();
      strncpy(out, a.c_str(), n - 1);
      out[n - 1] = 0;
    }
  }
#endif
  if (!out[0]) strncpy(out, "unknown", n - 1), out[n - 1] = 0;
}

// Is BLE currently active (advertising or connected)?  Lets the main loop
// manage the radio live (toggle on/off, 5-min block window) without rebooting.
int hidBleStarted() {
#if HID_BLE_ENABLE
  return active ? 1 : 0;
#else
  return 0;
#endif
}

}  // extern "C"
