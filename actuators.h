#pragma once
#include <Arduino.h>

enum RelayId : uint8_t {
  R1_ACID_PUMP = 0,   // acid group
  R2_WATER_A,         // acid group
  R3_DISP_A,          // acid group
  R4_WATER_B,         // chlorine group
  R5_MIXER,           // chlorine group
  R6_DISP_B,          // chlorine group
  RELAY_COUNT
};

namespace Actuators {
  void begin();
  // Returns false (and does nothing) if turning ON would break the acid/chlorine interlock.
  bool setRelay(RelayId r, bool on);
  bool isOn(RelayId r);
  void servo1Spin();     // Servo 1 is a continuous-rotation servo: spin at the configured speed
  void servo1Stop();     // and stop (see servo1_config.h)
  void servo2Open();
  void servo2Close();
  // Everything off, both flappers closed.
  void allOff();
}
