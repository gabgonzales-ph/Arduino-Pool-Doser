#pragma once
// ============================================================
//  PoolDoser - configuration
//  All times are in milliseconds. ALL VALUES ARE SAMPLES -
//  calibrate on the real hardware.
// ============================================================

#define SERIAL_BAUD 115200

// ---------------- Hardware options ----------------
#define RELAY_ACTIVE_LOW   1     // 1 = relay board turns ON with LOW signal
#define FLOAT_ACTIVE_LOW   1     // 1 = float reads LOW when "reached" (switch to GND, INPUT_PULLUP)
#define USE_I2C_LCD        1     // 1 = I2C backpack, 0 = parallel LCD (see pins.h)
#define LCD_I2C_ADDR       0x27  // common alternatives: 0x3F
#define USE_WATCHDOG       0     // see README before enabling (old Mega bootloaders can boot-loop)

// ---------------- pH ----------------
#define PH_THRESHOLD          7.2f
// pH = PH_SLOPE * voltage + PH_OFFSET   (placeholder values, calibrate with pH 4/7/10 buffers)
#define PH_SLOPE             -5.56f
#define PH_OFFSET            20.89f
#define PH_SAMPLES            15          // median of N samples
#define PH_SAMPLE_DELAY_MS    10
#define PH_CHECK_INTERVAL_MS  5000UL      // how often IDLE re-reads pH
#define PH_ERROR_LIMIT        3           // consecutive bad reads before FAULT
#define COOLDOWN_MS           0UL         // wait after each dose (none for now)

// ---------------- Acid mechanism ----------------
#define ACID_FILL_TIMEOUT_MS   60000UL    // R1 max run waiting for float 1
#define ACID_WATER_TIMEOUT_MS  60000UL    // R2 max run waiting for float 2
#define ACID_DISPENSE_MS       20000UL    // R3 run time

#define FLOAT_DEBOUNCE_MS      300UL

// ---------------- Chlorine mechanism ----------------
#define TARGET_WEIGHT_G        255.0f
#define SLOW_BAND_G            20.0f      // within this of target, Servo 1 switches to pulse feeding
#define PULSE_OPEN_MS          150UL
#define PULSE_SETTLE_MS        500UL
#define CL_FEED_TIMEOUT_MS     90000UL    // max time to reach target weight
#define LOADCELL_SCALE         420.0f     // placeholder, calibrate with a known weight

#define SERVO_CLOSED_DEG       0
#define SERVO_OPEN_DEG         90

#define SERVO2_OPEN_MS         4000UL     // Servo 2 open time (drop onto mixing container)
#define CL_SETTLE_MS           1500UL     // wait after Servo 2 closes before checking platform
#define CHECK_PLATFORM_EMPTY   1          // fault if platform not empty after drop
#define PLATFORM_EMPTY_TOL_G   30.0f

#define CL_R4_MS               15000UL    // Relay 4 water pump B duration (timer only, no float)
#define CL_R5_MS               30000UL    // Relay 5 mixer motor duration
#define CL_R6_MS               20000UL    // Relay 6 dispensing pump B (starts after BOTH R4 and R5 finish)
