/*
  settings.ino — the settings screen (network mode, connection details,
  brightness) and the device-info screen it links to.

  Deliberately plain compared to the rest of the UI: built-in font only,
  flat rows and buttons, no stylized/dithered button chrome — these
  screens are informational rather than a performance surface, so
  clarity wins over the retro styling.

  Mode switches restart the board on purpose. ESP32 WiFi doesn't
  reliably flip between AP and STA at runtime, so a clean reboot into
  the new saved mode is far more predictable than trying to tear down
  and rebuild the WiFi stack live during a show.
*/

// Checkbox-style selection row: 12px square at left, filled when
// selected, with a panel-gray row highlight behind the selected option.
void settingsOptionRow(Rect r, const String &label, bool selected) {
  if (selected) tft.fillRect(r.x, r.y, r.w, r.h, COL_PANEL);
  uint16_t rowBg = selected ? COL_PANEL : COL_BG;
  int boxY = r.y + (r.h - 12) / 2;
  tft.drawRect(r.x + 6, boxY, 12, 12, selected ? COL_TEXT : COL_TEXT_DIM);
  if (selected) tft.fillRect(r.x + 9, boxY + 3, 6, 6, COL_TEXT);
  tft.setTextColor(selected ? COL_TEXT : COL_TEXT_DIM, rowBg);
  tft.setTextDatum(ML_DATUM);
  tft.setTextFont(2);
  tft.drawString(label, r.x + 26, r.y + r.h / 2);
}

// Plain bordered button, built-in font, no fill/dither/rounding.
void settingsFlatButton(Rect r, const String &label) {
  tft.drawRect(r.x, r.y, r.w, r.h, COL_BORDER);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setTextDatum(MC_DATUM);
  tft.setTextFont(2);
  tft.drawString(label, r.x + r.w / 2, r.y + r.h / 2);
}

// "Key:   value" info line — dim key, bright value.
void settingsInfoLine(const String &key, const String &value, int y) {
  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(COL_TEXT_DIM, COL_BG);
  tft.drawString(key, 12, y);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString(value, 80, y);
}

void drawSettings() {
  tft.fillScreen(COL_BG);
  drawStatusBar();

  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString("Settings", 12, 30);

  // --- network mode selection ---
  tft.setTextColor(COL_TEXT_DIM, COL_BG);
  tft.drawString("NETWORK", 12, 50);
  Rect rowDirect = {10, 62, SCREEN_W - 20, 22};
  Rect rowVenue  = {10, 86, SCREEN_W - 20, 22};
  Rect rowReconf = {10, 110, SCREEN_W - 20, 22};
  settingsOptionRow(rowDirect, "Direct Link", netMode == MODE_DIRECT);
  settingsOptionRow(rowVenue,
                    venueConfigured ? "Venue WiFi" : "Venue WiFi (not set up)",
                    netMode == MODE_VENUE);
  settingsFlatButton(rowReconf, "Reconfigure venue WiFi");

  // --- live connection properties ---
  tft.setTextColor(COL_TEXT_DIM, COL_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("CONNECTION", 12, 138);

  String ssid, ip;
  bool isStation = true;
#if IS_DJ_UNIT
  ssid = WiFi.SSID();
  ip = WiFi.localIP().toString();
#else
  if (netMode == MODE_DIRECT) {
    // FOH hosts the AP in Direct Link mode — report the AP's own details.
    isStation = false;
    ssid = DIRECT_AP_SSID;
    ip = WiFi.softAPIP().toString();
  } else {
    ssid = WiFi.SSID();
    ip = WiFi.localIP().toString();
  }
#endif
  if (ssid.length() == 0) ssid = "-";

  settingsInfoLine("Link", wsConnected ? "Connected" : "Waiting...", 154);
  settingsInfoLine("WiFi", ssid, 170);
  settingsInfoLine("IP", ip, 186);
  if (isStation) {
    settingsInfoLine("Signal",
                     WiFi.status() == WL_CONNECTED ? String(WiFi.RSSI()) + " dBm" : "-",
                     202);
  } else {
    settingsInfoLine("Devices", String(WiFi.softAPgetStationNum()) + " joined", 202);
  }

  // --- device info entry ---
  Rect infoBtn = {10, 220, SCREEN_W - 20, 22};
  settingsFlatButton(infoBtn, "Device Info");

  // --- brightness ---
  tft.setTextColor(COL_TEXT_DIM, COL_BG);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("BRIGHTNESS", 12, 248);
  Rect minusBtn = {10, 260, 44, 26};
  Rect plusBtn  = {130, 260, 44, 26};
  settingsFlatButton(minusBtn, "-");
  settingsFlatButton(plusBtn, "+");
  tft.setTextDatum(MC_DATUM);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.setTextFont(2);
  tft.drawString(String(backlightLevel) + "%", 92, 273);

  Rect backBtn = {10, 290, SCREEN_W - 20, 26};
  settingsFlatButton(backBtn, "Back");
}

void tapSettings(int x, int y) {
  Rect rowDirect = {10, 62, SCREEN_W - 20, 22};
  Rect rowVenue  = {10, 86, SCREEN_W - 20, 22};
  Rect rowReconf = {10, 110, SCREEN_W - 20, 22};
  Rect infoBtn   = {10, 220, SCREEN_W - 20, 22};
  Rect minusBtn  = {10, 260, 44, 26};
  Rect plusBtn   = {130, 260, 44, 26};
  Rect backBtn   = {10, 290, SCREEN_W - 20, 26};

  if (pointInRect(x, y, rowDirect)) {
    flashPress(rowDirect);
    actionSetMode(MODE_DIRECT);
    drawSettings();  // only reached when already in this mode (no restart)
    return;
  }
  if (pointInRect(x, y, rowVenue)) {
    flashPress(rowVenue);
    // Never successfully joined a venue network before — going straight to
    // MODE_VENUE would just autoConnect() blind, silently fail, and fall
    // into WiFiManager's own setup portal anyway. Skip straight there.
    if (venueConfigured) actionSetMode(MODE_VENUE);
    else actionReconfigureVenueWifi();
    drawSettings();  // only reached when already in venue mode (no restart)
    return;
  }
  if (pointInRect(x, y, rowReconf)) { flashPress(rowReconf); actionReconfigureVenueWifi(); return; }

  if (pointInRect(x, y, infoBtn)) {
    flashPress(infoBtn);
    currentScreen = SCR_DEVINFO;
    drawDeviceInfo();
    return;
  }

  if (pointInRect(x, y, minusBtn)) {
    flashPress(minusBtn);
    backlightLevel = (backlightLevel < 20) ? 10 : backlightLevel - 10;
    setBacklight(backlightLevel);
    prefs.putUChar("backlight", backlightLevel);
    drawSettings();
    return;
  }
  if (pointInRect(x, y, plusBtn)) {
    flashPress(plusBtn);
    backlightLevel = (backlightLevel + 10 > 100) ? 100 : backlightLevel + 10;
    setBacklight(backlightLevel);
    prefs.putUChar("backlight", backlightLevel);
    drawSettings();
    return;
  }
  if (pointInRect(x, y, backBtn)) { flashPress(backBtn); currentScreen = SCR_HOME; drawHome(); return; }
}

// ---------------- device info screen ----------------
// One-glance hardware/firmware rundown so a board can be identified and
// sanity-checked in the field without plugging into a PC.
void drawDeviceInfo() {
  tft.fillScreen(COL_BG);
  drawStatusBar();

  tft.setTextDatum(TL_DATUM);
  tft.setTextFont(2);
  tft.setTextColor(COL_TEXT, COL_BG);
  tft.drawString("Device Info", 12, 30);

  // Efuse MAC doubles as the board's factory serial number.
  uint64_t mac = ESP.getEfuseMac();
  char serial[13];
  snprintf(serial, sizeof(serial), "%04X%08X",
           (uint16_t)(mac >> 32), (uint32_t)mac);

  unsigned long up = millis() / 1000;
  String uptime = String(up / 3600) + "h " + String((up % 3600) / 60) + "m " + String(up % 60) + "s";

  int y = 54;
  settingsInfoLine("Role",     IS_DJ_UNIT ? "DJ unit" : "FOH unit", y); y += 18;
  settingsInfoLine("Firmware", FW_VERSION, y); y += 18;
  settingsInfoLine("Built",    String(__DATE__) + " " + String(__TIME__), y); y += 18;
  settingsInfoLine("Model",    String(ESP.getChipModel()) + " r" + String(ESP.getChipRevision()), y); y += 18;
  settingsInfoLine("Serial",   String(serial), y); y += 18;
  settingsInfoLine("MAC",      WiFi.macAddress(), y); y += 18;
  settingsInfoLine("CPU",      String(getCpuFrequencyMhz()) + " MHz", y); y += 18;
  settingsInfoLine("Flash",    String(ESP.getFlashChipSize() / (1024 * 1024)) + " MB", y); y += 18;
  settingsInfoLine("Free RAM", String(ESP.getFreeHeap() / 1024) + " KB", y); y += 18;
  settingsInfoLine("SDK",      String(ESP.getSdkVersion()), y); y += 18;
  settingsInfoLine("Uptime",   uptime, y);

  Rect backBtn = {10, SCREEN_H - 34, SCREEN_W - 20, 28};
  settingsFlatButton(backBtn, "Back");
}

void tapDeviceInfo(int x, int y) {
  Rect backBtn = {10, SCREEN_H - 34, SCREEN_W - 20, 28};
  if (pointInRect(x, y, backBtn)) {
    flashPress(backBtn);
    currentScreen = SCR_SETTINGS;
    drawSettings();
  }
}

void actionSetMode(NetMode m) {
  if (m == netMode) return;
  prefs.putUChar("netMode", (uint8_t)m);
  drawStatusLine("Restarting...");
  delay(400);
  ESP.restart();
}

void actionReconfigureVenueWifi() {
  prefs.putBool("forcePortal", true);
  prefs.putUChar("netMode", MODE_VENUE);
  drawStatusLine("Restarting...");
  delay(400);
  ESP.restart();
}
