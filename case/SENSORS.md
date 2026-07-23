# StageLink Phase 2: sensors (new ESP32-32E board)

How the per-unit sensors wire up and get enabled. All of this is **off by
default** — the firmware ships with every `HAS_*` flag at 0 and the pin
defines at placeholder values, so the classic ESP32-2432S028R boards are
unaffected. Turn a sensor on by setting its pins from the real board
pinout and flipping its flag in `StageLink.ino` (section 4, pin map).

No external Arduino libraries are needed — the drivers are hand-rolled
against the ESP32 core (`Wire` for I2C, `ESP_I2S` for the mic).

## Pin budget (the gating constraint — verify on the real board)

The CYD family is pin-starved: the display, its backlight, the touch
controller (own SPI bus), and the RGB LED already claim most GPIOs. The
new sensors need, at minimum:

| Function            | Pins | Notes |
|---------------------|------|-------|
| Battery sense (ADC) | 1    | Usually pre-wired on the new board's LiPo interface; read-only, input-only GPIO is fine. |
| Temp/humidity (I2C) | 2    | SDA + SCL, shareable if other I2C devices are added later. |
| SPL mic (I2S)       | 3    | BCLK + WS must be **output-capable**; DIN can be input-only (e.g. GPIO35). |

That's up to 6 pins. **Confirm the ESP32-32E's free GPIOs against its
pinout before committing** — if there aren't 3 spare for I2S, fall back to
an analog electret mic on a single ADC pin (cruder, see SPL note). The pin
defines in `StageLink.ino` are placeholders picked only to be distinct;
replace them.

## Battery (LiPo, on-board interface)

- `BATTERY_ADC_PIN` = the board's battery-sense pin; `BATTERY_DIVIDER` =
  its resistor ratio (commonly 2.0 for a half-divider).
- Reads via the ESP32's calibrated `analogReadMilliVolts()`, smoothed.
  Maps `BATTERY_MIN_MV`..`BATTERY_MAX_MV` (3300..4200 default) to 0-100%.
- Feeds the on-screen battery gauge and the `S|` telemetry frame.
- **Validate:** confirm the divider ratio and the empty/full voltages
  against a meter and the actual pack; adjust the two `*_MV` constants.

## Temp / humidity (AHT20 over I2C)

- Set `HAS_TEMP_HUMIDITY 1`, `I2C_SDA`, `I2C_SCL`. Address `AHT20_ADDR`
  0x38 (default AHT20). An SHT31 could drop in with a different address +
  read sequence if preferred.
- Driver is a ~30-line hand-rolled AHT20 (`sensors.ino`): init/calibrate,
  trigger, 80 ms convert, read 6 bytes, unpack the 20-bit humidity and
  temperature. Reports degC x10 and %RH into telemetry + Device Info.

## SPL (I2S MEMS mic, e.g. INMP441)

- Set `HAS_SPL_MIC 1`, `I2S_BCLK`, `I2S_WS`, `I2S_DIN`, `SPL_SAMPLE_RATE`
  (48 kHz default).
- Pipeline (`sensors.ino`): read 32-bit I2S blocks (INMP441 packs 24-bit
  audio in the top of each slot) → RMS → dBFS (ref 2^23) → + per-unit
  calibration offset → reported as `splDbA()`.
- **Two things still need the real mic to finish, and are flagged in code:**
  1. **A-weighting is a pass-through stub right now** (`aWeight()` returns
     the sample unchanged), so `splDbA()` currently reports **unweighted
     dB, not true dB(A)**. Fitting it means generating a 3-biquad cascade
     from the IEC 61672 A-weighting prototype for `SPL_SAMPLE_RATE`
     (bilinear transform) and validating against a reference meter.
  2. **Calibration** — `calibrateSpl(referenceDb)` stores the offset that
     makes the reading match a reference SPL meter held at the mic, saved
     in Preferences. A Settings button to trigger it gets wired once the
     mic is in hand (calibration needs the hardware anyway).
- If the pin budget can't spare 3 GPIOs for I2S, an analog electret + amp
  (MAX9814) on one ADC pin is the fallback — simpler wiring, but noisier
  and further from real dB(A). The RMS→dB→calibration path is reusable;
  only the front-end read changes.

## Telemetry

Each sensored unit already reports these over the existing `S|` frame
(battery%, battery mV, tempC x10, humidity%, SPL dB, RSSI) to the FOH hub
every ~8 s — additive and optional, absent fields sent as `-`. The Phase 3
companion dashboard consumes this table. Nothing else to wire for a sensor
to show up remotely once it's reporting locally.
