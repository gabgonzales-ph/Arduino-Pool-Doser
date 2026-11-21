#pragma once
#include <Arduino.h>
#include "config.h"

// ============================================================
//  PoolDoser - pin assignments (Arduino Mega 2560)
//  Pins from the soldered board. Floats on D17 (float 1), D16 (float 2).
//  Avoid 0/1 (USB serial). I2C LCD is fixed: SDA = 20, SCL = 21.
// ============================================================

// Sensors
const uint8_t PIN_PH      = A0;   // PH-4502C analog out (Po)
const uint8_t PIN_FLOAT1  = 17;   // acid container, first float (acid level)
const uint8_t PIN_FLOAT2  = 16;   // acid container, second float (water level)
const uint8_t PIN_HX_DT   = 3;    // HX711 DT
const uint8_t PIN_HX_SCK  = 2;    // HX711 SCK

// Servos (PWM pins)
const uint8_t PIN_SERVO1  = 5;    // chlorine hopper flapper
const uint8_t PIN_SERVO2  = 4;    // 255 g platform flapper

// Relays, in order: R1 acid pump, R2 water pump A, R3 dispensing pump A,
//                   R4 water pump B, R5 mixer motor, R6 dispensing pump B
// Pumps/mixer sit on module channels IN3..IN8 (IN1, IN2 unused).
// R1->IN3 (D8), R2->IN4 (D9), R3->IN5 (D10), R4->IN6 (D11), R5->IN7 (D12), R6->IN8 (D13)
const uint8_t PIN_RELAY[6] = {9, 7, 6, 8, 10, 11};

// Parallel LCD only (used when USE_I2C_LCD == 0)
const uint8_t PIN_LCD_RS = 40;
const uint8_t PIN_LCD_EN = 41;
const uint8_t PIN_LCD_D4 = 42;
const uint8_t PIN_LCD_D5 = 43;
const uint8_t PIN_LCD_D6 = 44;
const uint8_t PIN_LCD_D7 = 45;
