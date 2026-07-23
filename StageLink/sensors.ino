/*
  sensors.ino — on-board sensor reads.

  Phase 1: battery voltage only, guarded by BATTERY_ADC_PIN. On boards
  with no battery-sense pin (the classic ESP32-2432S028R, BATTERY_ADC_PIN
  -1) every reader returns -1 and serviceSensors() compiles to nothing, so
  the existing units behave exactly as before. On the newer boards with an
  on-board LiPo interface, set BATTERY_ADC_PIN + BATTERY_DIVIDER and this
  starts feeding the battery gauge and telemetry.

  Phase 2 adds I2C temp/humidity and the I2S SPL mic here.
*/

static int batteryCachedMv = -1;   // -1 until a reading exists (or forever if no pin)

// Latest smoothed battery voltage in millivolts; -1 when unavailable.
int batteryMilliVolts() {
  return batteryCachedMv;
}

// Battery charge 0-100, or -1 when there's no sensing on this board.
int batteryPercent() {
  if (batteryCachedMv < 0) return -1;
  return map(constrain(batteryCachedMv, BATTERY_MIN_MV, BATTERY_MAX_MV),
             BATTERY_MIN_MV, BATTERY_MAX_MV, 0, 100);
}

// Samples the battery on a slow cadence and exponentially smooths it, so
// the gauge doesn't jitter on the voltage dips during WiFi transmit bursts.
void serviceSensors() {
#if BATTERY_ADC_PIN >= 0
  static unsigned long last = 0;
  if (millis() - last < 3000) return;
  last = millis();
  // analogReadMilliVolts() uses the ESP32's factory ADC calibration curve.
  int measured = (int)(analogReadMilliVolts(BATTERY_ADC_PIN) * BATTERY_DIVIDER);
  batteryCachedMv = (batteryCachedMv < 0) ? measured
                                          : (batteryCachedMv * 3 + measured) / 4;
#endif
}
