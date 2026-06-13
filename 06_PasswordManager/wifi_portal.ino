// =============================================================
//  wifi_portal.ino  —  On-device WiFi captive-portal import
//
//  The whole thing lives ON the device — no app, no hosting, no
//  GitHub. The user just joins the device's WiFi and the page pops
//  up (captive portal, like hotel WiFi), fills a form / pastes a
//  CSV, and the device saves it straight into the vault.
//
//  Security (see also the on-screen Settings page):
//    • Isolated SoftAP, internet-disconnected.
//    • WPA2 with a fresh RANDOM password each session (shown only
//      on the device screen).
//    • A 6-digit on-screen CODE must be entered on the page before
//      anything is saved.
//    • Single client; auto-off after idle.
//
//  Built-in libs only (WiFi / WebServer / DNSServer) — no deps.
// =============================================================
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

static WebServer  portalSrv(80);
static DNSServer  portalDns;
static bool       portalActive   = false;
static char       portalSsid[20] = "SecureKey-Setup";
static char       portalPass[12] = {0};   // random WPA2 (8 chars)
static char       portalCode[7]  = {0};   // 6-digit on-screen gate
static uint32_t   portalLastSeen = 0;     // for idle auto-off
static int        portalImported = 0;     // running count (for the screen)
static bool       portalNeedReload = false;

// ── Accessors for the Settings screen ────────────────────────────────
bool        wifiPortalActive()   { return portalActive; }
const char* wifiPortalSsid()     { return portalSsid; }
const char* wifiPortalPass()     { return portalPass; }
const char* wifiPortalCode()     { return portalCode; }
int         wifiPortalCount()    { return portalImported; }
String      wifiPortalIp()       { return WiFi.softAPIP().toString(); }

// ── Save one parsed entry (Arduino-task context — FFat safe) ─────────
static bool portalSaveEntry(const char *title, const char *user,
                            const char *pass, const char *url, uint16_t id) {
  // Title, username AND password are all mandatory — enforced server-side so
  // neither the form nor a bulk-import line can create half-empty entries.
  if (!title || !title[0]) return false;
  if (!user  || !user[0])  return false;
  if (!pass  || !pass[0])  return false;
  PassRecord rec; memset(&rec, 0, sizeof(rec));
  rec.id = id; rec.deleted = 0;
  strncpy(rec.title,    title, sizeof(rec.title)    - 1);
  strncpy(rec.username, user ? user : "", sizeof(rec.username) - 1);
  strncpy(rec.password, pass ? pass : "", sizeof(rec.password) - 1);
  strncpy(rec.url,      url ? url : "",   sizeof(rec.url)      - 1);
  strncpy(rec.folder,   url ? url : "",   sizeof(rec.folder)   - 1);
  return dbAppend(rec);
}

static uint16_t portalNextId() {
  uint16_t maxId = 0;
  for (uint16_t i = 0; i < passwordCount; i++)
    if (passwordIndex[i].id > maxId) maxId = passwordIndex[i].id;
  return maxId + 1;
}

// ── The page (served from flash) ─────────────────────────────────────
#include "portal_html.h"

// ── HTTP handlers ────────────────────────────────────────────────────
static void portalSendHtml() {
  portalLastSeen = millis();
  portalSrv.send_P(200, "text/html", PORTAL_HTML);
}

static void portalHandleSave() {
  portalLastSeen = millis();
  if (portalSrv.arg("code") != String(portalCode)) {
    portalSrv.send(200, "text/html",
      "<meta name=viewport content='width=device-width'><body style='background:#0b0e13;color:#ff5f6d;"
      "font-family:sans-serif;padding:30px;text-align:center'><h2>Wrong code</h2>"
      "<p style='color:#8a93a4'>Enter the 6-digit code shown on the device.</p>"
      "<a href='/' style='color:#4d9fff'>Back</a></body>");
    return;
  }

  uint16_t id = portalNextId();
  int added = 0;
  String bulk = portalSrv.arg("bulk");
  if (bulk.length() > 0) {
    // line-by-line; detect a Chrome CSV header
    int start = 0;
    bool first = true, chromeCsv = false;
    while (start < (int)bulk.length()) {
      int nl = bulk.indexOf('\n', start);
      if (nl < 0) nl = bulk.length();
      String line = bulk.substring(start, nl); line.trim();
      start = nl + 1;
      if (line.length() == 0) { first = false; continue; }
      if (first) {
        String h = line; h.toLowerCase();
        chromeCsv = h.indexOf("password") >= 0 && h.indexOf("url") >= 0 && h.indexOf(",") >= 0;
        first = false;
        if (chromeCsv) continue;   // skip header
      }
      // split into <=4 fields on tab or comma
      char sep = line.indexOf('\t') >= 0 ? '\t' : ',';
      String f[4] = {"", "", "", ""};
      int fi = 0, p = 0;
      while (fi < 4 && p <= (int)line.length()) {
        int s = line.indexOf(sep, p);
        if (s < 0) s = line.length();
        f[fi++] = line.substring(p, s); p = s + 1;
        if (s == (int)line.length()) break;
      }
      const char *title, *user, *pass, *url;
      if (chromeCsv) { title = f[0].c_str(); url = f[1].c_str(); user = f[2].c_str(); pass = f[3].c_str(); }
      else           { title = f[0].c_str(); user = f[1].c_str(); pass = f[2].c_str(); url = f[3].c_str(); }
      if (portalSaveEntry(title, user, pass, url, id)) { id++; added++; }
    }
  } else {
    if (portalSaveEntry(portalSrv.arg("title").c_str(), portalSrv.arg("user").c_str(),
                        portalSrv.arg("pass").c_str(),  portalSrv.arg("url").c_str(), id)) added++;
  }

  portalImported += added;
  if (added > 0) portalNeedReload = true;   // reload index in the loop

  char body[260];
  snprintf(body, sizeof(body),
    "<meta name=viewport content='width=device-width'><body style='background:#0b0e13;color:#36d67a;"
    "font-family:sans-serif;padding:30px;text-align:center'><h2>Saved %d</h2>"
    "<p style='color:#8a93a4'>Total imported this session: %d</p>"
    "<a href='/' style='color:#4d9fff'>Add more</a></body>", added, portalImported);
  portalSrv.send(200, "text/html", body);
}

// Captive-portal: send every unknown request to our page so the phone's
// "Sign in to network" sheet pops up automatically.
static void portalHandleNotFound() {
  portalLastSeen = millis();
  portalSrv.sendHeader("Location", String("http://") + WiFi.softAPIP().toString(), true);
  portalSrv.send(302, "text/plain", "");
}

// ── Vault management routes (all gated on the 6-digit code) ───────────
static bool portalCodeOk() { return portalSrv.arg("code") == String(portalCode); }

static String jsonEsc(const char *s) {
  String o;
  for (const char *p = s; *p; p++) {
    char c = *p;
    if (c == '\n' || c == '\r') continue;
    if (c == '"' || c == '\\') o += '\\';
    o += c;
  }
  return o;
}
static String csvField(const char *s) {
  String v(s);
  if (v.indexOf(',') >= 0 || v.indexOf('"') >= 0) {
    v.replace("\"", "\"\"");
    return "\"" + v + "\"";
  }
  return v;
}

// GET /list?code=XXX  →  streamed JSON array of every entry.
static void portalHandleList() {
  portalLastSeen = millis();
  if (!portalCodeOk()) { portalSrv.send(403, "application/json", "[]"); return; }
  portalSrv.setContentLength(CONTENT_LENGTH_UNKNOWN);
  portalSrv.send(200, "application/json", "");
  portalSrv.sendContent("[");
  File f = FFat.open(DB_PATH, "r");
  bool first = true;
  if (f) {
    PassRecord rec;
    while (f.available() >= RECORD_SIZE) {
      f.read((uint8_t *)&rec, RECORD_SIZE);
      if (rec.deleted) continue;
      String item = (first ? "" : ",");
      first = false;
      item += "{\"id\":" + String(rec.id)
            + ",\"title\":\"" + jsonEsc(rec.title)    + "\""
            + ",\"user\":\""  + jsonEsc(rec.username) + "\""
            + ",\"pass\":\""  + jsonEsc(rec.password) + "\""
            + ",\"url\":\""   + jsonEsc(rec.url)      + "\"}";
      portalSrv.sendContent(item);
    }
    f.close();
  }
  portalSrv.sendContent("]");
  portalSrv.sendContent("");
}

// POST /edit  (code,id,title,user,pass,url) → update one record.
static void portalHandleEdit() {
  portalLastSeen = millis();
  if (!portalCodeOk()) { portalSrv.send(403, "text/plain", "bad code"); return; }
  uint16_t id = (uint16_t)portalSrv.arg("id").toInt();
  PassRecord rec; memset(&rec, 0, sizeof(rec));
  strncpy(rec.title,    portalSrv.arg("title").c_str(), sizeof(rec.title)    - 1);
  strncpy(rec.username, portalSrv.arg("user").c_str(),  sizeof(rec.username) - 1);
  strncpy(rec.password, portalSrv.arg("pass").c_str(),  sizeof(rec.password) - 1);
  strncpy(rec.url,      portalSrv.arg("url").c_str(),   sizeof(rec.url)      - 1);
  strncpy(rec.folder,   portalSrv.arg("url").c_str(),   sizeof(rec.folder)   - 1);
  bool ok = dbUpdate(id, rec);
  portalNeedReload = true;
  portalSrv.send(200, "text/plain", ok ? "ok" : "notfound");
}

// POST /delete  (code,id) → soft-delete one record.
static void portalHandleDelete() {
  portalLastSeen = millis();
  if (!portalCodeOk()) { portalSrv.send(403, "text/plain", "bad code"); return; }
  uint16_t id = (uint16_t)portalSrv.arg("id").toInt();
  bool ok = dbDelete(id);
  portalNeedReload = true;
  portalSrv.send(200, "text/plain", ok ? "ok" : "notfound");
}

// GET /export?code=XXX → streamed CSV download (title,username,password,url).
static void portalHandleExport() {
  portalLastSeen = millis();
  if (!portalCodeOk()) { portalSrv.send(403, "text/plain", "bad code"); return; }
  portalSrv.sendHeader("Content-Disposition", "attachment; filename=securekey-vault.csv");
  portalSrv.setContentLength(CONTENT_LENGTH_UNKNOWN);
  portalSrv.send(200, "text/csv", "");
  portalSrv.sendContent("title,username,password,url\n");
  File f = FFat.open(DB_PATH, "r");
  if (f) {
    PassRecord rec;
    while (f.available() >= RECORD_SIZE) {
      f.read((uint8_t *)&rec, RECORD_SIZE);
      if (rec.deleted) continue;
      String line = csvField(rec.title) + "," + csvField(rec.username) + ","
                  + csvField(rec.password) + "," + csvField(rec.url) + "\n";
      portalSrv.sendContent(line);
    }
    f.close();
  }
  portalSrv.sendContent("");
}

// ── Public control ───────────────────────────────────────────────────
void wifiPortalStart() {
  if (portalActive) return;

  // Fresh random WPA2 password + 6-digit code each session.
  static const char *AZ = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";  // no confusing chars
  for (int i = 0; i < 8; i++)  portalPass[i] = AZ[esp_random() % 31];
  portalPass[8] = 0;
  for (int i = 0; i < 6; i++)  portalCode[i] = '0' + (esp_random() % 10);
  portalCode[6] = 0;
  portalImported = 0; portalNeedReload = false;

  WiFi.mode(WIFI_AP);
  // channel 1, not hidden, MAX 1 client
  WiFi.softAP(portalSsid, portalPass, 1, 0, 1);
  delay(100);
  portalDns.start(53, "*", WiFi.softAPIP());     // all domains → us

  portalSrv.on("/",       HTTP_GET,  portalSendHtml);
  portalSrv.on("/save",   HTTP_POST, portalHandleSave);
  portalSrv.on("/list",   HTTP_GET,  portalHandleList);
  portalSrv.on("/edit",   HTTP_POST, portalHandleEdit);
  portalSrv.on("/delete", HTTP_POST, portalHandleDelete);
  portalSrv.on("/export", HTTP_GET,  portalHandleExport);
  portalSrv.onNotFound(portalHandleNotFound);
  portalSrv.begin();

  portalActive = true; portalLastSeen = millis();
  Serial.printf("[WIFI] portal up: SSID=%s PASS=%s CODE=%s IP=%s\n",
                portalSsid, portalPass, portalCode, WiFi.softAPIP().toString().c_str());
}

void wifiPortalStop() {
  if (!portalActive) return;
  portalSrv.stop();
  portalDns.stop();
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_OFF);
  portalActive = false;
  Serial.println("[WIFI] portal stopped");
}

// Call every loop(). Services the portal and auto-stops after idle.
void wifiPortalLoop() {
  if (!portalActive) return;
  portalDns.processNextRequest();
  portalSrv.handleClient();

  if (portalNeedReload) {           // a save happened — refresh the index
    portalNeedReload = false;
    dbLoadIndex();
    Serial.printf("[WIFI] index reloaded (%u total)\n", passwordCount);
  }

  // Auto-off after 3 min of no requests (safety).
  if (millis() - portalLastSeen > 180000UL) {
    Serial.println("[WIFI] idle timeout — stopping portal");
    wifiPortalStop();
  }
}
