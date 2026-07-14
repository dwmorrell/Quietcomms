# Archived: SD-card firmware self-update

Boot-time firmware updates from the CYD's microSD slot — insert a FAT32
card with the role's `.bin` at the root, power on, the board flashes
itself and reboots. Pulled from the active build to keep the sketch on
the default partition scheme; re-integrate when wanted.

**Why it was archived:** the SD/OTA libraries add ~55KB, which pushed the
sketch to ~101% of the default partition's app slot ("Sketch too big").
Making it fit needs the **Minimal SPIFFS** partition scheme (1.9MB app
slots, keeps two OTA slots) — a partition-table change that has to be
re-flashed to both boards over USB. That's the main cost of turning this
back on.

## To re-integrate

1. Move `sdupdate.ino.txt` back to `StageLink/sdupdate.ino`.

2. In `StageLink.ino`, restore the three pieces removed when archiving:

   Includes (after the WiFi includes):
   ```cpp
   #include <SD.h>          // boot-time firmware updates from the CYD's microSD slot
   #include <Update.h>      // writes a firmware image into the spare OTA app slot
   #include <MD5Builder.h>  // checksum guard so the same SD image isn't reflashed every boot
   ```

   Pin map (after the touch pins):
   ```cpp
   // microSD slot — hardwired on the CYD to the classic VSPI pins. Only used
   // briefly at boot for SD firmware updates (sdupdate.ino), BEFORE the touch
   // controller claims the VSPI peripheral with its own remapped pins.
   #define SD_CS   5
   #define SD_SCK  18
   #define SD_MISO 19
   #define SD_MOSI 23
   ```

   In `setup()`, between `drawBootScreen();` and `touchSPI.begin(...)`:
   ```cpp
   // Check the SD slot for a firmware image before touch claims VSPI: both
   // the SD card and the touch controller use the VSPI peripheral, so this
   // borrows it first and releases it. Needs prefs (opened above) for its
   // already-flashed checksum guard. Restarts the board if it applies one.
   checkSdFirmwareUpdate();
   ```
   (The boot screen must be drawn first so `checkSdFirmwareUpdate()`'s
   `drawStatusLine()` progress messages have somewhere to show.)

3. Build BOTH roles with `--fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs`
   and flash both boards over USB (the partition table change can't happen
   over the air). Export the OTA images for the SD card with the same FQBN:
   `arduino-cli compile --fqbn esp32:esp32:esp32:PartitionScheme=min_spiffs --output-dir <dir> .`,
   then the SD file is `<dir>/StageLink.ino.bin` renamed to
   `stagelink-dj.bin` / `stagelink-foh.bin`.

4. Re-add the "Updating later from a microSD card" section to `README.md`
   (its text is preserved below), and flip the partition note in section 1
   back to "required."

## README section text (removed from README.md when archived)

```
## 4b. Updating later from a microSD card (no PC)
Once a board is running firmware from this version or later, you can
update it in the field with just a microSD card — no laptop, no USB.

1. Grab the prebuilt image for the role from the `firmware/` folder next
   to this sketch: `stagelink-dj.bin` for a DJ board, `stagelink-foh.bin`
   for an FOH board. (To build fresh ones yourself, compile each role with
   `arduino-cli compile --fqbn esp32:esp32:esp32 --export-binaries` and
   grab `build/esp32.esp32.esp32/StageLink.ino.bin`.)
2. Format a microSD card as FAT32 and copy the image to the card's
   root (top level, not in a folder). Put only the image for the role
   you're flashing — a DJ board ignores stagelink-foh.bin and vice
   versa, so it's fine to keep both on one card.
3. Power the board off, insert the card, power it on. It shows
   "Updating from SD...", writes the image, then "Update OK - restarting"
   and reboots into the new firmware. The whole thing takes a few seconds.
4. You can leave the card in — the board records each image it flashes
   and skips one it's already applied, so it won't re-flash on every boot.
   To push a newer build, just overwrite the .bin on the card.

Note: this only works on boards already running SD-capable firmware. The
very first flash of this version has to go over USB (steps in section 4);
after that, SD updates take over. If the card is missing or has no
matching image, the board just boots normally.
```
