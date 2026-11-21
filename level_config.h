#pragma once
#include <Arduino.h>

// ============================================================
//  Chlorine container level sensor (ultrasonic, HC-SR04 style)
//  Mounted on the container cap, facing down at the powder.
//  A larger distance means less chlorine. CALIBRATE THESE VALUES.
// ============================================================

// Pins
const uint8_t PIN_US_TRIG = A9;
const uint8_t PIN_US_ECHO = A8;
// ============================================================
// CHLORINE CONTAINER ULTRASONIC CALIBRATION
// ============================================================

// Empty detection threshold
#define CL_EMPTY_DISTANCE_CM       14.0f

// Refill hysteresis
#define CL_EMPTY_CLEAR_MARGIN_CM   3.0f

// Sampling and filtering
#define CL_US_CHECK_INTERVAL_MS    1000UL
#define CL_US_SAMPLES              5
#define CL_US_SAMPLE_GAP_MS        30UL
#define CL_US_CONFIRM_COUNT        3
#define CL_US_TIMEOUT_US           30000UL

// No echo = EMPTY (safe)
#define CL_NO_ECHO_IS_EMPTY        1

// Serial logging
#define CL_US_LOG                  1