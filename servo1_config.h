#pragma once

// ============================================================
//  Servo 1 (chlorine hopper) is a 360-degree CONTINUOUS ROTATION servo.
//  write(angle) sets speed and direction, not a position:
//    90 = stop,  above 90 = one direction,  below 90 = the other.
//  The farther from the stop value, the faster it spins.
//  CALIBRATE THESE VALUES.
// ============================================================

#define SERVO1_STOP_DEG       90     // value that holds the servo perfectly still (trim: try 85-95 if it creeps)
#define SERVO1_SPIN_DEG       180    // spin direction and speed (120 = gentle; 180 = fastest one way; use e.g. 60 to reverse)

#define SERVO1_STEP_MS        300UL  // spin time per step (normal feeding)
#define SERVO1_STEP_SLOW_MS   100UL  // spin time per step within SLOW_BAND_G of the target weight
#define SERVO1_PAUSE_MS       700UL  // stopped time between steps (powder settles, load cell reads)
