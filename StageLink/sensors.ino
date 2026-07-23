/*
  sensors.ino — on-board sensor reads. Everything here is guarded by a
  per-board feature flag / pin define in StageLink.ino, so on the classic
  ESP32-2432S028R (all flags off) every reader returns TELEM_NONE and the
  drivers compile out — the existing units are unchanged. On the new
  ESP32-32E board, set the pins + flip the HAS_* flags and these light up.

  Deps: none beyond the ESP32 core (Wire for I2C, ESP_I2S for the mic).

    - battery:      analogReadMilliVolts on BATTERY_ADC_PIN
    - temp/humidity: hand-rolled AHT20 over I2C (HAS_TEMP_HUMIDITY)
    - SPL:          I2S MEMS mic, RMS -> dBFS -> (A-weight) -> calibration
                    (HAS_SPL_MIC)

  All readers return "x10" or plain ints matching the S| telemetry fields:
  batteryPercent() 0-100, tempC10() degC x10, humidityPct() %RH,
  splDbA() dB, or TELEM_NONE when unavailable.
*/

#if HAS_TEMP_HUMIDITY || HAS_SPL_MIC
#include <math.h>
#endif
#if HAS_TEMP_HUMIDITY
#include <Wire.h>
#endif
#if HAS_SPL_MIC
#include <ESP_I2S.h>
#endif

// ---------------- battery (Phase 1) ----------------
static int batteryCachedMv = -1;   // -1 until a reading exists (or forever if no pin)

int batteryMilliVolts() { return batteryCachedMv; }

int batteryPercent() {
  if (batteryCachedMv < 0) return -1;
  return map(constrain(batteryCachedMv, BATTERY_MIN_MV, BATTERY_MAX_MV),
             BATTERY_MIN_MV, BATTERY_MAX_MV, 0, 100);
}

// ---------------- temp / humidity (AHT20 over I2C) ----------------
#if HAS_TEMP_HUMIDITY
static bool ahtReady = false;
static int cachedTempC10 = TELEM_NONE;
static int cachedHumidity = TELEM_NONE;

static void ahtInit() {
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(40);                              // AHT20 power-up time
  Wire.beginTransmission(AHT20_ADDR);
  Wire.write(0xBE); Wire.write(0x08); Wire.write(0x00);   // init/calibrate
  ahtReady = (Wire.endTransmission() == 0);
  delay(10);
}

static void ahtSample() {
  if (!ahtReady) { ahtInit(); if (!ahtReady) return; }
  Wire.beginTransmission(AHT20_ADDR);
  Wire.write(0xAC); Wire.write(0x33); Wire.write(0x00);   // trigger measurement
  if (Wire.endTransmission() != 0) { ahtReady = false; return; }
  delay(80);                                              // conversion time
  if (Wire.requestFrom((uint8_t)AHT20_ADDR, (uint8_t)6) != 6) return;
  uint8_t d[6];
  for (int i = 0; i < 6; i++) d[i] = Wire.read();
  if (d[0] & 0x80) return;                                // still busy — skip
  uint32_t rawH = ((uint32_t)d[1] << 12) | ((uint32_t)d[2] << 4) | (d[3] >> 4);
  uint32_t rawT = (((uint32_t)d[3] & 0x0F) << 16) | ((uint32_t)d[4] << 8) | d[5];
  cachedHumidity = (int)((uint64_t)rawH * 100 / 1048576);            // %RH
  cachedTempC10  = (int)((uint64_t)rawT * 2000 / 1048576) - 500;     // degC x10
}
#endif

int tempC10() {
#if HAS_TEMP_HUMIDITY
  return cachedTempC10;
#else
  return TELEM_NONE;
#endif
}
int humidityPct() {
#if HAS_TEMP_HUMIDITY
  return cachedHumidity;
#else
  return TELEM_NONE;
#endif
}

// ---------------- SPL (I2S MEMS mic) ----------------
#if HAS_SPL_MIC
static I2SClass i2sMic;
static bool i2sReady = false;
static int cachedSplDbA = TELEM_NONE;
float splCalOffset = 0;   // dB, per-unit; loaded from Preferences (see loadSplCal)

// A-weighting stage. NOT YET FITTED: this is currently a pass-through, so
// splDbA() reports unweighted dB, not true dB(A), until the biquad cascade
// is generated for SPL_SAMPLE_RATE (bilinear transform of the IEC 61672
// A-weighting prototype) and validated against a reference meter on the
// real mic. Kept as a hook so turning on real weighting is a local change.
static float aWeight(float sample) {
  return sample;   // TODO(phase2-hw): replace with the 3-biquad A-weighting cascade
}

static void splInit() {
  i2sMic.setPins(I2S_BCLK, I2S_WS, -1, I2S_DIN, -1);   // bclk, ws, dout(none), din, mclk(none)
  i2sReady = i2sMic.begin(I2S_MODE_STD, SPL_SAMPLE_RATE,
                          I2S_DATA_BIT_WIDTH_32BIT, I2S_SLOT_MODE_MONO);
}

// Reads a block of samples, computes the (optionally A-weighted) RMS, and
// converts to dB with the per-unit calibration offset. The INMP441 packs
// 24-bit audio into the top of a 32-bit slot.
static void splSample() {
  if (!i2sReady) { splInit(); if (!i2sReady) return; }
  const int N = 512;
  static int32_t buf[N];
  size_t got = i2sMic.readBytes((char *)buf, sizeof(buf));
  int n = got / sizeof(int32_t);
  if (n <= 0) return;

  double sumSq = 0;
  for (int i = 0; i < n; i++) {
    float s = (float)(buf[i] >> 8);       // 24-bit sample in the top bits
    s = aWeight(s);
    sumSq += (double)s * s;
  }
  float rms = sqrtf((float)(sumSq / n));
  if (rms < 1.0f) rms = 1.0f;
  // dBFS relative to the 24-bit full scale, then shift by the calibration
  // offset that maps this mic's dBFS onto real SPL.
  float dbfs = 20.0f * log10f(rms / 8388608.0f);   // 2^23
  cachedSplDbA = (int)lroundf(dbfs + splCalOffset);
}
#endif

int splDbA() {
#if HAS_SPL_MIC
  return cachedSplDbA;
#else
  return TELEM_NONE;
#endif
}

#if HAS_SPL_MIC
// Calibration: store the offset that makes splDbA() read `referenceDb`
// right now (hold a calibrated source or meter next to the mic). Persisted
// so it survives reboots.
void calibrateSpl(int referenceDb) {
  int uncal = cachedSplDbA - (int)lroundf(splCalOffset);   // back out the current offset
  splCalOffset = referenceDb - uncal;
  prefs.putFloat("splcal", splCalOffset);
}
static void loadSplCal() { splCalOffset = prefs.getFloat("splcal", 0); }
#endif

// ---------------- service loop ----------------
// Battery on a slow cadence with exponential smoothing (so it doesn't
// jitter on WiFi-TX voltage dips); temp/humidity every few seconds; SPL
// as fast as blocks arrive. All guarded — empty on the classic board.
void serviceSensors() {
#if BATTERY_ADC_PIN >= 0
  {
    static unsigned long last = 0;
    if (millis() - last >= 3000) {
      last = millis();
      int measured = (int)(analogReadMilliVolts(BATTERY_ADC_PIN) * BATTERY_DIVIDER);
      batteryCachedMv = (batteryCachedMv < 0) ? measured
                                              : (batteryCachedMv * 3 + measured) / 4;
    }
  }
#endif
#if HAS_TEMP_HUMIDITY
  {
    static unsigned long last = 0;
    if (millis() - last >= 5000) { last = millis(); ahtSample(); }
  }
#endif
#if HAS_SPL_MIC
  {
    static bool calLoaded = false;
    if (!calLoaded) { loadSplCal(); calLoaded = true; }
    static unsigned long last = 0;
    if (millis() - last >= 250) { last = millis(); splSample(); }
  }
#endif
}
