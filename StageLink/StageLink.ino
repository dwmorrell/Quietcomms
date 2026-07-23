/*
  ============================================================
  StageLink — DJ <-> FOH button comms over WiFi
  Board:  ESP32-2432S028R "Cheap Yellow Display" (2.8", resistive touch)
  ============================================================

  ONE SKETCH, FOUR BOARDS:
  Set UNIT_ROLE below, upload to that unit's board. FOH is always the
  WebSocket server (hub); DJ, Stage Manager, and Event Manager are all
  WebSocket clients of FOH. That one line is the only difference
  between what gets flashed to each of the four units.

  Before this will compile you need to install 4 libraries and
  configure TFT_eSPI once. Full steps are in README.md.
*/

// ---------------- 1. ROLE ----------------
// Plain #defines, not a C++ enum: these gate #if blocks throughout the
// sketch (library includes, server-vs-client object declarations), and
// the preprocessor can't see enum values — only macros expand before it
// runs.
#define ROLE_DJ         0
#define ROLE_FOH        1
#define ROLE_STAGE_MGR  2
#define ROLE_EVENT_MGR  3
#define ROLE_UNKNOWN    255   // WS server client slot not yet identified
#define NO_CLIENT       255   // clientNumForRole() found no connected client for that role

#define UNIT_ROLE ROLE_EVENT_MGR   // set per board before flashing: ROLE_DJ / ROLE_FOH / ROLE_STAGE_MGR / ROLE_EVENT_MGR
#define IS_FOH (UNIT_ROLE == ROLE_FOH)   // only FOH ever runs the WebSocket server (hub) — every other role is a client of it

// Wire-protocol destRole sentinels (see PromptCategory.destRole in
// prompts.h) — resolved to a real role at send time by resolveDestRole()
// in network.ino.
#define DEST_HOSPITALITY     253   // Event Manager if connected, else FOH — 60s ack-timeout escalation
#define DEST_REPLY_TO_SENDER 254   // whichever unit sent the message currently being replied to

const char* roleName(uint8_t role) {
  switch (role) {
    case ROLE_DJ:        return "DJ";
    case ROLE_FOH:       return "FOH";
    case ROLE_STAGE_MGR: return "Stage Mgr";
    case ROLE_EVENT_MGR: return "Event Mgr";
    default:             return "?";
  }
}

// Short form for the tight History row — roleName()'s full labels don't fit.
const char* roleAbbrev(uint8_t role) {
  switch (role) {
    case ROLE_DJ:        return "DJ";
    case ROLE_FOH:       return "FOH";
    case ROLE_STAGE_MGR: return "SM";
    case ROLE_EVENT_MGR: return "EM";
    default:             return "?";
  }
}

// Visual theme, chosen per flash — styling only, zero effect on function.
// 0=Midnight  1=Sepia  2=Game Boy  3=Amber  4=Ice  5=LCARS
// 6=Stardew   (full table in theme.h)
#define STAGELINK_THEME 5

// Firmware version, shown on Settings > Device Info. Date-based: bump to
// the current date whenever a firmware change ships to the boards.
#define FW_VERSION "v2026.07.15"

// ---------------- 2. LIBRARIES ----------------
#include <SPI.h>
// FS.h must load before TFT_eSPI.h: TFT_eSPI's ESP32 backend includes FS.h
// with FS_NO_GLOBALS defined (needed for its own smooth-font code), which
// suppresses the global `FS` alias WebServer.h (pulled in by WiFiManager
// below) needs. Loading it here first, without that macro, wins because
// FS.h is include-guarded.
#include <FS.h>
#include <TFT_eSPI.h>
// GFXFF fonts come in via TFT_eSPI.h -> Fonts/GFXFF/gfxfont.h, auto-included
// when LOAD_GFXFF is defined. This project's fonts are generated from the
// SIL-OFL-licensed Press Start 2P typeface and declared using that same
// header, so they must come after: PressStart2P16pt (boot title + incoming
// big message) and PressStart2P14pt (a step smaller with tightened word
// spacing, category/message buttons). Everything else uses the built-in
// numbered fonts.
#include <XPT2046_Touchscreen.h>
#include "PressStart2P16pt.h"
#include "PressStart2P14pt.h"
#include "theme.h"
#include "thumb_icon.h"
#include <WiFi.h>
#include <esp_wifi.h>   // esp_wifi_ap_get_sta_list: per-station RSSI when FOH hosts Direct Link
#include <WiFiUdp.h>
#include <WiFiManager.h>
#include <Preferences.h>
#if IS_FOH
  #include <WebSocketsServer.h>
#else
  #include <WebSocketsClient.h>
#endif
#include "prompts.h"

// ---------------- 3. SCREEN ----------------
#define SCREEN_W 240
#define SCREEN_H 320

// ---------------- 4. PIN MAP (ESP32-2432S028R) ----------------
// TFT pins are set in TFT_eSPI's User_Setup.h, not here — see README.
#define TFT_BL_PIN 21   // backlight, PWM dimmed

// Touch (XPT2046) — its OWN separate SPI bus, not shared with the display
#define TOUCH_CS   33
#define TOUCH_IRQ  36
#define TOUCH_MOSI 32
#define TOUCH_MISO 39
#define TOUCH_CLK  25

// Onboard RGB LED — active LOW (LOW = on)
#define LED_R 4
#define LED_G 16
#define LED_B 17

// Battery voltage sense (ADC). -1 = no battery sensing on this board (the
// classic ESP32-2432S028R has none), which makes batteryPercent() return
// -1 and the on-screen gauge draw all-hollow. On the newer boards with an
// on-board LiPo interface, set this to the battery-sense pin and adjust
// BATTERY_DIVIDER to the board's resistor ratio (commonly 2.0 for a
// half-divider). See case/POWER.md.
#define BATTERY_ADC_PIN   -1
#define BATTERY_DIVIDER   2.0f
#define BATTERY_MIN_MV    3300   // LiPo empty (~0%)
#define BATTERY_MAX_MV    4200   // LiPo full  (~100%)

// ---- Phase 2 sensors (per-board; all OFF on the classic CYD) ----------
// Everything below is a no-op until enabled: readers return TELEM_NONE and
// the drivers compile out, so the existing boards behave exactly as before.
// On the new ESP32-32E board, set the pins from its real pinout and flip the
// HAS_* flags to 1. Pin numbers here are PLACEHOLDERS chosen only to be
// distinct — replace with verified free GPIOs (mind the pin budget: I2C
// needs 2, the I2S mic needs 3, and I2S BCLK/WS must be output-capable).

// Temp/humidity — AHT20 on a shared I2C bus (hand-rolled driver, no library).
#define HAS_TEMP_HUMIDITY 0
#define I2C_SDA    27          // PLACEHOLDER
#define I2C_SCL    22          // PLACEHOLDER
#define AHT20_ADDR 0x38

// SPL — I2S MEMS mic (e.g. INMP441): ESP32 drives BCLK+WS, receives DIN.
// Best-effort dB(A): RMS -> dBFS -> A-weighting -> + per-unit calibration
// offset (stored in Preferences, set from Settings against a reference).
#define HAS_SPL_MIC 0
#define I2S_BCLK   14          // PLACEHOLDER (output-capable)
#define I2S_WS     15          // PLACEHOLDER (output-capable)
#define I2S_DIN    35          // PLACEHOLDER (input-only OK for DIN)
#define SPL_SAMPLE_RATE 48000  // A-weighting coefficients below assume 48 kHz

// ---------------- 5. TOUCH CALIBRATION ----------------
// If taps land in the wrong place: try TOUCH_SWAP_XY first, then the
// invert flags. Only touch the raw min/max numbers if presses feel
// squeezed into part of the screen. See README > Troubleshooting.
#define TOUCH_SWAP_XY   true
#define TOUCH_INVERT_X  true
#define TOUCH_INVERT_Y  false
#define TOUCH_RAW_XMIN  380
#define TOUCH_RAW_XMAX  3700
#define TOUCH_RAW_YMIN  320
#define TOUCH_RAW_YMAX  3650

// Set to 1 to print raw + mapped touch coordinates over Serial (115200) on
// every tap — useful for working out the calibration values above on a new
// board. Leave at 0 normally.
#define TOUCH_DEBUG 1

// ---------------- 6. NETWORK CONSTANTS ----------------
#define DIRECT_AP_SSID  "StageLink-Link"
#define DIRECT_AP_PASS  "boothtofoh"     // 8+ chars required, change if you like
#define WS_PORT         81
#define DISCOVERY_PORT  4210
const IPAddress DIRECT_FOH_IP(192, 168, 4, 1); // FOH is always the AP host in Direct Link mode

// WS connection path, one per role — doubles as FOH's primary mechanism for
// identifying which role just connected (WStype_CONNECTED's payload is the
// client's requested URL path, read synchronously with no extra round
// trip). FOH doesn't connect anywhere, but keeps a slot for index symmetry.
const char* const WS_PATH_BY_ROLE[] = { "/dj", "/foh", "/stagemgr", "/eventmgr" };

// ---------------- 7. SHARED TYPES (kept here so every tab can see them) ----------------
struct Rect { int x, y, w, h; };

struct HistoryEntry {
  String text;
  String category;
  bool incoming;
  bool acked;
  unsigned long atMillis;
  uint8_t role;   // for incoming: the sender; for outgoing: who this unit actually sent the WS frame to
};
#define HISTORY_MAX 8

// A message queued to display next on the incoming overlay — needed once a
// unit can hear from more than one sender (DJ hears FOH + Stage Manager;
// FOH hears all three; Event Manager hears DJ + Stage Manager). Rather than
// the old single-slot model silently overwriting an unacknowledged message,
// arrivals while something is already showing wait here in order.
struct QueuedIncoming {
  String category, text;
  uint8_t senderRole;
  bool isQuestion, escalated;
};
#define INCOMING_QUEUE_MAX 4

// FOH-only: tracks a message FOH relayed to another client, so an A| ack
// coming back can be routed to the original sender (not just to FOH), and
// so an unacked Hospitality relay to Event Manager can escalate to FOH
// after HOSPITALITY_ESCALATE_MS.
struct RelayRecord {
  bool active, escalated;
  String text;
  uint8_t originalSenderRole, relayedToRole;
  unsigned long relayedAtMillis;
};
#define RELAY_TABLE_MAX 6
#define HOSPITALITY_ESCALATE_MS 60000

// FOH-only: latest health telemetry heard from each peer role, indexed by
// role code (0..3). Fed by S| frames (see serviceTelemetry / routeFromClient).
// Sentinel -1000 = "no value / not yet heard"; individual fields stay at
// their sentinel when a unit doesn't have that sensor (older boards send
// only battery=-1 and rssi). The Phase 3 dashboard reads this table.
#define TELEM_NONE (-1000)
struct UnitTelemetry {
  bool everHeard;
  unsigned long atMillis;   // when this row last updated (staleness)
  int batteryPct;           // 0-100, or -1 if the unit has no battery sense
  int battMv;
  int tempC10;              // temperature x10 (so one int carries a decimal)
  int humidity;             // %RH
  int splDbA;               // dB(A)
  int rssi;                 // dBm
};
#define TELEMETRY_BROADCAST_MS 8000

// Upper bound on labels pickButtonFontStep() sizes at once (a category's
// items, or a submenu's subcategories) — headroom above prompts.h's
// largest list (Hospitality/Quick Comms, 7 items) for future growth.
#define MAX_GRID_LABELS 8

// Category/submenu screens always lay out this many row slots, whatever
// the actual list length — paging through the rest via categoryPageNavRect
// (display_ui.ino) instead of shrinking every row to cram everything onto
// one screen. Keeps button/font size identical across every category.
#define CATEGORY_ITEMS_PER_PAGE 4

enum NetMode { MODE_DIRECT = 0, MODE_VENUE = 1 };
enum Screen  { SCR_HOME, SCR_CATEGORY, SCR_INCOMING, SCR_HISTORY, SCR_SETTINGS, SCR_DEVINFO, SCR_TRANSIENT };

// Wire sentinel for the thumbs-up transient — matched on receipt and drawn
// as a pixel glyph, never rendered as font text.
#define THUMBS_WIRE_TEXT "[thumbs-up]"

// Arduino's ctags-based auto-prototype pass inserts generated forward
// declarations near the top of this file — before Rect/Screen/NetMode/
// PromptCategory exist yet, and before the WebSockets library headers that
// define WStype_t. Any function elsewhere using one of those types in its
// signature gets a broken auto-prototype (this codebase has never
// compiled before now — see HANDOFF.md). Writing the correct prototype
// ourselves, here, after all of those types ARE visible, makes ctags skip
// generating its own broken one for that function.
bool pointInRect(int x, int y, Rect r);
Rect gridRect(int index, int count, int areaX, int areaY, int areaW, int areaH, int cols);
int btnRadius(Rect r);
void shadedRoundRect(Rect r, uint16_t base);
void flashPress(Rect r);
void drawButton(Rect r, const String &label, uint16_t accent, bool bold, int forcedStep);
void drawButton(Rect r, const String &label, uint16_t accent, bool bold);
void drawButton(Rect r, const String &label, uint16_t accent);
int pickButtonFontStep(const String labels[], int count, Rect sample);
void drawSwipeHint(Rect r, uint16_t color);
void showSwipeNudge(Rect r);
bool swipeHit(Rect r, int dx, int dy, int cx2, int cy2);
void lcarsRailRects(int yTop, int yBottom, Rect &battery, Rect &wifi, Rect &brightness);
void drawFillMeter(Rect r, uint16_t accent, int pct);
Rect carouselButtonRect();
const PromptCategory* currentCatDef();
bool onSubmenu();
void categoryBottomRow(Rect &back, Rect &stage, bool draw);
Rect categoryPageNavRect();
void activateCategoryItem(int i, Rect r);
void flashSent(Rect r, uint16_t accent);
Rect questionAnswerRect(int i);
void redrawScreen(Screen s);
#if UNIT_ROLE == ROLE_DJ
void sendStageUrgent(Rect stage);
#endif
void settingsOptionRow(Rect r, const String &label, bool selected);
void settingsFlatButton(Rect r, const String &label);
void actionSetMode(NetMode m);
#if !IS_FOH
void wsClientEvent(WStype_t type, uint8_t *payload, size_t length);
#else
void wsServerEvent(uint8_t num, WStype_t type, uint8_t *payload, size_t length);
#endif

// ---------------- 8. GLOBAL OBJECTS ----------------
TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI = SPIClass(VSPI);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);
Preferences prefs;
WiFiUDP discoveryUDP;

#if IS_FOH
  WebSocketsServer ws = WebSocketsServer(WS_PORT);
  uint8_t clientRole[WEBSOCKETS_SERVER_CLIENT_MAX];   // role identified on each client slot, or ROLE_UNKNOWN
  bool clientConnected[WEBSOCKETS_SERVER_CLIENT_MAX];
  RelayRecord relayTable[RELAY_TABLE_MAX];
  UnitTelemetry telemetry[4];   // latest health per peer role (0..3); see UnitTelemetry
#else
  WebSocketsClient ws;
  bool wsClientStarted = false;
#endif

// ---------------- 9. PALETTE (filled in by initPalette() at boot) ----------------
uint16_t COL_BG, COL_PANEL, COL_TEXT, COL_TEXT_DIM;
uint16_t COL_TEAL, COL_AMBER, COL_CORAL, COL_ALERT, COL_OK, COL_BORDER;
uint16_t COL_GREEN, COL_YELLOW;   // connection status only — UI stays grayscale+red otherwise

// ---------------- 10. APP STATE ----------------
NetMode netMode = MODE_DIRECT;
bool venueConfigured = false;  // true once venue wifi has ever connected/been set up successfully
Screen currentScreen = SCR_HOME;

int activeCategory = -1;
int activeSubcategory = -1;   // -1 = showing the parent's submenu (or a leaf category)
int homeCarouselIndex = 0;    // carouselHome themes only: which category the home screen shows
int categoryPage = 0;         // which page of the submenu/item grid is showing (see categoryBottomRow's
                               // neighbor categoryPageNavRect in display_ui.ino) — reset on every
                               // navigation that changes which list is on screen
Screen savedScreenBeforeIncoming = SCR_HOME;
bool wsConnected = false;
uint8_t backlightLevel = 55;     // 0-100, normal dark-room-friendly default

HistoryEntry history[HISTORY_MAX];
uint8_t historyCount = 0;
uint8_t historyPage = 0;

String incomingCategory = "";
String incomingText = "";
String lastSentText = "";
bool incomingAcked = false;
bool incomingIsQuestion = false;   // Q| overlay: answer buttons instead of Seen
bool incomingEscalated = false;    // Hospitality relay that timed out on Event Manager and landed on FOH
uint8_t incomingSenderRole = ROLE_UNKNOWN;
unsigned long incomingAckedAtMillis = 0;
unsigned long incomingShownAtMillis = 0;

// Arrivals while the incoming overlay is already showing something else
// queue here instead of clobbering it — see QueuedIncoming above.
QueuedIncoming incomingQueue[INCOMING_QUEUE_MAX];
uint8_t incomingQueueCount = 0;

// The sender of the last message that arrived — read by DEST_REPLY_TO_SENDER
// categories (Event Manager's Quick Comms) to know who to reply to.
uint8_t lastIncomingSenderRole = ROLE_UNKNOWN;

// Transient banner (T| wire type): brief auto-dismissing display, no Seen,
// no ack — used by question answers and the thumbs-up.
String transientText = "";
uint8_t transientSenderRole = ROLE_UNKNOWN;
unsigned long transientShownAt = 0;
Screen savedScreenBeforeTransient = SCR_HOME;

bool wasTouched = false;
unsigned long lastTouchAccept = 0;

// Intentional-press gate: a touch must stay put this long before it counts
// as a tap, so a graze can't fire a button. Release early = no tap. This is
// the only accidental-press guard for message-send buttons unless a theme
// opts into swipeToSend (trySwipeGesture in display_ui.ino) instead.
// Loosened from 200ms/18px after real-hardware testing found the tighter
// gate made ordinary taps on the resistive touchscreen feel unreliable.
#define TAP_HOLD_MS 150
#define TAP_SLOP_PX 26
#define SWIPE_DIST  90    // horizontal travel to complete a swipe-send
#define SWIPE_BAND  26    // max vertical wander during a swipe
bool tapPending = false;
bool tapFired = false;
bool touchDownValid = false;
bool swipeConsumed = false;
unsigned long tapStart = 0;
int tapX = 0, tapY = 0;

// Ack-check feedback: the last sent prompt waits here until the peer's
// "seen" ack arrives, then an animated green check pops on the button
// that sent it (serviceAckCheck / drawAckCheck in display_ui.ino) and
// auto-dismisses after 10s. pendingAckText clears on the FIRST matching
// ack, so acks for duplicate/repeated requests only trigger one check.
// Category tracking is a (category, subcategory) pair so the check finds
// its way back to buttons on nested Sound sub-screens too.
String pendingAckText = "";
int pendingAckCategory = -1;
int pendingAckSubcategory = -1;
Rect pendingAckRect;
bool ackCheckActive = false;
unsigned long ackCheckStart = 0;
int ackCheckCategory = -1;
int ackCheckSubcategory = -1;
Rect ackCheckRect;
int ackCheckLastFrame = -1;

// ---------------- 11. SETUP ----------------
void setup() {
  Serial.begin(115200);

  // Red channel is PWM-driven so the incoming-message pulse can breathe
  // smoothly (serviceAlertPulse); green/blue stay plain digital, held off.
  ledcAttach(LED_R, 5000, 8);
  ledcWrite(LED_R, 255);   // active-low: 255 = off
  pinMode(LED_G, OUTPUT); pinMode(LED_B, OUTPUT);
  digitalWrite(LED_G, HIGH); digitalWrite(LED_B, HIGH); // off (active-low)

  tft.init();
  // Portrait, cable/USB edge at bottom. This value is a first guess and
  // hasn't been confirmed on real hardware yet — if the boot screen text
  // isn't upright with the cable exiting the bottom, try 2 or 3 next and
  // update this comment once confirmed. Touch calibration below will also
  // need re-deriving once rotation is confirmed (see README > Troubleshooting).
  tft.setRotation(0);
  initPalette();

  // Must come after tft.init(): TFT_eSPI's init() does its own pinMode()/
  // digitalWrite() on TFT_BL to turn the backlight on, which detaches the
  // pin from any LEDC PWM routing set up before it — attaching here instead
  // means our attach is the one that sticks, so ledcWrite() actually works.
  ledcAttach(TFT_BL_PIN, 5000, 8);

  prefs.begin("stagelink", false);
  netMode = (NetMode) prefs.getUChar("netMode", MODE_DIRECT);
  venueConfigured = prefs.getBool("venueConfigured", false);
  backlightLevel = prefs.getUChar("backlight", 55);
  setBacklight(backlightLevel);

  tft.fillScreen(COL_BG);
  drawBootScreen();
  spinBootReel();   // ~1.5s slow-spin intro before networking takes over

  touchSPI.begin(TOUCH_CLK, TOUCH_MISO, TOUCH_MOSI, TOUCH_CS);
  touch.begin(touchSPI);

  startNetworking();
  startDashboard();   // FOH serves the read-only web dashboard; DJ stub is empty

  currentScreen = SCR_HOME;
  drawHome();
}

// ---------------- 12. LOOP ----------------
void loop() {
  serviceNetworking();
  handleTouch();
  serviceAlertPulse();
  serviceIncomingAutoDismiss();
  serviceTransient();
  serviceStatusBar();
  serviceAckCheck();
  serviceDashboard();
  serviceSensors();
  serviceTelemetry();
}
