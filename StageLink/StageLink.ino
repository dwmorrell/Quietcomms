/*
  ============================================================
  StageLink — DJ <-> FOH button comms over WiFi
  Board:  ESP32-2432S028R "Cheap Yellow Display" (2.8", resistive touch)
  ============================================================

  ONE SKETCH, TWO BOARDS:
  Set IS_DJ_UNIT below, upload to the DJ booth board.
  Flip it to 0, upload to the FOH board.
  That single line is the only difference between the two units.

  Before this will compile you need to install 4 libraries and
  configure TFT_eSPI once. Full steps are in README.md.
*/

// ---------------- 1. ROLE ----------------
#define IS_DJ_UNIT 1   // 1 = DJ booth unit, 0 = FOH unit. Set per board before flashing.

// Visual theme, chosen per flash — styling only, zero effect on function.
// 0=Midnight  1=Sepia  2=Game Boy  3=Amber  4=Ice  5=LCARS
// 6=Stardew   (full table in theme.h)
#define STAGELINK_THEME 5

// Firmware version, shown on Settings > Device Info. Date-based: bump to
// the current date whenever a firmware change ships to the boards.
#define FW_VERSION "v2026.07.14"

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
#if IS_DJ_UNIT
  #include <WebSocketsClient.h>
#else
  #include <WebSocketsServer.h>
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

// ---------------- 7. SHARED TYPES (kept here so every tab can see them) ----------------
struct Rect { int x, y, w, h; };

struct HistoryEntry {
  String text;
  String category;
  bool incoming;
  bool acked;
  unsigned long atMillis;
};
#define HISTORY_MAX 8

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

// ---------------- 8. GLOBAL OBJECTS ----------------
TFT_eSPI tft = TFT_eSPI();
SPIClass touchSPI = SPIClass(VSPI);
XPT2046_Touchscreen touch(TOUCH_CS, TOUCH_IRQ);
Preferences prefs;
WiFiUDP discoveryUDP;

#if IS_DJ_UNIT
  WebSocketsClient ws;
  bool wsClientStarted = false;
#else
  WebSocketsServer ws = WebSocketsServer(WS_PORT);
  uint8_t peerClientNum = 0;
  bool peerClientKnown = false;
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
unsigned long incomingAckedAtMillis = 0;
unsigned long incomingShownAtMillis = 0;

// Transient banner (T| wire type): brief auto-dismissing display, no Seen,
// no ack — used by question answers and the thumbs-up.
String transientText = "";
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
}
