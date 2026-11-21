#include <Arduino.h>
#include <Servo.h>
#include "config.h"
#include "pins.h"
#include "actuators.h"
#include "servo1_config.h"

namespace Actuators {

static bool relayState[RELAY_COUNT];
static Servo servo1, servo2;

static bool isAcid(RelayId r)     { return r <= R3_DISP_A; }
static bool groupActive(bool acid) {
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    if (isAcid((RelayId)i) == acid && relayState[i]) return true;
  }
  return false;
}

static void writeRelay(uint8_t i, bool on) {
  digitalWrite(PIN_RELAY[i], (on != (bool)RELAY_ACTIVE_LOW) ? HIGH : LOW);
  relayState[i] = on;
}

void begin() {
  // Write the OFF level before switching to OUTPUT so relays never glitch on at boot.
  for (uint8_t i = 0; i < RELAY_COUNT; i++) {
    digitalWrite(PIN_RELAY[i], RELAY_ACTIVE_LOW ? HIGH : LOW);
    pinMode(PIN_RELAY[i], OUTPUT);
    relayState[i] = false;
  }
  servo1.attach(PIN_SERVO1);
  servo2.attach(PIN_SERVO2);
  servo1Stop();
  servo2Close();
}

bool setRelay(RelayId r, bool on) {
  if (on) {
    // Interlock: never run an acid-group relay while a chlorine-group relay is on, and vice versa.
    if (groupActive(!isAcid(r))) return false;
  }
  writeRelay(r, on);
  return true;
}

bool isOn(RelayId r) { return relayState[r]; }

void servo1Spin()  { servo1.write(SERVO1_SPIN_DEG); }
void servo1Stop()  { servo1.write(SERVO1_STOP_DEG); }
void servo2Open()  { servo2.write(SERVO_OPEN_DEG); }
void servo2Close() { servo2.write(SERVO_CLOSED_DEG); }

void allOff() {
  for (uint8_t i = 0; i < RELAY_COUNT; i++) writeRelay(i, false);
  servo1Stop();
  servo2Close();
}

}  // namespace Actuators
