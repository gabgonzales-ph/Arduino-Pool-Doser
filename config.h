#pragma once
// ============================================================
//  PoolDoser - configuration
//  All times are in milliseconds. ALL VALUES ARE SAMPLES -
//  calibrate on the real hardware.
// ============================================================

#define SERIAL_BAUD 115200

// ---------------- Hardware options ----------------
#define RELAY_ACTIVE_LOW   1     // 1 = relay board turns ON with LOW signal
#define FLOAT_ACTIVE_LOW   0     // floats 1 and 2: 1 = "reached" when pin reads LOW (closed to GND), 0 = when HIGH (open)
#define EMPTY_ACTIVE_LOW   1     // acid-container EMPTY float: 1 = EMPTY when pin reads LOW, 0 = EMPTY when pin reads HIGH (switch open)
#define USE_I2C_LCD        1     // 1 = I2C backpack, 0 = parallel LCD (see pins.h)
#define LCD_I2C_ADDR       0x27  // common alternatives: 0x3F
#define USE_WATCHDOG       0     // see README before enabling (old Mega bootloaders can boot-loop)
#define BOOT_TITLE_MS      5000UL   // how long the boot title stays on the LCD

// ---------------- pH ----------------
#define PH_THRESHOLD          8.2f
// pH = PH_SLOPE * voltfage + PH_OFFSET   (placeholder values, calibrate with pH 4/7/10 buffers)
// Three-point calibration (4.01, 6.86, 9.18)
#define PH_SLOPE   -7.64680f
#define PH_OFFSET  30.93342f
#define PH_SAMPLES            15          // ADC reads per pH sample (median of N, about 150 ms)
#define PH_SAMPLE_DELAY_MS    10
#define PH_CHECK_INTERVAL_MS  5000UL      // pause before the next pH check when the last one did not start a dose
#define PH_ERROR_LIMIT        3           // consecutive bad pH samples before FAULT

// ---- pH settle-and-confirm (same rule for the boot check and every later check) ----
// A check takes one pH sample every PH_SAMPLE_INTERVAL_MS. It is accepted only when
//   (1) at least PH_SETTLE_MS have passed since the check started,
//   (2) the last PH_WINDOW_SAMPLES samples are all valid, and
//   (3) their spread (max - min) is <= PH_STABLE_BAND.
// The accepted pH is the median of that window. There is no timeout: an unstable
// probe keeps sampling and never doses.
#define PH_SETTLE_MS          10000UL     // TEMPORARY test value. Intended value: 60000UL
#define PH_SAMPLE_INTERVAL_MS 1000UL      // one pH sample per second
#define PH_WINDOW_SAMPLES     10          // samples in the stability window (max 20)
#define PH_STABLE_BAND        1.00f       // max - min allowed in the window (= +/-0.05 pH).
                                          // One ADC count is about 0.036 pH, so 0.10 allows 2 counts.
#define COOLDOWN_MS           600000UL   // wait after each dose (60 minutes)

// ---------------- Acid mechanism ----------------
#define ACID_FILL_TIMEOUT_MS   360000UL    // R1 max run waiting for float 1
#define ACID_WATER_TIMEOUT_MS  360000UL    // R2 max run waiting for float 2
#define ACID_DISPENSE_MS       200000UL    // R3 run time
#define ACID_REFILLED_MS       3000UL      // after a refill during ACID_FILL is confirmed, wait this long before R1 restarts (0 = resume at once)

#define FLOAT_DEBOUNCE_MS      300UL

// ---------------- Chlorine mechanism ----------------
#define TARGET_WEIGHT_G        255.0f       //temporary value, default 255
#define SLOW_BAND_G            20.0f      // within this of target, Servo 1 switches to pulse feeding
#define PULSE_OPEN_MS          150UL
#define PULSE_SETTLE_MS        500UL
#define CL_FEED_TIMEOUT_MS     90000UL    // max time to reach target weight (does    not run while CL_EMPTY pauses the feed)
#define CL_REFILLED_MS         3000UL     // CL_REFILLED message time after a refill is confirmed, before feeding resumes
#define LOADCELL_SCALE  1174.62f
#define SERVO_CLOSED_DEG       0
#define SERVO_OPEN_DEG         90

#define SERVO2_OPEN_MS         4000UL     // Servo 2 open time (drop onto mixing container)
#define CL_SETTLE_MS           1500UL     // wait after Servo 2 closes before checking platform
#define CHECK_PLATFORM_EMPTY   1          // fault if platform not empty after drop
#define PLATFORM_EMPTY_TOL_G   100.0f

#define CL_R4_MS      70000UL     // water pump B: 70 s
#define CL_R5_MS      900000UL    // mixer: 15 min

//for testing dissolving of chlorine with mixer
#define CL_SKIP_FEED  0   // 1 = TEST: skip Servo 1 feeding, Servo 2 drop and platform check; go straight to mixing
#define CL_DISPENSE_ENABLED     1   // 0 = skip Relay 6 (dispensing pump B never turns on)


#define CL_R6_MS      90000UL     // dispensing pump B: 1 min 30 s