/*
  cyberpunk-legacy.h - the original generic "Cyberpunk" ThemeSpec entry,
  replaced in theme.h by "Cyberpunk 2077" (2026-07-15): pure black bg
  instead of near-black, single-button carousel home screen. Not compiled
  into any build — kept here purely as a record in case the older flat
  neon look is ever wanted back. To restore: paste the entry back into
  THEMES[] in theme.h (drop the trailing carouselHome value, it predates
  that field) and pick a free index.
*/

// 6 Cyberpunk (legacy) - near-black + neon yellow/cyan/magenta, hard
// corners, mono, ordinary grid home screen
{"Cyberpunk", 0x0A0A12, 0x181820, 0xFCEE0A, 0x6A6A80,
              0x262636, 0x00B8C8, 0xE93CAC, 0xFF2E5B, 0xFCEE0A,
              0x3A3A50, 0x39FF88, 0xFCEE0A, 4,  false, TF_MONO, false},
