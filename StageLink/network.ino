/*
  network.ino — gets all four units talking, in either network mode.

  FOH is always the WebSocket server (hub); DJ, Stage Manager, and Event
  Manager are always WebSocket clients of FOH — nobody else ever runs a
  server, and no unit besides FOH tracks more than one connection. Every
  message passes through FOH, which relays it to its destination (or
  displays it locally, if FOH is the destination) based on a small routing
  table (see routeFromClient below).

  Direct Link mode:
    FOH hosts its own WiFi network (192.168.4.1). The other three units
    join it directly. No venue wifi involved at all — most reliable
    option at a gig.

  Venue WiFi mode:
    All four units join the same existing network (set up once per venue
    via WiFiManager's phone-based captive portal). Since the venue
    router hands out IPs dynamically, FOH announces itself over a UDP
    broadcast every couple seconds so the others can find it.

  Wire format (plain text WebSocket frames):
    M|<srcRole>|<destRole>|<category>|<text>   a new prompt was sent
    Q|<srcRole>|<destRole>|<text>              a question (canned-reply answer)
    T|<srcRole>|<destRole>|<text>              a transient banner (thumbs-up, answers)
    A|<text>                                   "seen" acknowledgment — unchanged,
                                                always flows back over whichever single
                                                WS connection a client has to FOH; FOH
                                                looks up who to forward it to (see
                                                handleAckFromClient)
    I|<roleCode>                               client->FOH only: identify frame, sent
                                                right after connecting, defense-in-depth
                                                alongside the WS connection path (below)

  destRole is a plain role ordinal, or one of the DEST_* sentinels
  (StageLink.ino) resolved client-side by resolveDestRole() before sending —
  DEST_HOSPITALITY may still be overridden by FOH's routing table if Event
  Manager isn't connected.
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

#if !IS_FOH
  ws.onEvent(wsClientEvent);
  ws.setReconnectInterval(3000);
  if (netMode == MODE_DIRECT) {
    ws.begin(DIRECT_FOH_IP.toString().c_str(), WS_PORT, WS_PATH_BY_ROLE[UNIT_ROLE]);
    wsClientStarted = true;
  }
  // In venue mode, ws.begin() happens later once discovery finds the FOH IP.
#else
  for (int i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) {
    clientConnected[i] = false;
    clientRole[i] = ROLE_UNKNOWN;
  }
  for (int i = 0; i < RELAY_TABLE_MAX; i++) relayTable[i].active = false;
  ws.begin();
  ws.onEvent(wsServerEvent);
#endif

  if (netMode == MODE_VENUE) {
    discoveryUDP.begin(DISCOVERY_PORT);
  }
}

void connectDirectLink() {
#if !IS_FOH
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
  // Explicit max_connection=4: the 2-arg softAP() form defaults to 4 too,
  // but that was an implicit ceiling with zero headroom now that Direct
  // Link needs exactly 4 simultaneous stations (DJ, Stage Manager, Event
  // Manager) — made explicit so a future change to softAP's default
  // doesn't silently reintroduce the ceiling.
  WiFi.softAP(DIRECT_AP_SSID, DIRECT_AP_PASS, 1, 0, 4);
  drawStatusLine("Private link started");
#endif
}

void connectVenueWifi() {
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.setConfigPortalTimeout(180);

  const char* const AP_NAME_BY_ROLE[] = {
    "StageLink-DJ-Setup", "StageLink-FOH-Setup", "StageLink-StageMgr-Setup", "StageLink-EventMgr-Setup"
  };
  const char* apName = AP_NAME_BY_ROLE[UNIT_ROLE];

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
#if !IS_FOH
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
        ws.begin(peerIpVenueMode.toString().c_str(), WS_PORT, WS_PATH_BY_ROLE[UNIT_ROLE]);
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
  serviceRelayEscalation();
#endif
}

IPAddress calcBroadcastAddress(IPAddress ip, IPAddress mask) {
  IPAddress bcast;
  for (int i = 0; i < 4; i++) {
    bcast[i] = ip[i] | (~mask[i] & 0xFF);
  }
  return bcast;
}

String bytesToString(uint8_t *payload, size_t length) {
  String out = "";
  out.reserve(length + 1);
  for (size_t i = 0; i < length; i++) out += (char)payload[i];
  return out;
}

// ---------------- WebSocket event handlers ----------------
#if !IS_FOH
void wsClientEvent(WStype_t type, uint8_t *payload, size_t length) {
  switch (type) {
    case WStype_CONNECTED: {
      wsConnected = true;
      // Identify frame: tells FOH which role just connected, redundant
      // with (and defense-in-depth alongside) the WS path FOH already
      // reads off WStype_CONNECTED — see roleFromPath() on FOH's side.
      char idMsg[8];
      snprintf(idMsg, sizeof(idMsg), "I|%d", UNIT_ROLE);
      ws.sendTXT(idMsg);
      break;
    }
    case WStype_DISCONNECTED: wsConnected = false; break;
    case WStype_TEXT: {
      String msg = bytesToString(payload, length);
      if (msg.startsWith("A|")) markHistoryAcked(msg.substring(2));
      else handleIncomingWSText(msg);
      break;
    }
    default: break;
  }
}
#else
void wsServerEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length) {
  if (num >= WEBSOCKETS_SERVER_CLIENT_MAX) return;
  switch (type) {
    case WStype_CONNECTED:
      clientConnected[num] = true;
      clientRole[num] = roleFromPath(bytesToString(payload, length));
      wsConnected = true;
      break;
    case WStype_DISCONNECTED:
      clientConnected[num] = false;
      clientRole[num] = ROLE_UNKNOWN;
      wsConnected = anyClientConnected();
      break;
    case WStype_TEXT:
      routeFromClient(num, bytesToString(payload, length));
      break;
    default: break;
  }
}

uint8_t roleFromPath(const String &path) {
  for (uint8_t r = 0; r < 4; r++) if (r != ROLE_FOH && path == WS_PATH_BY_ROLE[r]) return r;
  return ROLE_UNKNOWN;
}

bool anyClientConnected() {
  for (int i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++) if (clientConnected[i]) return true;
  return false;
}

uint8_t clientNumForRole(uint8_t role) {
  for (int i = 0; i < WEBSOCKETS_SERVER_CLIENT_MAX; i++)
    if (clientConnected[i] && clientRole[i] == role) return i;
  return NO_CLIENT;
}

// FOH's routing table: every M|/Q|/T| frame from a connected client passes
// through here. Hospitality is the only category with dynamic routing
// (Event Manager if connected, else FOH, with a 60s ack-timeout escalation
// tracked in relayTable); everything else uses the sender's requested
// destRole as-is.
void routeFromClient(uint8_t num, const String &msg) {
  if (msg.startsWith("I|")) {
    uint8_t declaredRole = (uint8_t) msg.substring(2).toInt();
    if (clientRole[num] != ROLE_UNKNOWN && clientRole[num] != declaredRole) {
      Serial.printf("StageLink: role mismatch on client %d (path=%d, identify=%d) - using identify\n",
                    num, clientRole[num], declaredRole);
    }
    clientRole[num] = declaredRole;
    return;
  }

  uint8_t srcRole = clientRole[num];
  if (srcRole == ROLE_UNKNOWN) return;   // not yet identified — drop

  if (msg.startsWith("A|")) { handleAckFromClient(num, msg.substring(2)); return; }
  if (!(msg.startsWith("M|") || msg.startsWith("Q|") || msg.startsWith("T|"))) return;

  char type = msg[0];
  int p1 = 2, p2 = msg.indexOf('|', p1);
  if (p2 < 0) return;
  int p3 = msg.indexOf('|', p2 + 1);
  if (p3 < 0) return;
  uint8_t destRole = (uint8_t) msg.substring(p2 + 1, p3).toInt();
  String rest = msg.substring(p3 + 1);   // "<category>|<text>" for M|, "<text>" for Q|/T|

  String category, text;
  if (type == 'M') {
    int p4 = rest.indexOf('|');
    if (p4 < 0) return;
    category = rest.substring(0, p4);
    text = rest.substring(p4 + 1);
  } else {
    text = rest;
  }

  uint8_t finalDest = destRole;
  bool isHospitality = (type == 'M' && category == "Hospitality");
  if (isHospitality) {
    uint8_t emNum = clientNumForRole(ROLE_EVENT_MGR);
    finalDest = (emNum != NO_CLIENT) ? ROLE_EVENT_MGR : ROLE_FOH;
  }

  if (finalDest == ROLE_FOH) {
    // FOH is the destination -- display locally using the same parser a
    // client uses for a message that arrived over its own connection.
    handleIncomingWSText(msg);
    return;
  }

  uint8_t destNum = clientNumForRole(finalDest);
  if (destNum == NO_CLIENT) return;   // undeliverable — dropped, matches the
                                       // existing no-queue-when-disconnected
                                       // behavior of every send* function

  ws.sendTXT(destNum, msg.c_str());   // relay raw bytes, unmodified
  addHistory(text, category, false, false, finalDest);
  if (isHospitality) addRelayRecord(text, srcRole, finalDest);
}

void addRelayRecord(const String &text, uint8_t originalSenderRole, uint8_t relayedToRole) {
  int slot = -1;
  for (int i = 0; i < RELAY_TABLE_MAX; i++) if (!relayTable[i].active) { slot = i; break; }
  if (slot < 0) slot = 0;   // table full — reuse the oldest slot, mirrors HISTORY_MAX's bounded buffer
  relayTable[slot].active = true;
  relayTable[slot].escalated = false;
  relayTable[slot].text = text;
  relayTable[slot].originalSenderRole = originalSenderRole;
  relayTable[slot].relayedToRole = relayedToRole;
  relayTable[slot].relayedAtMillis = millis();
}

// A| from a client is either acking a message FOH sent it directly
// (existing markHistoryAcked path, unchanged) or one FOH relayed on behalf
// of another unit — in the latter case, forward the ack back to whoever
// actually sent it.
void handleAckFromClient(uint8_t num, const String &text) {
  markHistoryAcked(text);
  for (int i = 0; i < RELAY_TABLE_MAX; i++) {
    if (relayTable[i].active && relayTable[i].text == text) {
      uint8_t destNum = clientNumForRole(relayTable[i].originalSenderRole);
      if (destNum != NO_CLIENT) ws.sendTXT(destNum, ("A|" + text).c_str());
      relayTable[i].active = false;
      break;
    }
  }
}

// Escalates any Hospitality relay to Event Manager that's gone unacked for
// HOSPITALITY_ESCALATE_MS — FOH displays it locally, marked so drawIncoming
// can flag it as a timed-out fallback rather than a message meant for FOH.
void serviceRelayEscalation() {
  for (int i = 0; i < RELAY_TABLE_MAX; i++) {
    RelayRecord &r = relayTable[i];
    if (!r.active || r.escalated) continue;
    if (millis() - r.relayedAtMillis >= HOSPITALITY_ESCALATE_MS) {
      r.escalated = true;
      r.active = false;
      enqueueIncomingMessage("Hospitality", r.text, r.originalSenderRole, false, true);
    }
  }
}
#endif

// ---------------- message handling ----------------
// Parses a frame that has arrived at its final destination — either this
// unit's own WS client connection (non-FOH roles), or FOH's routing table
// deciding the message stays on FOH. destRole is ignored here: whoever
// calls this already made the routing decision.
void handleIncomingWSText(const String &msg) {
  if (msg.startsWith("M|")) {
    int p1 = 2, p2 = msg.indexOf('|', p1);
    if (p2 < 0) return;
    int p3 = msg.indexOf('|', p2 + 1);
    if (p3 < 0) return;
    uint8_t srcRole = (uint8_t) msg.substring(p1, p2).toInt();
    String rest = msg.substring(p3 + 1);
    int p4 = rest.indexOf('|');
    if (p4 < 0) return;
    String category = rest.substring(0, p4);
    String text = rest.substring(p4 + 1);
    enqueueIncomingMessage(category, text, srcRole, false, false);
  } else if (msg.startsWith("Q|")) {
    int p1 = 2, p2 = msg.indexOf('|', p1);
    if (p2 < 0) return;
    int p3 = msg.indexOf('|', p2 + 1);
    if (p3 < 0) return;
    uint8_t srcRole = (uint8_t) msg.substring(p1, p2).toInt();
    String text = msg.substring(p3 + 1);
    enqueueIncomingMessage("Question", text, srcRole, true, false);
  } else if (msg.startsWith("T|")) {
    int p1 = 2, p2 = msg.indexOf('|', p1);
    if (p2 < 0) return;
    int p3 = msg.indexOf('|', p2 + 1);
    if (p3 < 0) return;
    uint8_t srcRole = (uint8_t) msg.substring(p1, p2).toInt();
    String t = msg.substring(p3 + 1);
    // transient: brief self-dismissing banner, no Seen/ack — logged
    // pre-acked so it never reads as an unseen message
    if (t != THUMBS_WIRE_TEXT) {
      // an answer transient doubles as the outstanding question's ack
      for (int i = 0; i < historyCount; i++) {
        if (!history[i].incoming && !history[i].acked && history[i].category == "Question") {
          history[i].acked = true;
          break;
        }
      }
    }
    transientSenderRole = srcRole;
    addHistory(t, "", true, true, srcRole);
    showTransient(t);
  }
  // A| is handled by each side's own event handler (wsClientEvent /
  // handleAckFromClient), never routed through here.
}

// destRole as stored on a PromptCategory may be a DEST_* sentinel that
// still needs resolving to a concrete role before it goes on the wire.
uint8_t resolveDestRole(uint8_t catDestRole) {
  if (catDestRole == DEST_REPLY_TO_SENDER) return lastIncomingSenderRole;
  if (catDestRole == DEST_HOSPITALITY) return ROLE_EVENT_MGR;   // FOH applies the connected/fallback override
  return catDestRole;
}

void sendPromptMessage(const String &category, const String &text, uint8_t destRole) {
  String out = "M|" + String(UNIT_ROLE) + "|" + String(destRole) + "|" + category + "|" + text;
#if IS_FOH
  uint8_t destNum = clientNumForRole(destRole);
  if (destNum != NO_CLIENT) ws.sendTXT(destNum, out.c_str());
  addHistory(text, category, false, false, destRole);
#else
  if (wsConnected) ws.sendTXT(out.c_str());
  addHistory(text, category, false, false, ROLE_FOH);
#endif
  lastSentText = text;
}

// sendAck always answers whichever message is CURRENTLY on screen: a
// non-FOH client only ever has one WS connection (to FOH) to send it over,
// same as before; FOH routes it to incomingSenderRole's client, which is
// correct whether that message was addressed to FOH directly or is an
// escalated Hospitality relay.
void sendAck(const String &text) {
  String out = "A|" + text;
#if IS_FOH
  uint8_t destNum = clientNumForRole(incomingSenderRole);
  if (destNum != NO_CLIENT) ws.sendTXT(destNum, out.c_str());
#else
  if (wsConnected) ws.sendTXT(out.c_str());
#endif
}

// Question (Q|): the peer answers with canned-reply buttons; the answer
// comes back as a transient, so no A| ack is expected — don't arm the
// green-check tracker for these.
void sendQuestion(const String &text, uint8_t destRole) {
  String out = "Q|" + String(UNIT_ROLE) + "|" + String(destRole) + "|" + text;
#if IS_FOH
  uint8_t destNum = clientNumForRole(destRole);
  if (destNum != NO_CLIENT) ws.sendTXT(destNum, out.c_str());
  addHistory(text, "Question", false, false, destRole);
#else
  if (wsConnected) ws.sendTXT(out.c_str());
  addHistory(text, "Question", false, false, ROLE_FOH);
#endif
  lastSentText = text;
}

// Transient (T|): brief self-dismissing banner on the peer — no Seen, no
// ack ever, so it logs pre-acked on the sender too.
void sendTransient(const String &text, uint8_t destRole) {
  String out = "T|" + String(UNIT_ROLE) + "|" + String(destRole) + "|" + text;
#if IS_FOH
  uint8_t destNum = clientNumForRole(destRole);
  if (destNum != NO_CLIENT) ws.sendTXT(destNum, out.c_str());
  addHistory(text, "", false, true, destRole);
#else
  if (wsConnected) ws.sendTXT(out.c_str());
  addHistory(text, "", false, true, ROLE_FOH);
#endif
  lastSentText = text;
}
