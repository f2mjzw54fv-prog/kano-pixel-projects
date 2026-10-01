// Kano Pixel Kit - Online Slots firmware
//   Selection 1: WiFi stock ticker (Yahoo Finance, no API key)
//   Selection 2/3: projects fetched live from GitHub over WiFi
//                  (see https://github.com/f2mjzw54fv-prog/kano-pixel-projects)
// Joystick left/right = switch selection. Dial = brightness (all slots).
//
// Setup:
//   1. Set WIFI_SSID / WIFI_PASS below
//   2. Arduino IDE: ESP32 board package + FastLED + ArduinoJson libraries
//   3. Flash. Use the joystick to flip between the 3 selections.

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <FastLED.h>
#include <ArduinoJson.h>
#include <time.h>

// ---------------- CONFIG ----------------
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

// Raw URLs of the two online slot files
const char* SLOT_URLS[2] = {
  "https://raw.githubusercontent.com/f2mjzw54fv-prog/kano-pixel-projects/main/slots/slot2.json",
  "https://raw.githubusercontent.com/f2mjzw54fv-prog/kano-pixel-projects/main/slots/slot3.json",
};
const unsigned long SLOT_REFRESH_MS = 5 * 60 * 1000;  // re-fetch online slots every 5 min

// Ticker config (selection 1) — Yahoo Finance spark API (single batched request)
const char* YAHOO_SYMBOLS =
  "AAPL,TSLA,META,GOOG,UNH,O,SCHD,FDVV,GLD,IBIT,GBTC,RIVN,VRTX,CBRS,SPCX";
const unsigned long FETCH_INTERVAL_MS = 60000;
const int SCROLL_DELAY_MS = 35;

// ---------------- HARDWARE ----------------
#define LED_PIN   4
#define NUM_LEDS  128
#define WIDTH     16
#define HEIGHT    8
#define DIAL_PIN  36
#define JOY_UP    35
#define JOY_DOWN  34
#define JOY_LEFT  26
#define JOY_RIGHT 25
#define JOY_CLICK 27
#define BTN_A     23
#define BTN_B     18

CRGB leds[NUM_LEDS];
uint8_t hue = 0;

// ---------------- 5x7 FONT ----------------
// Needed chars only: space + - . % 0-9 A-Z. Row bytes, bit4 = left pixel.
struct Glyph { char c; uint8_t rows[7]; };
const Glyph FONT[] = {
  {' ', {0b00000,0b00000,0b00000,0b00000,0b00000,0b00000,0b00000}},
  {'+', {0b00000,0b00100,0b00100,0b11111,0b00100,0b00100,0b00000}},
  {'-', {0b00000,0b00000,0b00000,0b11111,0b00000,0b00000,0b00000}},
  {'.', {0b00000,0b00000,0b00000,0b00000,0b00000,0b01100,0b01100}},
  {'%', {0b11001,0b11010,0b00010,0b00100,0b01000,0b01011,0b10011}},
  {'0', {0b01110,0b10001,0b10011,0b10101,0b11001,0b10001,0b01110}},
  {'1', {0b00100,0b01100,0b00100,0b00100,0b00100,0b00100,0b01110}},
  {'2', {0b01110,0b10001,0b00001,0b00110,0b01000,0b10000,0b11111}},
  {'3', {0b11111,0b00010,0b00100,0b00010,0b00001,0b10001,0b01110}},
  {'4', {0b00010,0b00110,0b01010,0b10010,0b11111,0b00010,0b00010}},
  {'5', {0b11111,0b10000,0b11110,0b00001,0b00001,0b10001,0b01110}},
  {'6', {0b00110,0b01000,0b10000,0b11110,0b10001,0b10001,0b01110}},
  {'7', {0b11111,0b00001,0b00010,0b00100,0b01000,0b01000,0b01000}},
  {'8', {0b01110,0b10001,0b10001,0b01110,0b10001,0b10001,0b01110}},
  {'9', {0b01110,0b10001,0b10001,0b01111,0b00001,0b00010,0b01100}},
  {'A', {0b01110,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001}},
  {'B', {0b11110,0b10001,0b10001,0b11110,0b10001,0b10001,0b11110}},
  {'C', {0b01110,0b10001,0b10000,0b10000,0b10000,0b10001,0b01110}},
  {'D', {0b11110,0b10001,0b10001,0b10001,0b10001,0b10001,0b11110}},
  {'E', {0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b11111}},
  {'F', {0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b10000}},
  {'G', {0b01110,0b10001,0b10000,0b10111,0b10001,0b10001,0b01111}},
  {'H', {0b10001,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001}},
  {'I', {0b01110,0b00100,0b00100,0b00100,0b00100,0b00100,0b01110}},
  {'J', {0b00111,0b00010,0b00010,0b00010,0b00010,0b10010,0b01100}},
  {'K', {0b10001,0b10010,0b10100,0b11000,0b10100,0b10010,0b10001}},
  {'L', {0b10000,0b10000,0b10000,0b10000,0b10000,0b10000,0b11111}},
  {'M', {0b10001,0b11011,0b10101,0b10101,0b10001,0b10001,0b10001}},
  {'N', {0b10001,0b11001,0b10101,0b10011,0b10001,0b10001,0b10001}},
  {'O', {0b01110,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110}},
  {'P', {0b11110,0b10001,0b10001,0b11110,0b10000,0b10000,0b10000}},
  {'Q', {0b01110,0b10001,0b10001,0b10001,0b10101,0b10010,0b01101}},
  {'R', {0b11110,0b10001,0b10001,0b11110,0b10100,0b10010,0b10001}},
  {'S', {0b01111,0b10000,0b10000,0b01110,0b00001,0b00001,0b11110}},
  {'T', {0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b00100}},
  {'U', {0b10001,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110}},
  {'V', {0b10001,0b10001,0b10001,0b10001,0b10001,0b01010,0b00100}},
  {'W', {0b10001,0b10001,0b10001,0b10101,0b10101,0b11011,0b10001}},
  {'X', {0b10001,0b10001,0b01010,0b00100,0b01010,0b10001,0b10001}},
  {'Y', {0b10001,0b10001,0b01010,0b00100,0b00100,0b00100,0b00100}},
  {'Z', {0b11111,0b00001,0b00010,0b00100,0b01000,0b10000,0b11111}},
};
const int FONT_COUNT = sizeof(FONT) / sizeof(FONT[0]);

const uint8_t* glyphFor(char c) {
  if (c >= 'a' && c <= 'z') c -= 32;
  for (int i = 0; i < FONT_COUNT; i++)
    if (FONT[i].c == c) return FONT[i].rows;
  return FONT[0].rows;
}

uint16_t XY(uint8_t x, uint8_t y) { return y * WIDTH + x; }

void drawChar(int x, int y, char c, CRGB color) {
  const uint8_t* g = glyphFor(c);
  for (int row = 0; row < 7; row++)
    for (int col = 0; col < 5; col++) {
      int px = x + col, py = y + row;
      if (px >= 0 && px < WIDTH && py >= 0 && py < HEIGHT)
        if (g[row] & (1 << (4 - col))) leds[XY(px, py)] = color;
    }
}

void drawStr(int x, int y, const String& s, CRGB color) {
  for (unsigned int i = 0; i < s.length(); i++)
    drawChar(x + i * 6, y, s[i], color);
}

void drawCentered(const String& s, CRGB color) {
  int w = s.length() * 6 - 1;
  drawStr((WIDTH - w) / 2, 0, s, color);
}

CRGB hexColor(const String& h) {
  if (h.length() < 6) return CRGB::Black;
  long v = strtol(h.substring(0, 6).c_str(), nullptr, 16);
  return CRGB((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF);
}

bool pressed(uint8_t pin) {
  if (digitalRead(pin) == LOW) {
    delay(30);
    return digitalRead(pin) == LOW;
  }
  return false;
}

// ---------------- ONLINE PROJECT (selections 2 & 3) ----------------
struct Project {
  String name;
  String type = "scroll";   // scroll | static | effect | frames
  String text;
  CRGB color = CRGB::White;
  CRGB bg = CRGB::Black;
  int speed = 40;           // ms per step
  String effect = "rainbow";
  static const int MAX_FRAMES = 8;
  CRGB frames[MAX_FRAMES][NUM_LEDS];
  int frameHolds[MAX_FRAMES];
  int nFrames = 0;
  bool valid = false;
};
Project proj;

bool loadProject(int slotIdx) {
  // slotIdx 0 -> selection 2, 1 -> selection 3
  proj = Project();
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.begin(client, SLOT_URLS[slotIdx]);
  if (http.GET() != 200) { http.end(); return false; }
  String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload)) return false;

  String defName("slot");
  defName += String(slotIdx + 2);
  proj.name  = doc["name"] | defName;
  proj.type  = doc["type"] | "scroll";
  proj.text  = doc["text"] | "";
  proj.text.toUpperCase();
  String cs  = doc["color"] | "FFFFFF";
  String bs  = doc["bg"] | "000000";
  proj.color = hexColor(cs);
  proj.bg    = hexColor(bs);
  proj.speed = doc["speed"] | 40;
  if (proj.speed < 10) proj.speed = 10;
  proj.effect = doc["effect"] | "rainbow";

  if (proj.type == "frames") {
    JsonArray fr = doc["frames"].as<JsonArray>();
    int i = 0;
    for (JsonObject f : fr) {
      if (i >= Project::MAX_FRAMES) break;
      proj.frameHolds[i] = f["hold"] | 400;
      JsonArray px = f["pixels"].as<JsonArray>();
      int p = 0;
      for (const char* h : px) {
        if (p >= NUM_LEDS) break;
        proj.frames[i][p++] = hexColor(String(h));
      }
      while (p < NUM_LEDS) proj.frames[i][p++] = CRGB::Black;
      i++;
    }
    proj.nFrames = i;
    if (proj.nFrames == 0) return false;
  }
  proj.valid = true;
  return true;
}

// ---- renderers for online projects ----
void renderScroll(unsigned long now) {
  static unsigned long lastStep = 0;
  static int pos = 0;
  static String lastText = "";
  if (lastText != proj.text) { lastText = proj.text; pos = 0; }
  int w = proj.text.length() * 6;
  if (now - lastStep > (unsigned long)proj.speed) {
    lastStep = now;
    fill_solid(leds, NUM_LEDS, proj.bg);
    drawStr(WIDTH - pos, 0, proj.text, proj.color);
    FastLED.show();
    if (++pos > w + WIDTH) pos = 0;
  }
}

void renderStatic() {
  fill_solid(leds, NUM_LEDS, proj.bg);
  drawCentered(proj.text, proj.color);
  FastLED.show();
}

void renderEffect(unsigned long now) {
  static unsigned long lastStep = 0;
  if (now - lastStep < (unsigned long)proj.speed) return;
  lastStep = now;
  if (proj.effect == "plasma") {
    for (uint8_t x = 0; x < WIDTH; x++)
      for (uint8_t y = 0; y < HEIGHT; y++) {
        uint8_t v = sin8(x * 20 + hue) + cos8(y * 24 - hue);
        leds[XY(x, y)] = CHSV(v / 2, 255, 255);
      }
    hue += 3;
  } else if (proj.effect == "sparkle") {
    fadeToBlackBy(leds, NUM_LEDS, 40);
    leds[random16(NUM_LEDS)] = CHSV(random8(), 200, 255);
  } else {  // rainbow
    for (uint8_t x = 0; x < WIDTH; x++)
      for (uint8_t y = 0; y < HEIGHT; y++)
        leds[XY(x, y)] = CHSV(hue + x * 12 + y * 6, 255, 255);
    hue += 2;
  }
  FastLED.show();
}

void renderFrames(unsigned long now) {
  static unsigned long lastFlip = 0;
  static int fi = 0;
  if (now - lastFlip > (unsigned long)proj.frameHolds[fi]) {
    lastFlip = now;
    fi = (fi + 1) % proj.nFrames;
    for (int p = 0; p < NUM_LEDS; p++) leds[p] = proj.frames[fi][p];
    FastLED.show();
  }
}

// ---------------- STOCK TICKER (selection 1) ----------------
struct Segment { String text; CRGB color; };
Segment segs[20];
int nSegs = 0;
int totalWidth = 0;
bool haveData = false;

String displaySym(const String& s) {
  String t = s;
  t.toUpperCase();
  int dot = t.indexOf('.');
  if (dot > 0) t = t.substring(0, dot);
  return t;
}

bool fetchQuotes() {
  WiFiClientSecure client;
  client.setInsecure();
  HTTPClient http;
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
  http.setUserAgent("Mozilla/5.0");
  String url = "https://query1.finance.yahoo.com/v7/finance/spark?symbols=" +
               String(YAHOO_SYMBOLS) + "&interval=1d&range=5d";
  http.begin(client, url);
  if (http.GET() != 200) { http.end(); return false; }
  String payload = http.getString();
  http.end();

  JsonDocument doc;
  if (deserializeJson(doc, payload)) return false;
  JsonArray results = doc["spark"]["result"];
  if (results.isNull() || results.size() == 0) return false;

  // keep the watchlist order regardless of Yahoo's response order
  const char* order[] = {"AAPL","TSLA","META","GOOG","UNH","O","SCHD","FDVV",
                         "GLD","IBIT","GBTC","RIVN","VRTX","CBRS","SPCX"};
  int seg = 0;
  for (unsigned int oi = 0; oi < sizeof(order)/sizeof(order[0]) && seg < 20; oi++) {
    for (JsonObject r : results) {
      const char* sym = r["symbol"];
      if (!sym || strcmp(sym, order[oi]) != 0) continue;
      JsonObject meta = r["response"][0]["meta"];
      if (meta.isNull()) break;
      float price = meta["regularMarketPrice"] | 0.0f;
      float chg   = meta["regularMarketChangePercent"] | 0.0f;
      if (price <= 0) break;
      char buf[40];
      snprintf(buf, sizeof(buf), "%s %.2f %c%.2f%%    ",
               order[oi], price, chg >= 0 ? '+' : '-', fabsf(chg));
      segs[seg].text = String(buf);
      segs[seg].color = (chg >= 0) ? CRGB::Green : CRGB::Red;
      seg++;
      break;
    }
  }
  if (seg == 0) return false;
  nSegs = seg;
  totalWidth = 0;
  for (int i = 0; i < nSegs; i++) totalWidth += segs[i].text.length() * 6;
  haveData = true;
  return true;
}

void runTicker(unsigned long now) {
  static unsigned long lastFetch = 0;
  static unsigned long lastScroll = 0;
  static int scrollPos = 0;
  if (!haveData || now - lastFetch > FETCH_INTERVAL_MS) {
    if (fetchQuotes()) { lastFetch = now; scrollPos = 0; }
    else if (!haveData) {
      fill_solid(leds, NUM_LEDS, CRGB::Black);
      drawStr(0, 0, "FETCH FAIL", CRGB::Orange);
      FastLED.show();
      lastFetch = now;
      return;
    }
  }
  if (now - lastScroll > SCROLL_DELAY_MS) {
    lastScroll = now;
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    int x = WIDTH - scrollPos;
    for (int i = 0; i < nSegs; i++) {
      drawStr(x, 0, segs[i].text, segs[i].color);
      x += segs[i].text.length() * 6;
    }
    FastLED.show();
    if (++scrollPos > totalWidth + WIDTH) scrollPos = 0;
  }
}

// ---------------- MAIN ----------------
int slot = 1;  // 1 = ticker, 2/3 = online projects
unsigned long slotLoadedAt = 0;
bool slotLoadFailed = false;

bool nightTime() {
  struct tm t;
  if (!getLocalTime(&t)) return false;
  return (t.tm_hour >= 22 || t.tm_hour < 7);
}

void showSplash(const String& msg, CRGB color) {
  fill_solid(leds, NUM_LEDS, CRGB::Black);
  drawCentered(msg, color);
  FastLED.show();
  delay(900);
}

void selectSlot(int s) {
  slot = s;
  slotLoadFailed = false;
  if (slot == 1) {
    haveData = false;  // force fresh ticker fetch
    showSplash("TICKER", CRGB::Green);
  } else {
    showSplash("SLOT " + String(slot), CRGB::Cyan);
    showSplash("LOADING", CRGB::Yellow);
    if (loadProject(slot - 2)) {
      slotLoadedAt = millis();
    } else {
      slotLoadFailed = true;
    }
  }
}

void setup() {
  FastLED.addLeds<WS2812B, LED_PIN, GRB>(leds, NUM_LEDS);
  FastLED.setBrightness(64);

  pinMode(JOY_UP, INPUT_PULLUP);
  pinMode(JOY_DOWN, INPUT_PULLUP);
  pinMode(JOY_LEFT, INPUT_PULLUP);
  pinMode(JOY_RIGHT, INPUT_PULLUP);
  pinMode(JOY_CLICK, INPUT_PULLUP);
  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);

  configTime(0, 0, "pool.ntp.org", "time.nist.gov");
  setenv("TZ", "PST8PDT,M3.2.0,M11.1.0", 1);
  tzset();

  showSplash("KANO", CRGB::Magenta);
}

void loop() {
  unsigned long now = millis();
  static unsigned long lastWifiTry = 0;

  // Joystick left/right switches selection
  if (pressed(JOY_RIGHT)) { selectSlot(slot % 3 + 1); return; }
  if (pressed(JOY_LEFT))  { selectSlot((slot + 1) % 3 + 1); return; }

  // Keep WiFi alive
  if (WiFi.status() != WL_CONNECTED) {
    if (now - lastWifiTry > 10000) {
      lastWifiTry = now;
      WiFi.disconnect(); WiFi.reconnect();
    }
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    drawStr(1, 0, "NO WIFI", CRGB::Red);
    FastLED.show();
    delay(500);
    return;
  }

  // Brightness: dial, dimmed at night
  int dial = analogRead(DIAL_PIN);
  int b = map(dial, 0, 4095, 255, 8);
  if (nightTime()) b /= 4;
  FastLED.setBrightness(b);

  if (slot == 1) {
    runTicker(now);
    return;
  }

  // Selections 2/3: online project
  if (slotLoadFailed || !proj.valid) {
    fill_solid(leds, NUM_LEDS, CRGB::Black);
    drawStr(0, 0, "LOAD FAIL", CRGB::Red);
    FastLED.show();
    delay(2000);
    if (loadProject(slot - 2)) { slotLoadFailed = false; slotLoadedAt = now; }
    return;
  }
  if (now - slotLoadedAt > SLOT_REFRESH_MS) {
    if (loadProject(slot - 2)) slotLoadedAt = now;  // silent refresh
  }

  if (proj.type == "static")      renderStatic();
  else if (proj.type == "effect") renderEffect(now);
  else if (proj.type == "frames") renderFrames(now);
  else                           renderScroll(now);
}
