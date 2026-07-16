/*
  theme.h - flash-time visual themes. Pick one with STAGELINK_THEME in
  StageLink.ino. Styling only: palettes plus three style knobs; nothing
  here changes behavior.

  Colors are 24-bit hex (copy-pastable into CSS previews) and quantized
  to the display's RGB565 in initPalette(), so a previewed color is the
  shipped color.

  Knobs:
    buttonRadius - max button corner radius in px; 99 = full pill (h/2)
    useBevel     - inset highlight rings on buttons (off = flat fills)
    fontFamily   - the theme's typeface (ThemeFont below). Category
                   buttons shrink through the family's size ladder until
                   the label fits (see setThemeButtonFont in display_ui).
*/
#pragma once

enum ThemeFont : uint8_t {
  TF_PIXEL = 0,   // Press Start 2P (generated, retro 8-bit)
  TF_SANS  = 1,   // FreeSansBold (bundled with TFT_eSPI)
  TF_SERIF = 2,   // FreeSerifBold
  TF_MONO  = 3,   // FreeMonoBold
};

struct ThemeSpec {
  const char* name;
  uint32_t bg, panel, text, textDim;
  uint32_t shadeA, shadeB, shadeC;   // category colorId 0/1/2 accents
  uint32_t alert;                    // colorId 3 + alerts - always reads as red
  uint32_t ok;                       // Sent/Seen feedback
  uint32_t border;
  uint32_t green, yellow;            // connection dot: linked / net-no-peer
  uint8_t buttonRadius;              // 99 = full pill
  bool useBevel;
  uint8_t fontFamily;                // ThemeFont
  bool lcarsChrome;                  // LCARS screen chrome: sidebar rail + capped bars
  bool carouselHome;                 // home screen shows one category at a time,
                                      // swipe left/right to page (see drawHomeCarousel)
  bool swipeToSend;                  // message-send buttons require a swipe instead of
                                      // a tap; off = the ordinary hold-to-confirm tap gate
};

const ThemeSpec THEMES[] = {
  // 0 Midnight - the original grayscale-with-red look
  {"Midnight",  0x000000, 0x282828, 0xFFFFFF, 0x969696,
                0x464646, 0xA5A5A5, 0xD7D7D7, 0xC63838, 0xFFFFFF,
                0x5A5A5A, 0x4CB06C, 0xD2B43C, 16, true,  TF_PIXEL, false},
  // 1 Sepia - warm tan/brown/cream, "Papers, Please"
  {"Sepia",     0x2B1D12, 0x4A3524, 0xEFDFB3, 0xA08865,
                0x5C4630, 0x8A6C4C, 0xBA9B6E, 0xC8502E, 0xEFDFB3,
                0x7A5F40, 0x6E8B3D, 0xC9A227, 6,  false, TF_SERIF, false},
  // 2 Game Boy - 4-shade olive/green monochrome (alert stays true red: safety > purism)
  {"Game Boy",  0x0F380F, 0x285C28, 0x9BBC0F, 0x73942A,
                0x306230, 0x8BAC0F, 0x9BBC0F, 0xB03A30, 0x9BBC0F,
                0x4F7A42, 0x55A845, 0xC8B430, 8,  true,  TF_PIXEL, false},
  // 3 Amber terminal - black + amber CRT, monospace
  {"Amber",     0x140A00, 0x2E1E04, 0xFFB000, 0x8F6210,
                0x3C2800, 0x7A5200, 0xB87E10, 0xFF4530, 0xFFB000,
                0x5C3D08, 0x4CB06C, 0xFFDD55, 4,  false, TF_MONO, false},
  // 4 Ice - near-black + steel blue/cyan, clean sans
  {"Ice",       0x0A1220, 0x1C2A3E, 0xD8ECFF, 0x7E97B0,
                0x26405C, 0x4A7196, 0x7FA8C9, 0xE04848, 0xD8ECFF,
                0x3A5570, 0x3FBF8F, 0xD9B84A, 6,  false, TF_SANS, false},
  // 5 LCARS - TNG console: flat pills, orange/peach/lavender on black
  {"LCARS",     0x000000, 0x1A1A2E, 0xFFFFFF, 0x9090B8,
                0xFF9C00, 0xFFCC99, 0xCC99CC, 0xD14C4C, 0xFFFFFF,
                0x000000, 0x66CC66, 0xFFCC00, 99, false, TF_SANS, true},
  // 6 Stardew Valley - warm wood browns + cream, storybook serif
  {"Stardew",   0x3A2415, 0x5C3A21, 0xF7E7C6, 0xB08A5E,
                0x6E4A2A, 0x9C6B3F, 0xC89A62, 0xC0392B, 0xF7E7C6,
                0x8A5C33, 0x5FA548, 0xE8B93E, 6,  false, TF_SERIF, false},
};

#define THEME (THEMES[STAGELINK_THEME])
