# StageLink

Button-only, no-typing comms between a DJ booth and front-of-house, on two
ESP32 "Cheap Yellow Display" (CYD) boards.

**Status:** written but not yet compiled or tested on real hardware — see
`HANDOFF.md` if you're picking this up in Claude Code to finish that part.

## Hardware
- 2x ESP32-2432S028R ("CYD"), 2.8" 320x240 resistive touch
- 2x USB cable, plus real power (a phone charger or power bank — a weak
  laptop USB port can brown out the WiFi radio mid-transmit)

## 1. Arduino IDE setup
- Boards Manager → install **esp32** by Espressif Systems
- Tools → Board → **ESP32 Dev Module**
- Tools → Partition Scheme → **Minimal SPIFFS (1.9MB APP with OTA/128KB
  SPIFFS)**. This is **required**: with the embedded fonts, boot logo,
  and the FOH web dashboard, the sketch no longer fits the default
  scheme's 1.25MB app partition ("Sketch too big"). Minimal SPIFFS's
  1.9MB app slots hold it comfortably, and its two OTA slots also keep
  the door open for SD-card self-updates (currently archived). On the
  command line the equivalent is
  `--fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs`.

## 2. Install libraries (Library Manager)
- **TFT_eSPI** — by Bodmer
- **XPT2046_Touchscreen** — by Paul Stoffregen
- **WiFiManager** — by tzapu
- **WebSockets** — by Markus Sattler (Links2004)

## 3. Configure TFT_eSPI (one-time, per computer)
Copy the contents of `TFT_eSPI_User_Setup_reference.h` (in this folder)
into `User_Setup.h` inside your TFT_eSPI library folder, overwriting
what's there:
- Windows: `Documents\Arduino\libraries\TFT_eSPI\User_Setup.h`
- Mac: `~/Documents/Arduino/libraries/TFT_eSPI/User_Setup.h`
- Linux: `~/Arduino/libraries/TFT_eSPI/User_Setup.h`

## 4. Flash both boards
1. Open `StageLink.ino` in Arduino IDE (open the whole folder, not just
   one file — the other tabs will load alongside it).
2. Near the top: `#define IS_DJ_UNIT 1` → plug in the DJ booth board,
   select its port, Upload.
3. Change to `#define IS_DJ_UNIT 0` → plug in the FOH board, select its
   port, Upload.
4. That one line is the only difference between the two boards.

## 5. First boot
Both default to **Direct Link mode** — FOH creates its own private WiFi
network and DJ joins it automatically, no venue wifi involved. Power on
FOH first, then DJ; both should show "connected" within a few seconds.

To use venue/house WiFi instead: tap the gear icon (top right) on either
unit → **Use Venue WiFi**. It restarts and either reconnects automatically
(if it's joined that network before) or opens a setup hotspot —
**StageLink-DJ-Setup** or **StageLink-FOH-Setup**. Connect your phone to
it, a setup page should pop up automatically (or browse to 192.168.4.1),
and enter the venue's WiFi name and password. Do this on both boards.

## 5b. Web dashboard (read-only)
The FOH unit serves a live status page any phone or laptop browser can
open — connection state of both units, signal strength, and the message
log with seen/unseen marks. It's view-only: nothing on it can send
messages or change settings.

- **Direct Link mode:** join the `StageLink-Link` wifi (password
  `boothtofoh`), browse to `http://192.168.4.1`
- **Venue WiFi mode:** browse to the FOH unit's IP address, shown on its
  Settings screen under CONNECTION.

## 5c. Themes
The whole look (colors, button shapes, fonts) is picked at flash time
with `#define STAGELINK_THEME` near the top of `StageLink.ino`:
0 Midnight (default) · 1 Sepia · 2 Game Boy · 3 Amber · 4 Ice ·
5 LCARS · 6 Cyberpunk · 7 Stardew. The palettes and style knobs live in
`theme.h`. Styling only — behavior is identical in every theme, and the
connection dot always means red = no network, yellow = network but no
peer, green = linked. The two units don't need to match.

## 6. Customizing the prompts
Open the `prompts.h` tab. Each category has a name, a color id
(0-2 = theme accent shades, 3 = alert-red), and a list of short message
labels — edit freely. A category can instead hold sub-categories (see
the DJ "Sound" menu for the pattern): give it `subcats` + `subcatCount`
instead of items, and each sub-category gets its own screen. Avoid the
`|` character in labels. Re-upload whichever board's prompts you changed
(DJ and FOH prompts are separate blocks in the same file, controlled by
`IS_DJ_UNIT`).

## Troubleshooting
- **Screen stays blank after upload** — open `User_Setup.h` again and
  swap `ILI9341_DRIVER` for `ST7789_DRIVER` (or vice versa), re-upload.
  CYD boards ship with either display chip depending on production batch.
- **Touches land in the wrong spot** — in `StageLink.ino`, try flipping
  `TOUCH_SWAP_XY` first, then `TOUCH_INVERT_X` / `TOUCH_INVERT_Y`.
- **The two units won't connect** — confirm both are in the same mode
  (check the Settings screen on each). In Venue WiFi mode, confirm the
  network is 2.4GHz — the ESP32 can't join 5GHz-only networks.
- **Setup portal doesn't pop up automatically** — manually connect your
  phone to the StageLink-DJ-Setup / StageLink-FOH-Setup hotspot, then
  browse to 192.168.4.1.
- **Connection keeps dropping mid-show** — switch both units to Direct
  Link mode from Settings; it doesn't depend on venue infrastructure at
  all, which makes it the more reliable option for a live set.

## Notes
- Network mode and brightness are saved and survive a power cycle. The
  message history log is in-memory only and clears on restart.
- This code targets the classic 2.8" resistive-touch CYD specifically.
  Larger/capacitive CYD variants use different display and touch
  hardware and won't work with this code as-is.
