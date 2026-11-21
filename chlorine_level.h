#pragma once
#include <Arduino.h>

namespace ChlorineLevel {
  void begin();               // sets up pins and settles the first state before dosing starts
  void update();              // call every loop(); measures once per CL_US_CHECK_INTERVAL_MS
  bool isEmpty();             // chlorine container empty
  float lastDistanceCm();     // last measured distance, or -1 if there was no echo
}
