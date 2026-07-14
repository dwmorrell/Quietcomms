/*
  network.ino — gets the two units talking, in either network mode.

  Direct Link mode:
    FOH hosts its own WiFi network (192.168.4.1). DJ joins it directly.
    No venue wifi involved at all — most reliable option at a gig.

  Venue WiFi mode:
    Both units join the same existing network (set up once per venue
    via WiFiManager's phone-based captive portal). Since the venue
    router hands out IPs dynamically, FOH announces itself over a UDP
    broadcast every couple seconds so the DJ unit can find it.

  Wire format (plain text WebSocket frames), either direction:
    M|<category>|<message text>     a new prompt was sent
    A|<message text>                "seen" acknowledgment
*/

unsigned long lastDiscoveryBroadcast = 0;
IPAddress peerIpVenueMode;
bool peerIpKnown = false;

// ---------------- startup ----------------
void startNetworking() {
  if (netMode == MODE_DIRECT) {
    connectDirectLink();
  } else {
    connectVenueWifi();
  }

#if IS_DJ_UNIT
  ws.onEvent(wsClientEvent);
  ws.setReconnectInterval(3000);
  if (netMode == MODE_DIRECT) {
    ws.begin(DIRECT_FOH_IP.toString().c_str(), WS_PORT, "/");
    wsClientStarted = true;
  }
  // In venue mode, ws.begin() happens later once discovery finds the FOH IP.
#else
  ws.begin();
  ws.onEvent(wsServerEvent);
#endif

  if (netMode == MODE_VENUE) {
    discoveryUDP.begin(DISCOVERY_PORT);
  }
}

void connectDirectLink() {
#if IS_DJ_UNIT
  WiFi.mode(WIFI_STA);
  WiFi.begin(DIRECT_AP_SSID, DIRECT_AP_PASS);
  drawStatusLine("Joining private link...");
  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < 15000) {
    reelSpinTick();   // keep the boot reel turning while we wait
    delay(10);
  }
#else
  WiFi.mode(WIFI_AP);
  WiFi.softAP(DIRECT_AP_SSID, DIRECT_AP_PASS);
  drawStatusLine("Private link started");
#endif
}

void connectVenueWifi() {
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);

  const char* apName = IS_DJ_UNIT ? "StageLink-DJ-Setup" : "StageLink-FOH-Setup";

  // autoConnect() silently falls back to opening its own portal (same as
  // startConfigPortal below) whenever it can't join a known network, with
  // no built-in warning — without this callback the screen is stuck on
  // "Connecting to venue wifi..." while the board is actually waiting on
  // a phone to join its setup hotspot, which looks like a hang.
  wm.setAPCallback([apName](WiFiManager *wmPtr) {
    drawStatusLine(String("Connect phone to ") + apName);
  });

  bool forcePortal = prefs.getBool("forcePortal", false);
  if (forcePortal) prefs.putBool("forcePortal", false);

  bool ok;
  if (forcePortal) {
    drawStatusLine("Opening wifi setup...");
    ok = wm.startConfigPortal(apName);
  } else {
    drawStatusLine("Connecting to venue wifi...");
    ok = wm.autoConnect(apName);
  }

  if (ok) {
    venueConfigured = true;
    prefs.putBool("venueConfigured", true);
  }
  drawStatusLine(ok ? "Venue wifi connected" : "Wifi setup timed out");
}

// ---------------- per-loop servicing ----------------
void serviceNetworking() {
#if IS_DJ_UNIT
  if (wsClientStarted) ws.loop();

  if (netMode == MODE_VENUE && !peerIpKnown) {
    int packetSize = discoveryUDP.parsePacket();
    if (packetSize > 0) {
      char buf[32];
      int len = discoveryUDP.read(buf, sizeof(buf) - 1);
      if (len > 0) buf[len] = 0; else buf[0] = 0;
      if (strncmp(buf, "STAGELINK_FOH", 13) == 0) {
        peerIpVenueMode = discoveryUDP.remoteIP();
        peerIpKnown = true;
        ws.begin(peerIpVenueMode.toString().c_str(), WS_PORT, "/");
        wsClientStarted = true;
      }
    }
  }
#else
  ws.loop();
  if (netMode == MODE_VENUE && millis() - lastDiscoveryBroadcast > 2000) {
    lastDiscoveryBroadcast = millis();
    if (WiFi.status() == WL_CONNECTED) {
      IPAddress bcast = calcBroadcastAddress(WiFi.localIP(), WiFi.subnetMask());
      discoveryUDP.beginPacket(bcast, DISCOVERY_PORT);
      discoveryUDP.print("STAGELINK_FOH");
      discoveryUDP.endPacket();
    }
  }
#endif
}

IPAddress calcBroadcastAddress(IPAddress ip, IPAddress mask) {
  IPAddress bcast;
  for (int i = 0; i < 4; i++) {
    bcast[i] = ip[i] | (~mask[i] & 0xFF);
  }
  return bcast;
}

// ---------------- WebSocket event handlers ----------------
#if IS_DJ_UNIT
void wsClientEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:    wsConnected = true;  break;
    case WStype_DISCONNECTED: wsConnected = false; break;
    case WStype_TEXT:         handleIncomingWSText(bytesToString(payload, length)); break;
    default: break;
  }
}
#else
void wsServerEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED:
      wsConnected = true;
      peerClientNum = num;
      peerClientKnown = true;
      break;
    case WStype_DISCONNECTED:
      if (peerClientKnown && num == peerClientNum) {
        wsConnected = false;
        peerClientKnown = false;
      }
      break;
    case WStype_TEXT:
      handleIncomingWSText(bytesToString(payload, length));
      break;
    default: break;
  }
}
#endif

String bytesToString(uint8_t *payload, size_t length) {
  String out = "";
  out.reserve(length + 1);
  for (size_t i = 0; i < length; i++) out += (char)payload[i];
  return out;
}

// ---------------- message handling ----------------
void handleIncomingWSText(const String &msg) {
  if (msg.startsWith("M|")) {
    int secondBar = msg.indexOf('|', 2);
    if (secondBar > 0) {
      incomingCategory = msg.substring(2, secondBar);
      incomingText = msg.substring(secondBar + 1);
      incomingIsQuestion = false;
      addHistory(incomingText, incomingCategory, true, false);
      showIncomingOverlay();
    }
  } else if (msg.startsWith("Q|")) {
    // question: answered via canned-reply buttons instead of Seen
    incomingCategory = "Question";
    incomingText = msg.substring(2);
    incomingIsQuestion = true;
    addHistory(incomingText, incomingCategory, true, false);
    showIncomingOverlay();
  } else if (msg.startsWith("T|")) {
    // transient: brief self-dismissing banner, no Seen/ack — logged
    // pre-acked so it never reads as an unseen message
    String t = msg.substring(2);
    if (t != THUMBS_WIRE_TEXT) {
      // an answer transient doubles as the outstanding question's ack
      for (int i = 0; i < historyCount; i++) {
        if (!history[i].incoming && !history[i].acked && history[i].category == "Question") {
          history[i].acked = true;
          break;
        }
      }
    }
    addHistory(t, "", true, true);
    showTransient(t);
  } else if (msg.startsWith("A|")) {
    markHistoryAcked(msg.substring(2));
  }
}

void sendPromptMessage(const String &category, const String &text) {
  String out = "M|" + category + "|" + text;
#if IS_DJ_UNIT
  if (wsConnected) ws.sendTXT(out.c_str());
#else
  if (wsConnected && peerClientKnown) ws.sendTXT(peerClientNum, out.c_str());
#endif
  lastSentText = text;
  addHistory(text, category, false, false);
}

void sendAck(const String &text) {
  String out = "A|" + text;
#if IS_DJ_UNIT
  if (wsConnected) ws.sendTXT(out.c_str());
#else
  if (wsConnected && peerClientKnown) ws.sendTXT(peerClientNum, out.c_str());
#endif
}

// Question (Q|): the peer answers with canned-reply buttons; the answer
// comes back as a transient, so no A| ack is expected — don't arm the
// green-check tracker for these.
void sendQuestion(const String &text) {
  String out = "Q|" + text;
#if IS_DJ_UNIT
  if (wsConnected) ws.sendTXT(out.c_str());
#else
  if (wsConnected && peerClientKnown) ws.sendTXT(peerClientNum, out.c_str());
#endif
  lastSentText = text;
  addHistory(text, "Question", false, false);
}

// Transient (T|): brief self-dismissing banner on the peer — no Seen, no
// ack ever, so it logs pre-acked on the sender too.
void sendTransient(const String &text) {
  String out = "T|" + text;
#if IS_DJ_UNIT
  if (wsConnected) ws.sendTXT(out.c_str());
#else
  if (wsConnected && peerClientKnown) ws.sendTXT(peerClientNum, out.c_str());
#endif
  lastSentText = text;
  addHistory(text, "", false, true);
}
