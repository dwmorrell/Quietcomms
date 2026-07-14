# Archived: GrandMA2 lighting console bridge

Relays every DJ message to a GrandMA2 console as an on-screen chat line
(via the console's telnet remote — MA-Net itself is proprietary). Built
and flashed 2026-07-08, pulled from the active build 2026-07-14 during
the UAT rework: Davis wants console integration eventually but believes
there's an easier approach, so this is preserved intact for reference.

**Status when archived:** compiled clean and flashed to the FOH board,
but never verified against the real desk — see the caveat below.

## Connection facts (Davis's desk, confirmed 2026-07-08)

- Console IP: **192.168.2.30** (Davis confirmed `.168`, NOT the `.169`
  typo that appeared once), telnet port **30000**
- Console user for the bridge: **STAGELINK**, no password
- The console lives on the wifi network **SSID `MA2`, password
  `purplebox485`** — the FOH unit reaches it by pointing its venue wifi
  at that network (Settings > Reconfigure venue WiFi). The DJ unit must
  join the same network too, or no DJ→FOH messages flow at all.
- Per-message telnet session:
  ```
  login "STAGELINK"
  chat "<Category>: <message text>"
  logout
  ```
- Console-side prep (done on Davis's desk): STAGELINK user created,
  telnet remote enabled (Setup → Console → Global Settings → Remotes).

**Untested caveat:** the passwordless login syntax (`login "STAGELINK"`)
was never verified against a real console. If the desk rejects it, try
unquoted `login STAGELINK` or `login "STAGELINK" ""` — the desk's system
monitor shows exactly what arrived.

## Design notes

- FOH-only (DJ compiles empty stubs); gated to Venue WiFi mode — in
  Direct Link mode messages are skipped silently.
- Millis-based state machine (`serviceMa2Bridge()` from `loop()`); the
  only blocking step is the TCP connect, capped at 400ms, so a missing
  console can't stall the show UI. Unreachable console = queue dropped,
  DJ↔FOH messaging unaffected.
- 4-slot message queue; `ma2Sanitize()` strips `"` and `\` from labels.

## To re-integrate

1. Move `ma2bridge.ino.txt` back to `StageLink/ma2bridge.ino`.
2. In `StageLink.ino`: restore the constants block in section 6
   (NETWORK CONSTANTS):
   ```cpp
   // GrandMA2 bridge (FOH only, ma2bridge.ino): DJ messages are relayed to
   // the lighting console as on-screen chat messages via its telnet remote.
   // Only active in Venue WiFi mode.
   #define MA2_ENABLED     1
   const IPAddress MA2_CONSOLE_IP(192, 168, 2, 30);
   #define MA2_TELNET_PORT 30000
   #define MA2_LOGIN_USER  "STAGELINK"
   ```
   and add `serviceMa2Bridge();` to `loop()`.
3. In `network.ino`, in `handleIncomingWSText()`'s `M|` branch after
   `showIncomingOverlay();`, add:
   ```cpp
   ma2QueueMessage(incomingCategory, incomingText);
   ```
4. Restore the "GrandMA2 lighting console bridge" section to README.md
   (removed section 5c) describing the console-side setup above.
5. Compile both roles with
   `--fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs` and reflash the
   FOH board (DJ board only carries empty stubs; reflash it too if you
   want the builds matched).
