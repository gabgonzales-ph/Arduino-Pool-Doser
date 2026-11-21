#include <Arduino.h>
#include "config.h"
#include "level_config.h"
#include "chlorine_level.h"

namespace ChlorineLevel {

static bool     empty = false;
static float    lastCm = -1.0f;
static uint32_t lastCheck = 0;
static uint8_t  confirm = 0;

// One ping. Returns distance in cm, or -1 when no echo came back.
static float pingOnce() {
  digitalWrite(PIN_US_TRIG, LOW);
  delayMicroseconds(4);
  digitalWrite(PIN_US_TRIG, HIGH);
  delayMicroseconds(10);
  digitalWrite(PIN_US_TRIG, LOW);
  unsigned long us = pulseIn(PIN_US_ECHO, HIGH, CL_US_TIMEOUT_US);
  if (us == 0) return -1.0f;
  return us / 58.0f;   // round trip: about 58 microseconds per cm
}

// Median of CL_US_SAMPLES pings (ignoring pings with no echo). -1 if none came back.
static float measure() {
  float v[CL_US_SAMPLES];
  uint8_t n = 0;
  for (uint8_t i = 0; i < CL_US_SAMPLES; i++) {
    float c = pingOnce();
    if (c > 0) v[n++] = c;
    if (i < CL_US_SAMPLES - 1) delay(CL_US_SAMPLE_GAP_MS);
  }
  if (n == 0) return -1.0f;
  for (uint8_t i = 1; i < n; i++) {            // insertion sort
    float x = v[i];
    int j = i - 1;
    while (j >= 0 && v[j] > x) { v[j + 1] = v[j]; j--; }
    v[j + 1] = x;
  }
  return v[n / 2];
}

static void takeReading() {
  float cm = measure();
  lastCm = cm;

  bool nowEmpty;
  if (cm < 0) {
    nowEmpty = CL_NO_ECHO_IS_EMPTY ? true : empty;                  // no echo
  } else if (empty) {
    nowEmpty = !(cm <= CL_EMPTY_DISTANCE_CM - CL_EMPTY_CLEAR_MARGIN_CM);  // hysteresis on refill
  } else {
    nowEmpty = (cm >= CL_EMPTY_DISTANCE_CM);
  }

  if (nowEmpty != empty) {
    if (++confirm >= CL_US_CONFIRM_COUNT) {
      empty = nowEmpty;
      confirm = 0;
      Serial.print(F("Chlorine container "));
      Serial.println(empty ? F("EMPTY") : F("refilled"));
    }
  } else {
    confirm = 0;
  }

#if CL_US_LOG
  Serial.print(F("Chlorine distance: "));
  if (cm < 0) Serial.println(F("no echo"));
  else { Serial.print(cm, 1); Serial.println(F(" cm")); }
#endif
}

void begin() {
  pinMode(PIN_US_TRIG, OUTPUT);
  digitalWrite(PIN_US_TRIG, LOW);
  pinMode(PIN_US_ECHO, INPUT);
  // Settle the starting state now, so dosing can never begin before the level is known.
  for (uint8_t i = 0; i < CL_US_CONFIRM_COUNT; i++) {
    takeReading();
    delay(60);
  }
  lastCheck = millis();
}

void update() {
  if (millis() - lastCheck < CL_US_CHECK_INTERVAL_MS) return;
  lastCheck = millis();
  takeReading();
}

bool  isEmpty()        { return empty; }
float lastDistanceCm() { return lastCm; }

}  // namespace ChlorineLevel
