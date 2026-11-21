#include <Arduino.h>
#include <HX711.h>
#include <math.h>
#include "config.h"
#include "pins.h"
#include "sensors.h"

namespace Sensors {

static HX711 scale;

struct Debounced {
  bool stable;
  bool lastRaw;
  uint32_t changedAt;
};
static Debounced f1 = {false, false, 0};
static Debounced f2 = {false, false, 0};
static Debounced fe = {false, false, 0};   // acid container empty

static bool rawReached(uint8_t pin) {
  bool level = digitalRead(pin);
  return FLOAT_ACTIVE_LOW ? !level : level;
}

static bool rawEmpty() {
  bool level = digitalRead(PIN_ACID_EMPTY);
  return EMPTY_ACTIVE_LOW ? !level : level;
}

static void updateOne(Debounced &d, bool raw) {
  uint32_t now = millis();
  if (raw != d.lastRaw) {
    d.lastRaw = raw;
    d.changedAt = now;
  }
  if (raw != d.stable && (now - d.changedAt) >= FLOAT_DEBOUNCE_MS) {
    d.stable = raw;
  }
}

void begin() {
  pinMode(PIN_FLOAT1, INPUT_PULLUP);
  pinMode(PIN_FLOAT2, INPUT_PULLUP);
  pinMode(PIN_ACID_EMPTY, INPUT_PULLUP);
  fe.stable = fe.lastRaw = rawEmpty();
  f1.stable = f1.lastRaw = rawReached(PIN_FLOAT1);
  f2.stable = f2.lastRaw = rawReached(PIN_FLOAT2);

  scale.begin(PIN_HX_DT, PIN_HX_SCK);
  scale.set_scale(LOADCELL_SCALE);
  tare();
}

float readPH() {
  int s[PH_SAMPLES];
  for (uint8_t i = 0; i < PH_SAMPLES; i++) {
    s[i] = analogRead(PIN_PH);
    delay(PH_SAMPLE_DELAY_MS);
  }
  // insertion sort -> median
  for (uint8_t i = 1; i < PH_SAMPLES; i++) {
    int v = s[i];
    int j = i - 1;
    while (j >= 0 && s[j] > v) { s[j + 1] = s[j]; j--; }
    s[j + 1] = v;
  }
  int median = s[PH_SAMPLES / 2];
  float volts = median * 5.0f / 1023.0f;
  float ph = PH_SLOPE * volts + PH_OFFSET;
  if (median < 5 || median > 1018 || ph < 0.0f || ph > 14.0f) return NAN;
  return ph;
}

// ---------------- pH settle-and-confirm check ----------------
static float    phWin[PH_WINDOW_SAMPLES];
static uint8_t  phCount = 0;        // valid samples currently in the window
static uint8_t  phHead = 0;         // next slot to write (ring buffer)
static uint8_t  phBadStreak = 0;    // consecutive bad samples
static bool     phRunning = false;
static uint32_t phStart = 0;
static uint32_t phLastSample = 0;
static float    phLastValue = NAN;  // latest sample, for the LCD

void phCheckReset() {
  phRunning = false;
  phCount = 0;
  phHead = 0;
  phBadStreak = 0;
  phLastValue = NAN;
}

bool phCheckActive() { return phRunning; }

float phCheckLastValue() { return phLastValue; }

uint32_t phCheckElapsedMs() { return phRunning ? (millis() - phStart) : 0; }

// Sorts a copy of the window; returns median, and min/max through lo/hi.
static float windowStats(float &lo, float &hi) {
  float s[PH_WINDOW_SAMPLES];
  for (uint8_t i = 0; i < phCount; i++) s[i] = phWin[i];   // order does not matter
  for (uint8_t i = 1; i < phCount; i++) {
    float v = s[i];
    int8_t j = i - 1;
    while (j >= 0 && s[j] > v) { s[j + 1] = s[j]; j--; }
    s[j + 1] = v;
  }
  lo = s[0];
  hi = s[phCount - 1];
  if (phCount & 1) return s[phCount / 2];
  return 0.5f * (s[phCount / 2 - 1] + s[phCount / 2]);
}

PhCheckStatus phCheckUpdate(float &ph) {
  uint32_t now = millis();
  if (!phRunning) {                       // start a new check; first sample right away
    phRunning = true;
    phStart = now;
    phLastSample = now - PH_SAMPLE_INTERVAL_MS;
    phCount = 0;
    phHead = 0;
  }
  if ((now - phLastSample) < PH_SAMPLE_INTERVAL_MS) return PHC_SETTLING;
  phLastSample = now;

  float p = readPH();                     // median of PH_SAMPLES ADC reads (NAN if out of range)
  phLastValue = p;
  if (isnan(p)) {
    phCount = 0;                          // the window must be made of consecutive valid samples
    phHead = 0;
    Serial.println(F("pH sample: bad"));
    if (++phBadStreak >= PH_ERROR_LIMIT) return PHC_BAD_LIMIT;
    return PHC_SETTLING;
  }
  phBadStreak = 0;
  phWin[phHead] = p;
  phHead = (phHead + 1) % PH_WINDOW_SAMPLES;
  if (phCount < PH_WINDOW_SAMPLES) phCount++;
  Serial.print(F("pH sample: "));
  Serial.println(p, 2);

  if ((now - phStart) >= PH_SETTLE_MS && phCount >= PH_WINDOW_SAMPLES) {
    float lo, hi;
    float med = windowStats(lo, hi);
    if ((hi - lo) <= PH_STABLE_BAND) {
      Serial.print(F("pH accepted: "));
      Serial.print(med, 2);
      Serial.print(F("  spread "));
      Serial.println(hi - lo, 2);
      phRunning = false;
      phCount = 0;
      phHead = 0;
      ph = med;
      return PHC_READY;
    }
  }
  return PHC_SETTLING;
}

void updateFloats() {
  updateOne(f1, rawReached(PIN_FLOAT1));
  updateOne(f2, rawReached(PIN_FLOAT2));
  updateOne(fe, rawEmpty());
}

bool float1Reached() { return f1.stable; }
bool float2Reached() { return f2.stable; }
bool acidEmpty()     { return fe.stable; }

bool tare() {
  if (!scale.wait_ready_timeout(1000)) return false;
  scale.tare(10);
  return true;
}

bool readWeight(float &g) {
  if (!scale.is_ready()) return false;
  g = scale.get_units(1);
  return true;
}

}  // namespace Sensors
