#pragma once
#include <Arduino.h>

namespace Sensors {
  void begin();

  // pH: median of PH_SAMPLES readings. Returns NAN if the reading is out of range.
  float readPH();

  // Settle-and-confirm pH check (non-blocking apart from ~150 ms once per second).
  // Call phCheckUpdate() every loop while a check is wanted. It starts a new check by itself
  // when none is running. PHC_READY returns the accepted pH (median of the stable window).
  // PHC_BAD_LIMIT = PH_ERROR_LIMIT consecutive bad samples (caller should FAULT).
  enum PhCheckStatus : uint8_t { PHC_SETTLING, PHC_READY, PHC_BAD_LIMIT };
  PhCheckStatus phCheckUpdate(float &ph);
  void     phCheckReset();         // abandon any running check (next update starts a fresh one)
  bool     phCheckActive();        // true while a check is in progress
  uint32_t phCheckElapsedMs();     // time since the running check started (0 if none)
  float    phCheckLastValue();     // latest pH sample of the running check (NAN if none yet / bad)

  // Floats: call updateFloats() every loop (debounce), then query.
  void updateFloats();
  bool float1Reached();
  bool float2Reached();
  bool acidEmpty();            // true when the acid main container is empty

  // Load cell
  bool tare();                 // false if the HX711 does not respond
  bool readWeight(float &g);   // true if a fresh reading was available
}
