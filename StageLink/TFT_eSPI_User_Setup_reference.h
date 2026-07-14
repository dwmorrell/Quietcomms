// ============================================================
// Copy this ENTIRE file's content into User_Setup.h inside your
// TFT_eSPI library folder (overwrite what's there). One-time setup,
// applies to both boards since they're the same hardware.
//
// Library folder is usually at:
//   Windows: Documents\Arduino\libraries\TFT_eSPI\User_Setup.h
//   Mac:     ~/Documents/Arduino/libraries/TFT_eSPI/User_Setup.h
//   Linux:   ~/Arduino/libraries/TFT_eSPI/User_Setup.h
// ============================================================

#define ILI9341_DRIVER
// If the screen stays blank after uploading, comment ILI9341_DRIVER
// out and uncomment the line below instead — CYD boards ship with
// either chip depending on production batch:
// #define ST7789_DRIVER

#define TFT_MISO 12
#define TFT_MOSI 13
#define TFT_SCLK 14
#define TFT_CS    15
#define TFT_DC     2
#define TFT_RST   -1
#define TFT_BL    21
#define TFT_BACKLIGHT_ON HIGH

// This project reads touch itself via XPT2046_Touchscreen on its own
// SPI bus, so TFT_eSPI's built-in touch (XPT2046_SPI_PORT etc.) is
// intentionally left out here — it isn't needed.

#define SPI_FREQUENCY        40000000
#define SPI_READ_FREQUENCY   20000000
#define SPI_TOUCH_FREQUENCY   2500000

#define LOAD_GLCD
#define LOAD_FONT2
#define LOAD_FONT4
#define LOAD_GFXFF
// SMOOTH_FONT intentionally left undefined — this project uses a blocky,
// non-anti-aliased retro 8-bit look, and SMOOTH_FONT would anti-alias the
// numbered GLCD fonts (2 and 4) used throughout the UI.
