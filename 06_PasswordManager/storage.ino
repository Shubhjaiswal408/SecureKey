// =============================================================
//  storage.ino  —  FFat binary password database
//
//  File:  /db.bin
//  Format: fixed-size PassRecord records (256 bytes each).
//  Index: loaded into PSRAM at boot (ListItem array, 64 bytes/entry).
//  Max entries: MAX_PASSWORDS (30000).  30000×256 ≈ 7.3 MB on FAT.
// =============================================================

#define DB_PATH  "/db.bin"

// ── Helpers ──────────────────────────────────────────────────
static uint16_t dbCount() {
  File f = FFat.open(DB_PATH, "r");
  if (!f) return 0;
  uint16_t n = (uint16_t)(f.size() / RECORD_SIZE);
  f.close();
  return n;
}

// ── Load index into PSRAM ────────────────────────────────────
void dbLoadIndex() {
  passwordCount = 0;
  File f = FFat.open(DB_PATH, "r");
  if (!f) return;

  PassRecord rec;
  while (f.available() >= RECORD_SIZE && passwordCount < MAX_PASSWORDS) {
    f.read((uint8_t *)&rec, RECORD_SIZE);
    if (rec.deleted) continue;
    ListItem &li = passwordIndex[passwordCount++];
    li.id = rec.id;
    li.favorite = rec.favorite;
    strncpy(li.folder, rec.folder, sizeof(li.folder) - 1);
    li.folder[sizeof(li.folder) - 1] = 0;
    strncpy(li.title, rec.title, sizeof(li.title) - 1);
    li.title[sizeof(li.title) - 1] = 0;
  }
  f.close();
}

// ── Load a single full record by id ─────────────────────────
bool dbLoadRecord(uint16_t id, PassRecord &out_rec) {
  File f = FFat.open(DB_PATH, "r");
  if (!f) { Serial.println("[DB] dbLoadRecord: open failed"); return false; }

  PassRecord rec;
  while (f.available() >= RECORD_SIZE) {
    f.read((uint8_t *)&rec, RECORD_SIZE);
    if (rec.id == id && !rec.deleted) {
      out_rec = rec;
      f.close();
      Serial.printf("[DB] Load id=%u OK  title='%s'  pass_len=%u\n",
                    id, out_rec.title, (unsigned)strlen(out_rec.password));
      return true;
    }
  }
  f.close();
  Serial.printf("[DB] Load id=%u NOT FOUND\n", id);
  return false;
}

// ── Append a new record ──────────────────────────────────────
// Robust against the FFat quirk where "a" mode silently fails
// if the file doesn't exist yet — falls back to "w" in that case,
// flushes explicitly before close, and verifies bytes written.
bool dbAppend(const PassRecord &rec) {
  File f;
  if (FFat.exists(DB_PATH)) {
    f = FFat.open(DB_PATH, "a");
  } else {
    f = FFat.open(DB_PATH, "w");
  }
  if (!f) {
    Serial.println("[DB] ERROR: open for append failed");
    return false;
  }
  size_t before = f.size();
  size_t n = f.write((const uint8_t *)&rec, RECORD_SIZE);
  f.flush();
  f.close();
  if (n != RECORD_SIZE) {
    Serial.printf("[DB] ERROR: wrote only %u of %u bytes\n",
                  (unsigned)n, (unsigned)RECORD_SIZE);
    return false;
  }
  Serial.printf("[DB] Appended id=%u '%s'  (file was %u, now ~%u)\n",
                rec.id, rec.title,
                (unsigned)before, (unsigned)(before + RECORD_SIZE));
  return true;
}

// ── Mark record as deleted (soft delete) ────────────────────
bool dbDelete(uint16_t id) {
  // We rewrite the file with matching record deleted flag set
  File src = FFat.open(DB_PATH, "r");
  if (!src) return false;
  File dst = FFat.open("/db_tmp.bin", "w");
  if (!dst) { src.close(); return false; }

  PassRecord rec;
  while (src.available() >= RECORD_SIZE) {
    src.read((uint8_t *)&rec, RECORD_SIZE);
    if (rec.id == id) rec.deleted = 1;
    dst.write((uint8_t *)&rec, RECORD_SIZE);
  }
  src.close(); dst.close();
  FFat.remove(DB_PATH);
  FFat.rename("/db_tmp.bin", DB_PATH);
  return true;
}

// ── Update an existing record (rewrite file, replace matching id) ───
bool dbUpdate(uint16_t id, const PassRecord &newRec) {
  File src = FFat.open(DB_PATH, "r");
  if (!src) return false;
  File dst = FFat.open("/db_tmp.bin", "w");
  if (!dst) { src.close(); return false; }

  PassRecord rec; bool found = false;
  while (src.available() >= RECORD_SIZE) {
    src.read((uint8_t *)&rec, RECORD_SIZE);
    if (rec.id == id && !rec.deleted) {
      PassRecord up = newRec; up.id = id; up.deleted = 0;
      dst.write((uint8_t *)&up, RECORD_SIZE);
      found = true;
    } else {
      dst.write((uint8_t *)&rec, RECORD_SIZE);
    }
  }
  src.close(); dst.close();
  FFat.remove(DB_PATH);
  FFat.rename("/db_tmp.bin", DB_PATH);
  return found;
}

// ── Toggle the "favorite" (heart) flag on a record ──────────────────
bool dbToggleFavorite(uint16_t id) {
  PassRecord rec;
  if (!dbLoadRecord(id, rec)) return false;
  rec.favorite = rec.favorite ? 0 : 1;
  bool ok = dbUpdate(id, rec);
  if (ok) dbLoadIndex();          // refresh in-memory index (favorite flags)
  return ok;
}

// ── Next available id ────────────────────────────────────────
static uint16_t dbNextId() {
  uint16_t maxId = 0;
  for (uint16_t i = 0; i < passwordCount; i++)
    if (passwordIndex[i].id > maxId) maxId = passwordIndex[i].id;
  return maxId + 1;
}

// ── Seed demo passwords on first boot ────────────────────────
// "folder" field is reused as the subtitle (URL) in this monochrome
// build, so we put the short URL there too.
struct SeedEntry { const char *title, *user, *pass, *url, *note; };
static const SeedEntry SEEDS[] = {
  {"Amazon",        "shubh@gmail.com",   "Amzn$hop99",    "www.amazon.com",        ""},
  {"Apple ID",      "shubh@icloud.com",  "iCl0ud#24",     "appleid.apple.com",     ""},
  {"Dropbox",       "shubh@gmail.com",   "Dr0pbox@24",    "www.dropbox.com",       ""},
  {"Epic Games",    "shubhgamer408",     "Epic@2024!",    "epicgames.com",         ""},
  {"Facebook",      "shubh@gmail.com",   "FBpass#99",     "facebook.com",          ""},
  {"Flipkart",      "shubh@gmail.com",   "Flip#kart1",    "www.flipkart.com",      ""},
  {"GitHub",        "Shubhjaiswal408",   "GH_token_24",   "github.com",            "Work"},
  {"Gmail Personal","shubh@gmail.com",   "Gm@il#p24",     "gmail.com",             ""},
  {"Gmail Work",    "shubh@company.com", "Gw0rk$2024",    "gmail.com",             "Work"},
  {"Google Drive",  "shubh@gmail.com",   "GDr!ve24",      "drive.google.com",      ""},
  {"Google Pay",    "+91 9876543210",    "gp_pin_7789",   "pay.google.com",        ""},
  {"HDFC Bank",     "shubhjaiswal408",   "hdfcP@ss24",    "www.hdfcbank.com",      ""},
  {"Instagram",     "shubh@gmail.com",   "Insta@2024!",   "www.instagram.com",     ""},
  {"Jira",          "shubhjaiswal",      "JiraAdm#1",     "jira.company.com",      ""},
  {"LinkedIn",      "shubh@gmail.com",   "LI_Pass@99",    "linkedin.com",          ""},
  {"Myntra",        "shubh@gmail.com",   "Myn!ra2024",    "www.myntra.com",        ""},
  {"Notion",        "shubh@company.com", "N0tion$2024",   "notion.so",             ""},
  {"Outlook",       "shubh@outlook.com", "Outl00k$24",    "outlook.com",           ""},
  {"PayTM",         "shubh@gmail.com",   "Ptm@2024!",     "www.paytm.com",         ""},
  {"Slack",         "shubh@company.com", "Sl@ck2024!",    "slack.com",             ""},
  {"Steam",         "shubhgamer408",     "St3am!P24",     "store.steampowered.com",""},
  {"Twitter / X",   "shubhj@gmail.com",  "twitX!23",      "x.com",                 ""},
  {"WhatsApp",      "+91 9876543210",    "wa_pin_2024",   "web.whatsapp.com",      ""},
  {"Zerodha",       "shubhjaiswal",      "Zer0dha#24",    "kite.zerodha.com",      ""},
};

void dbSeed() {
  if (FFat.exists(DB_PATH)) return;
  Serial.println("[DB] Seeding demo passwords...");
  const int N = sizeof(SEEDS) / sizeof(SEEDS[0]);
  for (int i = 0; i < N; i++) {
    PassRecord rec;
    memset(&rec, 0, RECORD_SIZE);
    rec.id      = (uint16_t)(i + 1);
    rec.deleted = 0;
    // folder = URL (becomes the subtitle shown under the title)
    strncpy(rec.folder,   SEEDS[i].url,   sizeof(rec.folder)  - 1);
    strncpy(rec.title,    SEEDS[i].title, sizeof(rec.title)   - 1);
    strncpy(rec.username, SEEDS[i].user,  sizeof(rec.username)- 1);
    strncpy(rec.password, SEEDS[i].pass,  sizeof(rec.password)- 1);
    strncpy(rec.url,      SEEDS[i].url,   sizeof(rec.url)     - 1);
    strncpy(rec.note,     SEEDS[i].note,  sizeof(rec.note)    - 1);
    dbAppend(rec);
  }
  Serial.printf("[DB] Seeded %d passwords.\n", N);
}
