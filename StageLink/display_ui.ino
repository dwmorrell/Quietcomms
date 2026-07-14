/*
  display_ui.ino — everything drawn on screen, and all touch handling.

  Screen layout is 240x320 portrait. Five screens: home (category
  grid), category (message list), incoming (full-screen alert),
  history, and settings (see settings.ino for that one's actions).
*/

// ---------------- palette & backlight ----------------
// 24-bit theme hex -> the display's RGB565.
uint16_t col565(uint32_t rgb) {
  return tft.color565((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
}

// Everything visual reads these globals; they're filled from the THEME
// record picked by STAGELINK_THEME. (The COL_TEAL/AMBER/CORAL names are
// historic — they're the three category accent shades of whatever theme
// is active, routed through colorForId().)
void initPalette() {
  COL_BG       = col565(THEME.bg);
  COL_PANEL    = col565(THEME.panel);
  COL_TEXT     = col565(THEME.text);
  COL_TEXT_DIM = col565(THEME.textDim);
  COL_TEAL     = col565(THEME.shadeA);
  COL_AMBER    = col565(THEME.shadeB);
  COL_CORAL    = col565(THEME.shadeC);
  COL_ALERT    = col565(THEME.alert);
  COL_OK       = col565(THEME.ok);
  COL_BORDER   = col565(THEME.border);
  COL_GREEN    = col565(THEME.green);
  COL_YELLOW   = col565(THEME.yellow);
}

// Theme-aware button corner radius; 99 in the theme table = full pill.
int btnRadius(Rect r) {
  return (THEME.buttonRadius >= 99) ? r.h / 2 : min(r.h / 2, (int)THEME.buttonRadius);
}

void setBacklight(uint8_t percent) {
  if (percent > 100) percent = 100;
  uint32_t duty = map(percent, 0, 100, 0, 255);
  ledcWrite(TFT_BL_PIN, duty);
}

uint16_t colorForId(uint8_t id) {
  switch (id) {
    case 0: return COL_TEAL;
    case 1: return COL_AMBER;
    case 2: return COL_CORAL;
    case 3: return COL_ALERT;
    default: return COL_PANEL;
  }
}

// ---------------- small helpers ----------------
bool pointInRect(int x, int y, Rect r) {
  return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

Rect gridRect(int index, int count, int areaX, int areaY, int areaW, int areaH, int cols) {
  int rows = (count + cols - 1) / cols;
  int gap = 8;
  int cellW = (areaW - gap * (cols - 1)) / cols;
  int cellH = (areaH - gap * (rows - 1)) / rows;
  int col = index % cols;
  int row = index / cols;
  Rect r;
  r.x = areaX + col * (cellW + gap);
  r.y = areaY + row * (cellH + gap);
  r.w = cellW;
  r.h = cellH;
  return r;
}

// Picks whichever of near-black / near-white gives better contrast against
// a given accent, using WCAG-style relative luminance rather than a fixed
// threshold — near-white button text reads poorly on the mid-brightness
// teal/amber/coral accents even though it looks fine on the near-black panel.
uint16_t contrastTextFor(uint16_t bg565) {
  uint8_t r5 = (bg565 >> 11) & 0x1F;
  uint8_t g6 = (bg565 >> 5) & 0x3F;
  uint8_t b5 = bg565 & 0x1F;
  uint8_t bgR = (r5 * 527 + 23) >> 6;
  uint8_t bgG = (g6 * 259 + 33) >> 6;
  uint8_t bgB = (b5 * 527 + 23) >> 6;

  float rl = powf(bgR / 255.0f, 2.2f);
  float gl = powf(bgG / 255.0f, 2.2f);
  float bl = powf(bgB / 255.0f, 2.2f);
  float bgLum = 0.2126f * rl + 0.7152f * gl + 0.0722f * bl;

  float contrastWithWhite = 1.05f / (bgLum + 0.05f);
  float contrastWithBlack = (bgLum + 0.05f) / 0.05f;

  return (contrastWithBlack > contrastWithWhite) ? COL_BG : COL_TEXT;
}

// Word-wraps label into at most maxLines, measuring at whatever font is
// currently set. Returns the number of lines produced.
int wrapButtonLabel(const String &label, int maxWidth, String lines[], int maxLines) {
  int count = 0;
  String line = "";
  int start = 0;
  int len = label.length();
  while (start < len && count < maxLines) {
    int spaceIdx = label.indexOf(' ', start);
    String word = (spaceIdx == -1) ? label.substring(start) : label.substring(start, spaceIdx);
    String tryLine = line.length() ? (line + " " + word) : word;
    if (tft.textWidth(tryLine) > maxWidth && line.length()) {
      lines[count++] = line;
      line = word;
    } else {
      line = tryLine;
    }
    start = (spaceIdx == -1) ? len : spaceIdx + 1;
  }
  if (line.length() && count < maxLines) lines[count++] = line;
  return count;
}

// Blends two RGB565 colors; amount 0 = all c1, 255 = all c2. Operates
// directly on the packed 5/6/5-bit components — close enough for a subtle
// UI shading effect without the cost of unpacking to 8-bit RGB and back.
uint16_t blend565(uint16_t c1, uint16_t c2, uint8_t amount) {
  int r1 = (c1 >> 11) & 0x1F, g1 = (c1 >> 5) & 0x3F, b1 = c1 & 0x1F;
  int r2 = (c2 >> 11) & 0x1F, g2 = (c2 >> 5) & 0x3F, b2 = c2 & 0x1F;
  int r = r1 + ((r2 - r1) * amount) / 255;
  int g = g1 + ((g2 - g1) * amount) / 255;
  int b = b1 + ((b2 - b1) * amount) / 255;
  return (r << 11) | (g << 5) | b;
}

// Flat rounded fill with a subtle "bevel" gradient: a few concentric rings
// fading from a highlight right at the inside border back down to the flat
// base color a few pixels in. Confined to a narrow band near the edge, so
// it never reaches into the button's text area in the center.
void shadedRoundRect(Rect r, uint16_t base) {
  int radius = btnRadius(r);
  tft.fillRoundRect(r.x, r.y, r.w, r.h, radius, base);
  if (!THEME.useBevel) return;   // flat-fill themes (LCARS, Cyberpunk)

  const int insets[3] = {2, 4, 6};
  const uint8_t amounts[3] = {90, 55, 25};
  for (int i = 0; i < 3; i++) {
    int inset = insets[i];
    if (r.w - inset * 2 < 4 || r.h - inset * 2 < 4) continue;
    int rr = radius - inset;
    if (rr < 2) rr = 2;
    tft.drawRoundRect(r.x + inset, r.y + inset, r.w - inset * 2, r.h - inset * 2, rr,
                       blend565(base, COL_TEXT, amounts[i]));
  }
}

// Shifts every other button in a list so a run of same-color buttons (a
// category's message items, history rows) doesn't read as one flat block —
// index is the button's position in that list. Blends toward the base's
// OWN extreme (light gets lighter, dark gets darker): the alternation
// stays visible, and every alternate lands further from the mid-tones,
// where contrast against the button text is at its worst. Brightness is
// true luminance, not a single channel — themes have saturated accents
// (LCARS orange, Cyberpunk magenta) that a channel test misjudges.
uint16_t altShade(uint16_t base, int index) {
  if (index % 2 == 0) return base;
  int r = ((base >> 11) & 0x1F) << 3;
  int g = ((base >> 5) & 0x3F) << 2;
  int b = (base & 0x1F) << 3;
  int lum = (r * 77 + g * 151 + b * 28) >> 8;
  if (lum > 128) return blend565(base, COL_TEXT, 60);
  return blend565(base, COL_BG, 90);
}

// Quick full-brightness flash on a just-tapped button's own silhouette,
// shown briefly before the tap's resulting action/redraw — a visible
// "this one, got it" confirmation for every button, not just Sent/Seen
// which already change their own label as feedback.
void flashPress(Rect r) {
  tft.fillRoundRect(r.x, r.y, r.w, r.h, btnRadius(r), COL_TEXT);
  delay(80);
}

// The theme's category-button font at a shrink step: step 0 is the
// family's biggest, each step smaller, ending at built-in font 2.
// Returns false once past the end of the ladder.
bool setThemeButtonFont(int step) {
  switch (THEME.fontFamily) {
    case TF_PIXEL:
      if (step == 0) { tft.setFreeFont(&PressStart2P14pt); return true; }
      if (step == 1) { tft.setFreeFont(NULL); tft.setTextFont(2); return true; }
      return false;
    case TF_SERIF:
      if (step == 0) { tft.setFreeFont(&FreeSerifBold12pt7b); return true; }
      if (step == 1) { tft.setFreeFont(&FreeSerifBold9pt7b); return true; }
      if (step == 2) { tft.setFreeFont(NULL); tft.setTextFont(2); return true; }
      return false;
    case TF_MONO:
      if (step == 0) { tft.setFreeFont(&FreeMonoBold12pt7b); return true; }
      if (step == 1) { tft.setFreeFont(&FreeMonoBold9pt7b); return true; }
      if (step == 2) { tft.setFreeFont(NULL); tft.setTextFont(2); return true; }
      return false;
    default:   // TF_SANS
      if (step == 0) { tft.setFreeFont(&FreeSansBold12pt7b); return true; }
      if (step == 1) { tft.setFreeFont(&FreeSansBold9pt7b); return true; }
      if (step == 2) { tft.setFreeFont(NULL); tft.setTextFont(2); return true; }
      return false;
  }
}

// The theme's large display font — boot title only.
void setThemeTitleFont() {
  switch (THEME.fontFamily) {
    case TF_PIXEL: tft.setFreeFont(&PressStart2P16pt);   break;
    case TF_SERIF: tft.setFreeFont(&FreeSerifBold18pt7b); break;
    case TF_MONO:  tft.setFreeFont(&FreeMonoBold18pt7b);  break;
    default:       tft.setFreeFont(&FreeSansBold18pt7b);  break;
  }
}

// bold=true (category buttons: home screen categories, message items)
// walks the theme's font ladder from biggest to smallest until the label
// fits — one line if it can, wrapped to two if the button is tall
// enough — so long labels shrink instead of overflowing. bold=false
// (nav/utility actions) always uses the plain built-in font.
void drawButton(Rect r, const String &label, uint16_t accent, bool bold) {
  shadedRoundRect(r, accent);
  tft.drawRoundRect(r.x, r.y, r.w, r.h, btnRadius(r), COL_BORDER);
  tft.setTextColor(contrastTextFor(accent), accent);
  tft.setTextDatum(MC_DATUM);

  int maxWidth = r.w - 12;
  int cx = r.x + r.w / 2;
  int cy = r.y + r.h / 2 + 1;

  if (!bold) {
    tft.setFreeFont(NULL);
    tft.setTextFont(2);
    if (tft.textWidth(label) <= maxWidth) {
      tft.drawString(label, cx, cy);
    } else {
      String lines[2];
      int n = wrapButtonLabel(label, maxWidth, lines, 2);
      int lh = tft.fontHeight();
      int y = cy - (lh * n) / 2 + lh / 2;
      for (int i = 0; i < n; i++) { tft.drawString(lines[i], cx, y); y += lh; }
    }
    return;
  }

  bool drawn = false;
  for (int step = 0; !drawn && setThemeButtonFont(step); step++) {
    int lh = tft.fontHeight();
    if (lh > r.h - 2) continue;   // font taller than the button — shrink
    if (tft.textWidth(label) <= maxWidth) {
      tft.drawString(label, cx, cy);
      drawn = true;
    } else if (2 * lh <= r.h - 4) {
      String lines[2];
      int n = wrapButtonLabel(label, maxWidth, lines, 2);
      // only accept the wrap if nothing got truncated past 2 lines
      if (n == 1 || tft.textWidth(lines[n - 1]) <= maxWidth) {
        int y = cy - (lh * n) / 2 + lh / 2;
        for (int i = 0; i < n; i++) { tft.drawString(lines[i], cx, y); y += lh; }
        drawn = true;
      }
    }
  }
  if (!drawn) {
    // ladder exhausted (extreme label) — best effort at the smallest font
    tft.setFreeFont(NULL);
    tft.setTextFont(2);
    String lines[2];
    int n = wrapButtonLabel(label, maxWidth, lines, 2);
    int lh = tft.fontHeight();
    int y = cy - (lh * n) / 2 + lh / 2;
    for (int i = 0; i < n; i++) { tft.drawString(lines[i], cx, y); y += lh; }
  }

  tft.setFreeFont(NULL);
  tft.setTextFont(2);  // back to the default built-in font for other draw calls
}

void drawButton(Rect r, const String &label, uint16_t accent) {
  drawButton(r, label, accent, true);
}

void drawWrappedCentered(const String &text, int cx, int yStart, int maxWidth, int lineHeight) {
  String line = "";
  int y = yStart;
  int start = 0;
  int len = text.length();
  while (start < len) {
    int spaceIdx = text.indexOf(' ', start);
    String word = (spaceIdx == -1) ? text.substring(start) : text.substring(start, spaceIdx);
    String tryLine = line.length() ? (line + " " + word) : word;
    if (tft.textWidth(tryLine) > maxWidth && line.length()) {
      tft.drawString(line, cx, y);
      y += lineHeight;
      line = word;
    } else {
      line = tryLine;
    }
    start = (spaceIdx == -1) ? len : spaceIdx + 1;
  }
  if (line.length()) tft.drawString(line, cx, y);
}

// ---------------- boot / status ----------------
// Procedural ReelWorks film reel — drawn from geometry with anti-aliased
// circles (fillSmoothCircle) rather than a stored bitmap, which keeps it
// smooth at any size and lets the boot animation spin it for free: each
// frame just redraws the reel body with the holes at a new angle.
#define REEL_CX (SCREEN_W / 2)
#define REEL_CY 84

// Geometry measured from images/ReelWorks Logo.png (see logo_measure.ps1):
// thin outline 0.955R..R, black gap 0.855R..0.955R, disc to 0.855R, five
// big holes at ring 0.49R radius 0.18R (one at top), center cluster of
// five small dots at ring 0.13R staggered 36 deg, tiny center dot.
void drawReelFrame(float angleDeg) {
  // body only — the outer outline and gap ring are static, drawn once
  tft.fillSmoothCircle(REEL_CX, REEL_CY, 38, COL_TEXT, COL_BG);
  float rad = angleDeg * DEG_TO_RAD;
  for (int i = 0; i < 5; i++) {
    float a = rad + i * (2.0f * PI / 5.0f) - PI / 2.0f;
    int hx = REEL_CX + (int)roundf(22.0f * cosf(a));
    int hy = REEL_CY + (int)roundf(22.0f * sinf(a));
    tft.fillSmoothCircle(hx, hy, 8, COL_BG, COL_TEXT);
  }
  for (int i = 0; i < 5; i++) {
    float a = rad + i * (2.0f * PI / 5.0f) - PI / 2.0f + (PI / 5.0f);
    int dx = REEL_CX + (int)roundf(6.0f * cosf(a));
    int dy = REEL_CY + (int)roundf(6.0f * sinf(a));
    tft.fillSmoothCircle(dx, dy, 2, COL_BG, COL_TEXT);
  }
  tft.fillSmoothCircle(REEL_CX, REEL_CY, 1, COL_BG, COL_TEXT);   // center dot
}

// Boot reel animation state — the reel keeps turning for as long as the
// boot screen is on display: through the intro spin and, on the DJ unit,
// all the way through the wait for the private link (network.ino calls
// reelSpinTick() from its connect loop).
float bootReelAngle = 0;
unsigned long lastReelFrameAt = 0;

// Advances the reel one animation step (~4.5 degrees) at most every 90ms;
// safe to call as often as you like from any wait loop.
void reelSpinTick() {
  if (millis() - lastReelFrameAt < 90) return;
  lastReelFrameAt = millis();
  bootReelAngle += 4.5f;
  if (bootReelAngle >= 360.0f) bootReelAngle -= 360.0f;
  drawReelFrame(bootReelAngle);
}

// Short guaranteed "spinning in space" intro before networking starts.
void spinBootReel() {
  unsigned long until = millis() + 1500;
  while (millis() < until) {
    reelSpinTick();
    delay(10);
  }
}

void drawBootScreen() {
  tft.fillScreen(COL_BG);

  // sparse fixed-seed starfield so the reel hangs "in space"
  uint32_t seed = 12345;
  for (int i = 0; i < 34; i++) {
    seed = seed * 1103515245u + 12345u;
    int sx = (seed >> 16) % SCREEN_W;
    seed = seed * 1103515245u + 12345u;
    int sy = (seed >> 16) % SCREEN_H;
    tft.drawPixel(sx, sy, COL_TEXT_DIM);
  }

  // static rim: thin white outline with the black gap ring inside it
  tft.fillSmoothCircle(REEL_CX, REEL_CY, 44, COL_TEXT, COL_BG);
  tft.fillSmoothCircle(REEL_CX, REEL_CY, 42, COL_BG, COL_TEXT);
  drawReelFrame(0);

  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setTextDatum(MC_DATUM);
  setThemeTitleFont();
  tft.drawString("StageLink", SCREEN_W / 2, 150);
  tft.setFreeFont(NULL);
  tft.setTextFont(2);
  tft.setTextColor(COL_TEXT_DIM, COL_BG);
  tft.drawString(IS_DJ_UNIT ? "DJ unit" : "FOH unit", SCREEN_W / 2, 180);
}

void drawStatusLine(const String &msg) {
  tft.fillRect(0, 190, SCREEN_W, 30, COL_BG);
  tft.setTextColor(COL_TEXT_DIM, COL_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.drawString(msg, SCREEN_W / 2, 205);
}

// Link strength of whatever this unit's radio is actually using: own STA
// RSSI when joined to a network (DJ always; FOH on venue wifi), or the
// connected DJ station's RSSI read from the AP's station list when FOH is
// hosting Direct Link. valid=false when there's nothing to measure yet.
int currentRssi(bool &valid) {
#if IS_DJ_UNIT
  valid = (WiFi.status() == WL_CONNECTED);
  return valid ? WiFi.RSSI() : 0;
#else
  if (netMode == MODE_VENUE) {
    valid = (WiFi.status() == WL_CONNECTED);
    return valid ? WiFi.RSSI() : 0;
  }
  wifi_sta_list_t staList;
  if (esp_wifi_ap_get_sta_list(&staList) == ESP_OK && staList.num > 0) {
    valid = true;
    return staList.sta[0].rssi;
  }
  valid = false;
  return 0;
#endif
}

// Three-state connection indicator color:
//   red    — no network yet (searching / venue wifi not joined)
//   yellow — on a network but not linked to the other unit
//   green  — linked to the other unit (a peer WebSocket is up)
// FOH hosting Direct Link counts its own AP as "on a network," so it goes
// yellow (never red) while waiting for the DJ unit to join.
uint16_t connectionColor() {
  if (wsConnected) return COL_GREEN;
  bool networkUp;
#if IS_DJ_UNIT
  networkUp = (WiFi.status() == WL_CONNECTED);
#else
  networkUp = (netMode == MODE_VENUE) ? (WiFi.status() == WL_CONNECTED) : true;
#endif
  return networkUp ? COL_YELLOW : COL_ALERT;
}

// Blocky 4-bar signal indicator in the status bar. 4 states by RSSI
// (>=-55 / >=-65 / >=-75 / weaker = 4/3/2/1 bars); all bars sit dim
// until there's a live link to measure.
void drawSignalBars() {
  bool valid;
  int rssi = currentRssi(valid);
  int bars = 0;
  if (valid) {
    if (rssi >= -55) bars = 4;
    else if (rssi >= -65) bars = 3;
    else if (rssi >= -75) bars = 2;
    else bars = 1;
  }
  const int heights[4] = {4, 7, 10, 13};
  int bx = SCREEN_W - 72;
  for (int i = 0; i < 4; i++) {
    int x = bx + i * 4;
    tft.fillRect(x, 4, 3, 16, COL_BG);
    tft.fillRect(x, 20 - heights[i], 3, heights[i], (i < bars) ? COL_TEXT : COL_PANEL);
  }
}

// "DJ connected" / "FOH waiting..." label — extracted so the periodic
// refresher can update it live along with the dot, not just on full
// screen redraws. Clears its own region first since the two strings
// differ in length.
void drawStatusLabel() {
  tft.fillRect(24, 0, SCREEN_W - 100, 24, COL_BG);
  tft.setTextColor(COL_TEXT_DIM, COL_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(2);
  String label = String(IS_DJ_UNIT ? "DJ" : "FOH") + (wsConnected ? " connected" : " waiting...");
  tft.drawString(label, 24, 6);
}

// Keeps the live parts of the status bar (connection dot, label, signal
// bars) fresh between full screen redraws — every 5s normally, and
// immediately when the peer link comes up or drops so the dot and the
// "connected/waiting" text never disagree.
void serviceStatusBar() {
  static unsigned long lastRefresh = 0;
  static bool lastWsConnected = false;
  bool stateChanged = (wsConnected != lastWsConnected);
  if (!stateChanged && millis() - lastRefresh < 5000) return;
  lastRefresh = millis();
  lastWsConnected = wsConnected;
  if (currentScreen == SCR_INCOMING || currentScreen == SCR_TRANSIENT) return;
  tft.fillRect(7, 7, 10, 10, connectionColor());
  drawStatusLabel();
  drawSignalBars();
}

void drawStatusBar() {
  tft.fillRect(0, 0, SCREEN_W, 24, COL_BG);
  tft.drawFastHLine(0, 24, SCREEN_W, COL_BORDER);

  tft.fillRect(7, 7, 10, 10, connectionColor());
  drawStatusLabel();
  drawSignalBars();

  // history icon: blocky square-bezel clock
  tft.fillRect(SCREEN_W - 48, 4, 16, 16, COL_PANEL);
  tft.drawRect(SCREEN_W - 48, 4, 16, 16, COL_BORDER);
  tft.fillRect(SCREEN_W - 41, 7, 2, 6, COL_TEXT_DIM);   // minute hand
  tft.fillRect(SCREEN_W - 41, 12, 5, 2, COL_TEXT_DIM);  // hour hand

  // settings icon: blocky plus/gear silhouette
  tft.fillRect(SCREEN_W - 24, 4, 16, 16, COL_PANEL);
  tft.drawRect(SCREEN_W - 24, 4, 16, 16, COL_BORDER);
  tft.fillRect(SCREEN_W - 21, 9, 10, 6, COL_TEXT_DIM);  // horizontal bar
  tft.fillRect(SCREEN_W - 19, 7, 6, 10, COL_TEXT_DIM);  // vertical bar
}

// ---------------- home screen ----------------
void drawHome() {
  tft.fillScreen(COL_BG);
  drawStatusBar();
  for (int i = 0; i < CATEGORY_COUNT; i++) {
    Rect r = gridRect(i, CATEGORY_COUNT, 10, 32, SCREEN_W - 20, SCREEN_H - 42, 1);
    drawButton(r, CATEGORIES[i].name, altShade(colorForId(CATEGORIES[i].colorId), i));
  }
}

void tapHome(int x, int y) {
  for (int i = 0; i < CATEGORY_COUNT; i++) {
    Rect r = gridRect(i, CATEGORY_COUNT, 10, 32, SCREEN_W - 20, SCREEN_H - 42, 1);
    if (pointInRect(x, y, r)) {
      flashPress(r);
      activeCategory = i;
      activeSubcategory = -1;   // land on the submenu when the category has one
      currentScreen = SCR_CATEGORY;
      drawCategory();
      return;
    }
  }
}

// ---------------- category / item screen ----------------
// The category screen shows one of three things for CATEGORIES[activeCategory]:
//   - a leaf category's items (subcats == nullptr)
//   - a parent's SUBMENU of child categories (subcats set, activeSubcategory -1)
//   - a child category's items (subcats set, activeSubcategory >= 0)
// currentCatDef() resolves which definition the item list/taps operate on.
const PromptCategory* currentCatDef() {
  if (activeCategory < 0 || activeCategory >= CATEGORY_COUNT) return nullptr;
  const PromptCategory* c = &CATEGORIES[activeCategory];
  if (c->subcats && activeSubcategory >= 0 && activeSubcategory < c->subcatCount)
    return &c->subcats[activeSubcategory];
  return c;
}

bool onSubmenu() {
  const PromptCategory &cat = CATEGORIES[activeCategory];
  return cat.subcats != nullptr && activeSubcategory < 0;
}

// Draws the shared bottom row (Back everywhere; DJ also gets the standing
// "Come to stage" escape hatch) and returns the rects via out-params so
// tapCategory uses identical geometry.
void categoryBottomRow(Rect &back, Rect &stage, bool draw) {
#if IS_DJ_UNIT
  back  = {10, SCREEN_H - 36, (SCREEN_W - 24) / 2, 30};
  stage = {14 + (SCREEN_W - 24) / 2, SCREEN_H - 36, (SCREEN_W - 24) / 2, 30};
  if (draw) {
    drawButton(back, "Back", COL_PANEL, false);
    drawButton(stage, "Come to stage", COL_ALERT, false);
  }
#else
  back  = {10, SCREEN_H - 36, SCREEN_W - 20, 30};
  stage = {0, 0, 0, 0};
  if (draw) drawButton(back, "Back", COL_PANEL, false);
#endif
}

void drawCategory() {
  if (activeCategory < 0 || activeCategory >= CATEGORY_COUNT) { currentScreen = SCR_HOME; drawHome(); return; }
  const PromptCategory &parent = CATEGORIES[activeCategory];
  tft.fillScreen(COL_BG);
  drawStatusBar();

  int areaY = 48, areaH = SCREEN_H - 48 - 42;
  Rect back, stage;

  if (onSubmenu()) {
    // submenu: the parent's child categories as big buttons
    tft.setTextColor(COL_TEXT_DIM, COL_BG);
    tft.setTextDatum(TL_DATUM);
    tft.setTextFont(2);
    tft.drawString(parent.name, 12, 30);

    for (int i = 0; i < parent.subcatCount; i++) {
      Rect r = gridRect(i, parent.subcatCount, 10, areaY, SCREEN_W - 20, areaH, 1);
      drawButton(r, parent.subcats[i].name, altShade(colorForId(parent.subcats[i].colorId), i));
    }
    categoryBottomRow(back, stage, true);
    return;
  }

  const PromptCategory &cat = *currentCatDef();
  tft.setTextColor(COL_TEXT_DIM, COL_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(2);
  // breadcrumb on child screens ("Sound > Monitors") so the DJ always
  // knows where Back leads
  String title = (&cat != &parent) ? String(parent.name) + " > " + cat.name : String(cat.name);
  tft.drawString(title, 12, 30);

  for (int i = 0; i < cat.itemCount; i++) {
    Rect r = gridRect(i, cat.itemCount, 10, areaY, SCREEN_W - 20, areaH, 1);
    uint16_t accent = altShade(colorForId(cat.colorId), i);
    if (cat.items[i].kind == KIND_THUMBS) {
      // thumbs-up renders as a drawn glyph, never as font text; sized to
      // leave a few px of margin inside whatever row height the list has
      shadedRoundRect(r, accent);
      tft.drawRoundRect(r.x, r.y, r.w, r.h, btnRadius(r), COL_BORDER);
      drawThumbsGlyph(r.x + r.w / 2, r.y + r.h / 2, (r.h - 6) / 16.0f,
                      contrastTextFor(accent), accent);
    } else {
      // drawButton's font ladder shrinks labels to fit even short rows
      drawButton(r, cat.items[i].label, accent, true);
    }
  }

  categoryBottomRow(back, stage, true);

  // A full redraw paints over an in-flight ack check — put it back if the
  // acknowledged button lives on this screen and the 10s window is open.
  if (ackCheckActive && activeCategory == ackCheckCategory &&
      activeSubcategory == ackCheckSubcategory) {
    ackCheckLastFrame = 2;
    drawAckCheck(2);
  }
}

void tapCategory(int x, int y) {
  const PromptCategory &parent = CATEGORIES[activeCategory];
  int areaY = 48, areaH = SCREEN_H - 48 - 42;
  Rect back, stage;
  categoryBottomRow(back, stage, false);

#if IS_DJ_UNIT
  if (pointInRect(x, y, stage)) {
    flashPress(stage);
    sendPromptMessage("Urgent", "Come to stage");
    pendingAckText = "Come to stage";
    pendingAckCategory = activeCategory;
    pendingAckSubcategory = activeSubcategory;
    pendingAckRect = stage;
    flashSent(stage, COL_ALERT);
    return;
  }
#endif
  if (pointInRect(x, y, back)) {
    flashPress(back);
    if (parent.subcats && activeSubcategory >= 0) {
      // child list -> back to the submenu
      activeSubcategory = -1;
      drawCategory();
    } else {
      currentScreen = SCR_HOME;
      drawHome();
    }
    return;
  }

  if (onSubmenu()) {
    for (int i = 0; i < parent.subcatCount; i++) {
      Rect r = gridRect(i, parent.subcatCount, 10, areaY, SCREEN_W - 20, areaH, 1);
      if (pointInRect(x, y, r)) {
        flashPress(r);
        activeSubcategory = i;
        drawCategory();
        return;
      }
    }
    return;
  }

  const PromptCategory &cat = *currentCatDef();
  for (int i = 0; i < cat.itemCount; i++) {
    Rect r = gridRect(i, cat.itemCount, 10, areaY, SCREEN_W - 20, areaH, 1);
    if (pointInRect(x, y, r)) {
      const PromptItem &item = cat.items[i];
      if (item.kind == KIND_QUESTION) {
        // question: peer answers via its own popup; the answer arrives as
        // a transient, so there's no Seen ack to wait for
        sendQuestion(item.label);
        flashSent(r, colorForId(cat.colorId));
      } else if (item.kind == KIND_THUMBS) {
        sendTransient(THUMBS_WIRE_TEXT);
        flashSent(r, colorForId(cat.colorId));
      } else if (cat.colorId == 3) {
        startPendingUrgent(i, r);
      } else {
        sendPromptMessage(cat.name, item.label);
        pendingAckText = item.label;
        pendingAckCategory = activeCategory;
        pendingAckSubcategory = activeSubcategory;
        pendingAckRect = r;
        flashSent(r, colorForId(cat.colorId));
      }
      return;
    }
  }
}

void flashSent(Rect r, uint16_t accent) {
  // quick "sent" flash on the button itself, then redraw the category screen
  drawButton(r, "Sent", COL_OK);
  delay(350);
  drawCategory();
}

// ---------------- "seen" acknowledgment check ----------------
// Green check that pops on the button whose request was just acknowledged
// by the other unit — the artist's proof their message was actually seen.
void drawAckCheck(int frame) {
  int size = (frame == 0) ? 12 : (frame == 1) ? 18 : 24;
  int cx = ackCheckRect.x + ackCheckRect.w - 16;
  int cy = ackCheckRect.y + ackCheckRect.h / 2;
  int x = cx - size / 2, y = cy - size / 2;
  tft.fillRoundRect(x, y, size, size, 4, COL_GREEN);
  tft.drawRoundRect(x, y, size, size, 4, COL_BG);
  if (frame >= 2) {
    tft.drawLine(cx - 6, cy,     cx - 2, cy + 4, COL_TEXT);
    tft.drawLine(cx - 6, cy + 1, cx - 2, cy + 5, COL_TEXT);
    tft.drawLine(cx - 2, cy + 4, cx + 6, cy - 4, COL_TEXT);
    tft.drawLine(cx - 2, cy + 5, cx + 6, cy - 3, COL_TEXT);
  }
}

void serviceAckCheck() {
  if (!ackCheckActive) return;
  bool onOwningScreen = (currentScreen == SCR_CATEGORY &&
                         activeCategory == ackCheckCategory &&
                         activeSubcategory == ackCheckSubcategory);
  unsigned long elapsed = millis() - ackCheckStart;
  if (elapsed >= 10000) {
    ackCheckActive = false;
    if (onOwningScreen) drawCategory();
    return;
  }
  if (!onOwningScreen) return;
  int frame = (elapsed < 120) ? 0 : (elapsed < 240) ? 1 : 2;
  if (frame != ackCheckLastFrame) {
    ackCheckLastFrame = frame;
    drawAckCheck(frame);
  }
}

// ---------------- urgent hold-to-confirm ----------------
void startPendingUrgent(int i, Rect r) {
  pendingUrgentIndex = i;
  pendingUrgentStart = millis();
  pendingUrgentRect = r;
  drawButton(r, "Hold to confirm...", COL_ALERT);
}

void cancelPendingUrgent() {
  if (pendingUrgentIndex == -1) return;
  pendingUrgentIndex = -1;
  if (currentScreen == SCR_CATEGORY) drawCategory();
}

void confirmPendingUrgent() {
  int i = pendingUrgentIndex;
  pendingUrgentIndex = -1;
  if (currentScreen != SCR_CATEGORY) return;
  const PromptCategory* catPtr = currentCatDef();
  if (!catPtr || onSubmenu()) return;
  const PromptCategory &cat = *catPtr;
  if (i < 0 || i >= cat.itemCount) return;
  sendPromptMessage(cat.name, cat.items[i].label);
  pendingAckText = cat.items[i].label;
  pendingAckCategory = activeCategory;
  pendingAckSubcategory = activeSubcategory;
  pendingAckRect = pendingUrgentRect;
  flashSent(pendingUrgentRect, colorForId(cat.colorId));
}

void serviceUrgentHold() {
  if (pendingUrgentIndex == -1) return;

  if (currentScreen != SCR_CATEGORY) { pendingUrgentIndex = -1; return; }

  if (!touch.touched()) { cancelPendingUrgent(); return; }

  TS_Point p = touch.getPoint();
  int sx, sy;
  mapTouch(p.x, p.y, sx, sy);
  if (!pointInRect(sx, sy, pendingUrgentRect)) { cancelPendingUrgent(); return; }

  if (millis() - pendingUrgentStart >= URGENT_HOLD_MS) {
    confirmPendingUrgent();
  }
}

// ---------------- incoming message overlay ----------------
#define INCOMING_AUTO_DISMISS_MS 30000

void showIncomingOverlay() {
  if (currentScreen != SCR_INCOMING) savedScreenBeforeIncoming = currentScreen;
  currentScreen = SCR_INCOMING;
  incomingAcked = false;
  incomingShownAtMillis = millis();
  backlightBoostForAlert();
  drawIncoming();
}

void backlightBoostForAlert() {
  uint8_t boosted = backlightLevel < 80 ? backlightLevel + 25 : 100;
  setBacklight(boosted);
}

// The three canned answers a question overlay offers.
const char* const QUESTION_ANSWERS[3] = {"More", "Less", "Just Right"};

Rect questionAnswerRect(int i) {
  return {30, 196 + i * 42, SCREEN_W - 60, 34};
}

void drawIncoming() {
  tft.fillScreen(COL_BG);
  tft.fillRect(0, 0, SCREEN_W, 44, COL_ALERT);
  tft.setTextColor(contrastTextFor(COL_ALERT), COL_ALERT);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  const char* header = incomingIsQuestion
      ? (IS_DJ_UNIT ? "QUESTION FROM FOH" : "QUESTION FROM DJ")
      : (IS_DJ_UNIT ? "MESSAGE FROM FOH" : "MESSAGE FROM DJ");
  tft.drawString(header, SCREEN_W / 2, 22);

  tft.setTextColor(COL_TEXT_DIM, COL_BG);
  tft.setTextFont(2);
  tft.drawString(incomingCategory, SCREEN_W / 2, 76);

  tft.setTextColor(COL_TEXT, COL_BG);
  setThemeButtonFont(0);   // theme's biggest button font for the message body
  drawWrappedCentered(incomingText, SCREEN_W / 2, 110, SCREEN_W - 40, tft.fontHeight() + 6);
  tft.setFreeFont(NULL);
  tft.setTextFont(2);

  if (incomingIsQuestion) {
    // A question is answered, not "seen": three stacked canned answers
    // replace the Seen button, and picking one both replies and closes.
    for (int i = 0; i < 3; i++) {
      drawButton(questionAnswerRect(i), QUESTION_ANSWERS[i], altShade(COL_PANEL, i), false);
    }
    return;
  }

  // Single-state green Seen button in the plain built-in font — one tap
  // acknowledges AND closes, no second "tap to close" step.
  Rect seenBtn = {30, 240, SCREEN_W - 60, 56};
  shadedRoundRect(seenBtn, COL_GREEN);
  tft.drawRoundRect(seenBtn.x, seenBtn.y, seenBtn.w, seenBtn.h, btnRadius(seenBtn), COL_BORDER);
  tft.setTextColor(contrastTextFor(COL_GREEN), COL_GREEN);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.drawString("Seen", seenBtn.x + seenBtn.w / 2, seenBtn.y + seenBtn.h / 2);
}

void tapIncoming(int x, int y) {
  if (incomingIsQuestion) {
    for (int i = 0; i < 3; i++) {
      Rect r = questionAnswerRect(i);
      if (pointInRect(x, y, r)) {
        flashPress(r);
        sendTransient(QUESTION_ANSWERS[i]);   // the answer IS the acknowledgment
        incomingIsQuestion = false;
        incomingAcked = true;
        dismissIncoming();
        return;
      }
    }
    return;   // taps elsewhere do nothing — answering is the way out
  }

  Rect seenBtn = {30, 240, SCREEN_W - 60, 56};
  if (pointInRect(x, y, seenBtn)) {
    flashPress(seenBtn);
    sendAck(incomingText);
    incomingAcked = true;
    dismissIncoming();
  }
  // taps elsewhere do nothing — Seen is the one explicit way out
}

// Redraws whichever regular screen `s` names (overlays are not restorable
// targets — anything unexpected lands on home).
void redrawScreen(Screen s) {
  currentScreen = s;
  switch (s) {
    case SCR_CATEGORY: drawCategory();   break;
    case SCR_HISTORY:  drawHistory();    break;
    case SCR_SETTINGS: drawSettings();   break;
    case SCR_DEVINFO:  drawDeviceInfo(); break;
    default:           currentScreen = SCR_HOME; drawHome(); break;
  }
}

void dismissIncoming() {
  setBacklight(backlightLevel);
  incomingIsQuestion = false;
  redrawScreen(savedScreenBeforeIncoming);
}

// ---------------- transient banner (T| wire type) ----------------
// Brief self-dismissing display — no Seen, no ack, no expiry logic. Used
// by question answers ("More"/"Less"/"Just Right") and the thumbs-up.
#define TRANSIENT_SHOW_MS 1500

// Smooth-primitive thumbs-up (same construction as the boot reel): a solid
// rounded fist with three finger-crease lines knocked out in the surface
// color, and a thumb column merged into its top-left. u is the unit size
// (glyph spans ~16u square); bg is the surface behind the glyph, needed
// for the anti-aliased edges. Geometry prototyped in thumb_preview.ps1.
void drawThumbsGlyph(int cx, int cy, float u, uint16_t color, uint16_t bg) {
  int ox = cx - (int)(8.0f * u);
  int oy = cy - (int)(8.0f * u);
  tft.fillSmoothRoundRect(ox + (int)(2.6f * u), oy + (int)(6.4f * u),
                          (int)(12.0f * u), (int)(9.2f * u), (int)(2.4f * u), color, bg);
  for (int i = 1; i <= 3; i++) {
    int y = oy + (int)((6.4f + i * 2.3f) * u);
    tft.fillSmoothRoundRect(ox + (int)(7.0f * u), y,
                            (int)(8.2f * u), max(1, (int)(0.55f * u)),
                            max(1, (int)(0.27f * u)), bg, color);
  }
  tft.fillSmoothRoundRect(ox + (int)(2.0f * u), oy + (int)(1.0f * u),
                          (int)(4.2f * u), (int)(8.0f * u), (int)(2.1f * u), color, bg);
}

void showTransient(const String &text) {
  if (currentScreen != SCR_TRANSIENT) savedScreenBeforeTransient = currentScreen;
  // never trap an overlay as the restore target
  if (savedScreenBeforeTransient == SCR_INCOMING) savedScreenBeforeTransient = savedScreenBeforeIncoming;
  transientText = text;
  transientShownAt = millis();
  currentScreen = SCR_TRANSIENT;

  tft.fillScreen(COL_BG);
  Rect panel = {20, 110, SCREEN_W - 40, 100};
  shadedRoundRect(panel, COL_PANEL);
  tft.drawRoundRect(panel.x, panel.y, panel.w, panel.h, btnRadius(panel), COL_BORDER);

  if (text == THUMBS_WIRE_TEXT) {
    drawThumbsGlyph(SCREEN_W / 2, 160, 5.5f, COL_GREEN, COL_PANEL);
  } else {
    tft.setTextColor(COL_TEXT, COL_PANEL);
    tft.setTextDatum(MC_DATUM);
    setThemeButtonFont(0);
    tft.drawString(text, SCREEN_W / 2, 160);
    tft.setFreeFont(NULL);
    tft.setTextFont(2);
  }

  tft.setTextColor(COL_TEXT_DIM, COL_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.drawString(IS_DJ_UNIT ? "from FOH" : "from DJ", SCREEN_W / 2, 232);
}

void serviceTransient() {
  if (currentScreen != SCR_TRANSIENT) return;
  if (millis() - transientShownAt >= TRANSIENT_SHOW_MS) {
    redrawScreen(savedScreenBeforeTransient);
  }
}

void serviceIncomingAutoDismiss() {
#if IS_DJ_UNIT
  // FOH -> DJ prompts expire unseen after 25s: if the artist hasn't tapped
  // Seen by then, the overlay clears itself and no ack is sent — FOH's
  // green check never fires, which correctly reads as "not acknowledged."
  if (currentScreen == SCR_INCOMING && !incomingAcked &&
      millis() - incomingShownAtMillis > 25000) {
    dismissIncoming();
    return;
  }
#endif
  if (currentScreen == SCR_INCOMING && incomingAcked &&
      millis() - incomingAckedAtMillis > INCOMING_AUTO_DISMISS_MS) {
    dismissIncoming();
  }
}

// Smooth breathing pulse on the red LED while an incoming message is up:
// a 3s cycle — 1s fading from off to fully on, 1s back down, then 1s of
// rest at fully off before the next breath — updated every loop pass
// through 8-bit PWM (~255 steps per ramp, no visible stepping).
// Active-low LED, so the duty is inverted.
void serviceAlertPulse() {
  if (currentScreen != SCR_INCOMING) {
    ledcWrite(LED_R, 255);   // off
    digitalWrite(LED_G, HIGH); digitalWrite(LED_B, HIGH);
    return;
  }
  unsigned long t = millis() % 3000;
  int level;
  if (t < 1000)      level = map(t, 0, 999, 0, 255);
  else if (t < 2000) level = map(t, 1000, 1999, 255, 0);
  else               level = 0;   // rest, fully off
  ledcWrite(LED_R, 255 - level);
}

// ---------------- history ----------------
void addHistory(const String &text, const String &category, bool incoming, bool acked) {
  // shift everything down (index 0 = most recent)
  int last = HISTORY_MAX - 1;
  for (int i = last; i > 0; i--) history[i] = history[i - 1];
  history[0].text = text;
  history[0].category = category;
  history[0].incoming = incoming;
  history[0].acked = acked;
  history[0].atMillis = millis();
  if (historyCount < HISTORY_MAX) historyCount++;
}

void markHistoryAcked(const String &text) {
  // First matching ack arms the green-check feedback and clears the
  // pending slot — acks for duplicate/repeated sends of the same text
  // fall through here without re-triggering the check.
  if (pendingAckText.length() && text == pendingAckText) {
    pendingAckText = "";
    ackCheckActive = true;
    ackCheckStart = millis();
    ackCheckCategory = pendingAckCategory;
    ackCheckSubcategory = pendingAckSubcategory;
    ackCheckRect = pendingAckRect;
    ackCheckLastFrame = -1;
  }

  for (int i = 0; i < historyCount; i++) {
    if (!history[i].incoming && !history[i].acked && history[i].text == text) {
      history[i].acked = true;
      break;
    }
  }
  if (currentScreen == SCR_HISTORY) drawHistory();
}

String relativeTime(unsigned long atMillis) {
  unsigned long diff = (millis() - atMillis) / 1000; // seconds
  if (diff < 5) return "just now";
  if (diff < 60) return String(diff) + "s ago";
  unsigned long mins = diff / 60;
  if (mins < 60) return String(mins) + "m ago";
  unsigned long hrs = mins / 60;
  return String(hrs) + "h ago";
}

void goHistory() {
  historyPage = 0;
  currentScreen = SCR_HISTORY;
  drawHistory();
}

#define HISTORY_PER_PAGE 4

void drawHistory() {
  tft.fillScreen(COL_BG);
  drawStatusBar();
  tft.setTextColor(COL_TEXT_DIM, COL_BG);
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(2);
  tft.drawString("History", 12, 30);

  if (historyCount == 0) {
    tft.setTextColor(COL_TEXT_DIM, COL_BG);
    tft.setTextDatum(MC_DATUM);
    tft.drawString("No messages yet", SCREEN_W / 2, 160);
  } else {
    int startIdx = historyPage * HISTORY_PER_PAGE;
    int rowY = 50;
    for (int i = 0; i < HISTORY_PER_PAGE; i++) {
      int idx = startIdx + i;
      if (idx >= historyCount) break;
      HistoryEntry &h = history[idx];
      Rect row = {10, rowY, SCREEN_W - 20, 34};
      shadedRoundRect(row, altShade(COL_PANEL, i));

      String dirLabel = h.incoming ? "IN " : "OUT";
      String ackMark = (!h.incoming && h.acked) ? " (seen)" : "";
      tft.setTextColor(h.incoming ? COL_CORAL : COL_TEXT_DIM, COL_PANEL);
      tft.setTextDatum(TL_DATUM);
      tft.setTextFont(2);
      tft.drawString(dirLabel + "  " + h.text + ackMark, row.x + 8, row.y + 5);
      tft.setTextColor(COL_TEXT_DIM, COL_PANEL);
      tft.drawString(relativeTime(h.atMillis), row.x + 8, row.y + 19);

      rowY += 40;
    }
  }

  bool hasPrev = historyPage > 0;
  bool hasNext = (historyPage + 1) * HISTORY_PER_PAGE < historyCount;
  Rect prevBtn = {10, SCREEN_H - 36, 90, 30};
  Rect backBtn = {(SCREEN_W - 100) / 2, SCREEN_H - 36, 100, 30};
  Rect nextBtn = {SCREEN_W - 100, SCREEN_H - 36, 90, 30};
  drawButton(prevBtn, "Prev", altShade(hasPrev ? COL_PANEL : COL_BG, 0), false);
  drawButton(backBtn, "Back", altShade(COL_PANEL, 1), false);
  drawButton(nextBtn, "Next", altShade(hasNext ? COL_PANEL : COL_BG, 2), false);
}

void tapHistory(int x, int y) {
  bool hasPrev = historyPage > 0;
  bool hasNext = (historyPage + 1) * HISTORY_PER_PAGE < historyCount;
  Rect prevBtn = {10, SCREEN_H - 36, 90, 30};
  Rect backBtn = {(SCREEN_W - 100) / 2, SCREEN_H - 36, 100, 30};
  Rect nextBtn = {SCREEN_W - 100, SCREEN_H - 36, 90, 30};

  if (hasPrev && pointInRect(x, y, prevBtn)) { flashPress(prevBtn); historyPage--; drawHistory(); return; }
  if (hasNext && pointInRect(x, y, nextBtn)) { flashPress(nextBtn); historyPage++; drawHistory(); return; }
  if (pointInRect(x, y, backBtn)) { flashPress(backBtn); currentScreen = SCR_HOME; drawHome(); return; }
}

// ---------------- settings entry point ----------------
void goSettings() {
  currentScreen = SCR_SETTINGS;
  drawSettings();
}

// ---------------- touch input ----------------
// Intentional-press gate: a tap fires only after the touch has stayed put
// for TAP_HOLD_MS (wander tolerance TAP_SLOP_PX) — a quick graze or a
// press that slides away sends nothing. No visible "hold" UI; the delay
// is short enough to feel like a normal deliberate press. Urgent items'
// separate 600ms hold-to-confirm layers on top of this (the tap fires
// while the finger is still down, and serviceUrgentHold rides the same
// continuing physical press).
void handleTouch() {
  bool isTouched = touch.touched();
  unsigned long now = millis();

  if (isTouched) {
    TS_Point p = touch.getPoint();
    int sx, sy;
    mapTouch(p.x, p.y, sx, sy);

    if (!wasTouched) {
      // touch-down edge: arm a pending tap (existing inter-tap debounce kept)
      if (now - lastTouchAccept > 180) {
        tapPending = true;
        tapFired = false;
        tapStart = now;
        tapX = sx;
        tapY = sy;
      }
    } else if (tapPending && !tapFired) {
      if (abs(sx - tapX) > TAP_SLOP_PX || abs(sy - tapY) > TAP_SLOP_PX) {
        tapPending = false;   // wandered — not an intentional press
      } else if (now - tapStart >= TAP_HOLD_MS) {
        tapFired = true;
        tapPending = false;
        lastTouchAccept = now;
#if TOUCH_DEBUG
        Serial.printf("tap held=(%d,%d) screen=%d\n", tapX, tapY, currentScreen);
#endif
        onScreenTap(tapX, tapY);   // fire at the touch-down position
      }
    }
  } else {
    tapPending = false;   // early release = no tap
    tapFired = false;
  }
  wasTouched = isTouched;
}

void mapTouch(int rawX, int rawY, int &sx, int &sy) {
  int x = rawX, y = rawY;
  if (TOUCH_SWAP_XY) { int t = x; x = y; y = t; }
  if (TOUCH_INVERT_X) x = TOUCH_RAW_XMIN + TOUCH_RAW_XMAX - x;
  if (TOUCH_INVERT_Y) y = TOUCH_RAW_YMIN + TOUCH_RAW_YMAX - y;
  sx = map(x, TOUCH_RAW_XMIN, TOUCH_RAW_XMAX, 0, SCREEN_W);
  sy = map(y, TOUCH_RAW_YMIN, TOUCH_RAW_YMAX, 0, SCREEN_H);
  sx = constrain(sx, 0, SCREEN_W - 1);
  sy = constrain(sy, 0, SCREEN_H - 1);
}

void onScreenTap(int x, int y) {
  if (y < 24 && currentScreen != SCR_INCOMING) {
    if (x > SCREEN_W - 28) { goSettings(); return; }
    if (x > SCREEN_W - 52) { goHistory(); return; }
  }

  switch (currentScreen) {
    case SCR_HOME:      tapHome(x, y);       break;
    case SCR_CATEGORY:  tapCategory(x, y);   break;
    case SCR_INCOMING:  tapIncoming(x, y);   break;
    case SCR_HISTORY:   tapHistory(x, y);    break;
    case SCR_SETTINGS:  tapSettings(x, y);   break;
    case SCR_DEVINFO:   tapDeviceInfo(x, y); break;
    case SCR_TRANSIENT: break;   // self-dismisses; taps do nothing
  }
}
