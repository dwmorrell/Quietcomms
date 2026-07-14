# StageLink enclosure: power + antenna spec

Target board: ESP32-2432S028R "Cheap Yellow Display" (2.8" resistive CYD).
Nominal board size ~86.5 x 50 mm with 4 corner mounting holes; the display
module and SD slot sit on opposite faces. **Verify hole spacing and port
positions with calipers against your actual boards before printing** —
CYD production batches vary by a millimeter or two.

## Power budget

Measured/typical draw for this firmware (WiFi on, backlight ~55%):
- Average: ~150–180 mA at 5 V
- Peaks: ~400–500 mA during WiFi TX bursts

Any supply option must deliver **5 V at 1 A** comfortably.

## Option A — rechargeable via USB-C (recommended)

Use an IP5306-based power-bank module (widely sold as "5V 2A power bank
module, USB-C"): it combines Li-ion charging, protection, 5 V boost, and
load detection on one board about 25 x 18 mm.

- Cell: one 18650 (2500–3500 mAh) in a spring holder, or a 103450-size
  LiPo pouch with JST lead.
- Wiring: cell -> IP5306 BAT +/- ; IP5306 5V OUT -> CYD 5 V pin (P1 VIN)
  and GND. The CYD's own micro-USB stays free for flashing.
- Case exposes the module's **USB-C jack** through a side cutout for
  charging; the module's charge LEDs can share the light-leak gaps.
- Runtime estimate: 3000 mAh x 3.7 V x ~0.8 boost efficiency
  / (5 V x 0.17 A) ≈ **10 hours**.
- Note: IP5306 auto-sleeps below ~50 mA load — the CYD idles well above
  that, so no keep-alive issues.

## Option B — AA batteries

3 x AA (4.5 V fresh, sags to ~3.3 V) is NOT enough headroom for the
CYD's 5 V rail + AMS1117 regulator. Use **4 x AA (6 V nominal)** into a
small buck converter:

- Holder: standard 4xAA flat holder ~62 x 58 x 16 mm.
- Regulator: Mini-360 (MP2307) buck set to 5.0 V -> CYD 5 V pin + GND.
- Runtime estimate: ~2000 mAh alkaline ≈ **12 hours**; NiMH (4.8 V pack)
  also works through the same buck.
- Fit a rocker/slide power switch in the battery leg on either option.

The case model (stagelink_case.scad) has a parametric battery bay sized
for whichever option you build — set `battery_mode` to "18650" or "4xAA".

## External antenna

The CYD's ESP32 module uses its onboard PCB antenna by default. To use
an external antenna:

1. If the module is a **WROOM-32U**, it already has a U.FL/IPEX socket —
   just plug in.
2. On the common **WROOM-32 / bare-antenna modules**, there is a 0-ohm
   link resistor near the antenna that selects PCB antenna vs the IPEX
   pad. Rotate/move that resistor (or bridge with a solder blob) to the
   IPEX position. This is fine-pitch soldering — magnification needed.
3. Use a **U.FL -> RP-SMA bulkhead pigtail** (~100 mm) to the case's
   antenna port, and any 2.4 GHz RP-SMA antenna (2–5 dBi).

The case model includes a 6.5 mm bulkhead hole for the RP-SMA jack and
clearance to route the pigtail away from the battery bay.

## Front LED glow

The CYD's RGB alert LED sits on the component side of the PCB (facing
the case interior). The case gets its jack-o-lantern glow from the
deliberate **1.8 mm light-leak gap** running around the display bezel:
the bezel frame is held by four corner tabs only, so interior light
escapes around all four display edges. Tips for a stronger glow:

- Print the case interior in white or natural (translucent) filament,
  or line the inside with white tape — the LED light bounces forward.
- Avoid black interior walls; they eat the glow.
- The gap width is the `glow_gap` parameter in the SCAD file.
