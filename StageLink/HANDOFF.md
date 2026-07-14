# Handoff brief: StageLink firmware

Paste this whole file as your first message to Claude Code, in a folder
containing this StageLink project (unzip it there first).

---

I'm continuing an ESP32 firmware project called StageLink: a 2-way,
button-only comms tool so a DJ on stage and their front-of-house (FOH)
engineer can message each other during a live set without either person
typing anything — tap a category, tap a preset message, it sends.

**Hardware:** 2x ESP32-2432S028R "Cheap Yellow Display" boards (2.8"
320x240 resistive touch). One shared sketch gets flashed to both boards;
`#define IS_DJ_UNIT` (1 or 0, top of `StageLink.ino`) is the only thing
that differs between them.

**Architecture already decided — please keep these, don't redesign:**
- Network mode is switchable at runtime from an on-device Settings
  screen: **Direct Link** (FOH hosts its own WiFi AP at 192.168.4.1, DJ
  joins it directly — zero dependency on venue wifi) or **Venue WiFi**
  (both join an existing network via WiFiManager's phone-based captive
  portal; FOH announces its IP over a UDP broadcast on port 4210 every 2s
  so DJ can find it, since venue routers hand out dynamic IPs).
- FOH always runs the WebSocket **server** (port 81); DJ always runs the
  WebSocket **client**. True in both network modes — this is what keeps
  the two roles from needing symmetric client+server code.
- Libraries: TFT_eSPI (Bodmer), XPT2046_Touchscreen (Paul Stoffregen),
  WiFiManager (tzapu), WebSockets (Markus Sattler / Links2004). Not
  ESPAsyncWebServer — deliberately dropped in favor of one library that
  covers both client and server roles.
- Dark theme throughout (near-black background, muted accent colors,
  PWM-dimmable backlight) since this sits in a dark venue. Incoming
  messages flash an alert color and pulse the onboard RGB LED rather
  than using a jarring full-white flash.
- Wire protocol, plain text WebSocket frames: `M|<category>|<message>`
  for a new prompt, `A|<message>` for a "seen" acknowledgment.
- Prompt content (the actual DJ→FOH / FOH→DJ message categories) lives
  in `prompts.h` and is meant to be freely user-editable — don't restructure
  that data shape without a good reason.

**Current state:** All five files are written (`StageLink.ino`,
`network.ino`, `display_ui.ino`, `settings.ino`, `prompts.h`), based on
web research into correct library APIs and pin mappings rather than a
real compiler — I built this without network/library access, so it has
never actually been compiled. Treat it as a solid first draft, not
verified-working code.

**What I need from you:**
1. Set up an ESP32 Arduino build (arduino-cli if you have it available)
   and install the esp32 board package plus the four libraries above.
2. Apply `TFT_eSPI_User_Setup_reference.h` into the installed TFT_eSPI
   library's `User_Setup.h` (instructions are in `README.md`).
3. Compile for both `IS_DJ_UNIT 1` and `IS_DJ_UNIT 0`, and fix whatever
   errors turn up — there are likely a few, since none of this has hit a
   real compiler yet.
4. Once it compiles, double-check the WebSockets, WiFiManager, and
   XPT2046_Touchscreen API calls against the actual installed library
   source (not just what I inferred from search results), and correct
   anything that's off.
5. Flag anything in the architecture notes above that turns out to be
   wrong or impractical once you're actually working with the libraries
   — I'd rather hear about it than have it silently patched around.
6. Update `README.md` if setup steps change as a result of your fixes.

I can't test on real hardware from where I'm working either, so once it
compiles cleanly, the next real test is flashing actual boards — flag
anything you're unsure will work correctly on physical hardware (touch
calibration and screen driver chip especially — both have known
board-to-board variation on this hardware, and are called out in the
README's troubleshooting section).
