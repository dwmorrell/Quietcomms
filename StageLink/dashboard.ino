/*
  dashboard.ino — read-only web dashboard served by the FOH unit, so a
  third party (tour manager, venue tech) can watch the system from any
  phone/laptop browser on the same network. View-only by design: it
  exposes status and the message log, but no way to send or change
  anything. (An admin-messaging form was considered and deliberately
  deferred — see the project archive notes.)

  Reaching it:
    Direct Link mode — join the StageLink-Link wifi, browse to
    http://192.168.4.1
    Venue WiFi mode — browse to the FOH unit's IP, shown on its
    Settings screen under CONNECTION.

  The DJ unit compiles empty stubs: FOH is the hub, so it's the one
  place with a full picture (its own state plus whether the DJ unit is
  linked and, in Direct Link mode, the DJ's radio signal strength).
*/

#if !IS_DJ_UNIT

#include <WebServer.h>
WebServer dashboard(80);

// Kept lean and dependency-free: dark, monospace, auto-refreshing off a
// small JSON endpoint every 2.5s.
const char DASHBOARD_HTML[] PROGMEM = R"rawliteral(<!DOCTYPE html><html><head><meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>StageLink FOH</title>
<style>
 body{background:#0a0a0a;color:#eee;font-family:'Courier New',monospace;margin:0;padding:16px}
 h1{font-size:20px;letter-spacing:3px;margin:0 0 14px}
 .cards{display:flex;flex-wrap:wrap;gap:10px}
 .card{background:#1c1c1c;border:1px solid #444;border-radius:8px;padding:10px 14px;min-width:130px}
 .card .k{color:#999;font-size:11px;text-transform:uppercase;letter-spacing:1px}
 .card .v{font-size:17px;margin-top:4px}
 .ok{color:#6fdc8c}.bad{color:#e05555}
 table{width:100%;border-collapse:collapse;margin-top:18px;font-size:13px}
 th,td{padding:6px 8px;text-align:left;border-bottom:1px solid #2a2a2a}
 th{color:#999;text-transform:uppercase;font-size:11px;letter-spacing:1px}
 .in{color:#e0c04e}.out{color:#7fb2e5}.seen{color:#6fdc8c}
 .muted{color:#666;margin-top:14px;font-size:11px}
</style></head><body>
<h1>STAGELINK &middot; FOH</h1>
<div class="cards" id="cards"></div>
<table><thead><tr><th>Dir</th><th>Category</th><th>Message</th><th>Age</th><th>Seen</th></tr></thead><tbody id="log"></tbody></table>
<div class="muted">Read-only view. Newest messages first. Auto-refreshes every 2.5s.</div>
<script>
async function tick(){
 try{
  const r=await fetch('/status.json');const s=await r.json();
  const c=[["Link",s.link?"Connected":"Waiting",s.link?"ok":"bad"],
   ["Mode",s.mode,""],["WiFi",s.ssid,""],["IP",s.ip,""],
   [s.rssiLabel,s.rssi,""],["DJ radio",s.djRssi,""],
   ["Uptime",s.uptime,""],["Firmware",s.fw,""],["Free RAM",s.heap,""]];
  document.getElementById('cards').innerHTML=c.map(x=>`<div class="card"><div class="k">${x[0]}</div><div class="v ${x[2]}">${x[1]}</div></div>`).join('');
  document.getElementById('log').innerHTML=s.log.map(m=>`<tr><td class="${m.in?'in':'out'}">${m.in?'IN':'OUT'}</td><td>${m.cat}</td><td>${m.text}</td><td>${m.age}</td><td class="seen">${m.ack?'&#10003;':''}</td></tr>`).join('');
 }catch(e){}
 setTimeout(tick,2500);
}
tick();
</script></body></html>)rawliteral";

String jsonEscape(const String &s) {
  String out;
  out.reserve(s.length() + 4);
  for (unsigned int i = 0; i < s.length(); i++) {
    char c = s[i];
    if (c == '"' || c == '\\') { out += '\\'; out += c; }
    else if (c >= 32) out += c;
  }
  return out;
}

void handleDashboardStatus() {
  String ssid, ip, rssiLabel, rssiVal, djRssi = "-";
  if (netMode == MODE_DIRECT) {
    ssid = DIRECT_AP_SSID;
    ip = WiFi.softAPIP().toString();
    rssiLabel = "Devices";
    rssiVal = String(WiFi.softAPgetStationNum()) + " joined";
    wifi_sta_list_t staList;
    if (esp_wifi_ap_get_sta_list(&staList) == ESP_OK && staList.num > 0) {
      djRssi = String(staList.sta[0].rssi) + " dBm";
    }
  } else {
    ssid = WiFi.SSID();
    ip = WiFi.localIP().toString();
    rssiLabel = "Signal";
    rssiVal = (WiFi.status() == WL_CONNECTED) ? String(WiFi.RSSI()) + " dBm" : "-";
    djRssi = wsConnected ? "linked" : "-";
  }
  if (ssid.length() == 0) ssid = "-";
  unsigned long up = millis() / 1000;
  String uptime = String(up / 3600) + "h " + String((up % 3600) / 60) + "m";

  String json = "{";
  json += "\"link\":" + String(wsConnected ? "true" : "false") + ",";
  json += "\"mode\":\"" + String(netMode == MODE_DIRECT ? "Direct Link" : "Venue WiFi") + "\",";
  json += "\"ssid\":\"" + jsonEscape(ssid) + "\",";
  json += "\"ip\":\"" + ip + "\",";
  json += "\"rssiLabel\":\"" + rssiLabel + "\",";
  json += "\"rssi\":\"" + rssiVal + "\",";
  json += "\"djRssi\":\"" + djRssi + "\",";
  json += "\"uptime\":\"" + uptime + "\",";
  json += "\"fw\":\"" FW_VERSION "\",";
  json += "\"heap\":\"" + String(ESP.getFreeHeap() / 1024) + " KB\",";
  json += "\"log\":[";
  for (int i = 0; i < historyCount; i++) {
    if (i) json += ",";
    json += "{\"in\":" + String(history[i].incoming ? "true" : "false");
    json += ",\"cat\":\"" + jsonEscape(history[i].category) + "\"";
    json += ",\"text\":\"" + jsonEscape(history[i].text) + "\"";
    json += ",\"age\":\"" + relativeTime(history[i].atMillis) + "\"";
    json += ",\"ack\":" + String(history[i].acked ? "true" : "false") + "}";
  }
  json += "]}";
  dashboard.send(200, "application/json", json);
}

void startDashboard() {
  dashboard.on("/", []() { dashboard.send_P(200, "text/html", DASHBOARD_HTML); });
  dashboard.on("/status.json", handleDashboardStatus);
  dashboard.begin();
}

void serviceDashboard() {
  dashboard.handleClient();
}

#else
// DJ unit: no dashboard — FOH is the hub with the full picture.
void startDashboard() {}
void serviceDashboard() {}
#endif
