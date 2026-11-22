# Arduino Pool Doser

An Arduino Mega 2560 automatic pool chemical dosing system. It reads pool pH and decides which of two physically separate dosing mechanisms to run: **muriatic acid** (pH too high) or **chlorine powder** (pH too low).

> **Safety:** Acid and chlorine must never mix. Keep their containers, plumbing, mixing containers and discharge lines physically separate. Do not run the system unattended or with real chemicals until it has been fully validated (see [Project status](#project-status)).

*Last updated: November 22, 2026.*

## How it works

| Accepted pH | Action |
|---|---|
| Above 8.2 | Muriatic acid mechanism |
| Below 8.2 | Chlorine mechanism |

- Single 8.2 threshold, no hysteresis.
- Only a *settled and confirmed* pH reading (median of the last 10 samples, taken about once per second, within a stability band) can trigger dosing.
- After a completed dosing cycle, a **cooldown** starts (`COOLDOWN_MS`, intended 60 minutes). The LCD shows the countdown. Cooldown does not persist across power cycles, so the first pH check after power-up runs immediately.
- No cooldown starts after refilling an empty container.

### Muriatic acid cycle
1. Relay 1 pumps acid into the mixing container until Float 1 is reached.
2. Relay 2 adds water until Float 2 is reached.
3. Relay 3 dispenses the diluted mixture into the pool on a timer (`ACID_DISPENSE_MS`).

### Chlorine cycle
1. Servo 1 step-feeds chlorine powder onto the load-cell platform until the target weight (255 g).
2. Servo 2 opens the platform flapper and drops the powder into the mixing container.
3. Relay 4 (water pump B) and Relay 5 (mixer) run simultaneously on separate timers.
4. Relay 6 pumps the mixture into the pool once **both** have finished.

Current timings: water pump B 70 s, mixer 15 min, dispensing pump B 1 min 30 s. In bench testing the powder took about 12 minutes to dissolve, so the mixer time keeps a margin.

### Empty-container handling
- **Acid empty** (float on A10):
  - At IDLE (or right after pH acceptance, before R1 starts): blocks both mechanisms. After refill the system returns to IDLE and a fresh pH check follows.
  - **During `ACID_FILL`: pause and resume.** R1 stops and the fill time used so far is saved. After a confirmed refill and `ACID_REFILLED_MS`, R1 restarts and the cycle continues with the **same accepted pH reading** (no new pH check). The fill timeout continues from the saved time. If the float reads empty again during the delay, the resume is cancelled.
  - Empty during `ACID_WATER` or `ACID_DISPENSE` is not handled specially.
- **Chlorine empty** (HC-SR04 ultrasonic, 5 confirmations at 0.5 s): blocks both mechanisms at IDLE. During `CL_FEED` it pauses feeding, holds the current weight (no re-tare) and pauses the feed timeout. After a confirmed refill it resumes toward the target weight. If empty is detected again during the 3-second refill confirmation, the resume is cancelled.
- Empty during later chlorine stages (`CL_DROP` through `CL_DISPENSE`) does not interrupt the cycle.

## Hardware

| Component | Purpose |
|---|---|
| Arduino Mega 2560 | Main controller |
| PH-4502C + E201-BNC electrode | pH sensing (electrode replaced Nov 22, 2026, recalibrated) |
| HX711 + two 1 kg load cells | Chlorine powder weighing (both cells feed one HX711) |
| MG996R Servo 1 | Chlorine hopper flapper (continuous rotation) |
| MG996R Servo 2 | Weighing platform flapper |
| Float switches (x3) | Mixing-container levels, acid-empty |
| HC-SR04 | Chlorine container level |
| 8-channel relay module | Pumps and mixer |
| 16x2 I2C LCD | Status display |
| Buck converters, 12 V supply | Power |

### Relay mapping

Relay output array in firmware: `{9, 7, 6, 8, 10, 11}`

| Relay | Arduino pin | Module input | Load |
|---|---|---|---|
| 1 | D9 | IN6 | Muriatic acid pump (acid IN) |
| 2 | D7 | IN4 | Water pump A (acid water) |
| 3 | D6 | IN3 | Acid dispensing pump |
| 4 | D8 | IN5 | Water pump B (chlorine water) |
| 5 | D10 | IN7 | Mixer motor |
| 6 | D11 | IN8 | Chlorine dispensing pump B |

IN1 and IN2 are unused. Test each relay with all loads disconnected before wiring pumps.

### Pin assignments

| Signal | Pin |
|---|---|
| HX711 SCK / DT | D2 / D3 |
| Servo 2 / Servo 1 | D4 / D5 |
| Relays | D6-D11 (D12, D13 reserved) |
| pH sensor (Po) | A0 |
| LCD SDA / SCL | D20 / D21 |
| Float 1 / Float 2 | D17 / D16 |
| Acid-empty float | A10 |
| Ultrasonic trigger / echo | A9 / A8 |

## Calibration

### pH (new electrode, Nov 22, 2026)

Three-point calibration at about 25 °C. Average voltages measured on the new electrode:

| Buffer | Voltage |
|---|---|
| pH 4.01 | 3.5285 V |
| pH 6.86 | 3.1293 V |
| pH 9.18 | 2.8560 V |

```
pH = (-7.64680 x Voltage) + 30.93342
```

```c
#define PH_SLOPE  -7.64680f
#define PH_OFFSET 30.93342f
```

Fit error is within about 0.14 pH. The 9.18 reading was noisy (about 0.05 V spread), so recheck it once the probe is settled. The previous electrode used `-7.46641` / `31.67465`; those values do not fit the new electrode (about 1.2 to 1.5 pH too high).

Voltages were read with a sketch that averages 20 ADC reads on A0 and converts with `ADC x 5.0 / 1023`, the same conversion the firmware uses. Calibrate on the same power supply you will run on, because the ADC reference is the Mega's 5 V rail.

### Load cell (Nov 22, 2026)

Calibrated with a known 210 g weight using the load cell calibration sketch (`LoadCellCalibration/`):

```c
#define LOADCELL_SCALE 1174.62f
```

Verify with other known weights (for example 100 g and about 255 g) and confirm it returns to about 0 g when empty. Redo the calibration if the platform, container or mounting changes.

## Firmware layout

```
PoolDoser/
├── PoolDoser.ino        # Main program, non-blocking state machine
├── config.h             # Timers, thresholds, pH calibration
├── pins.h               # Pin assignments
├── sensors.cpp/.h       # pH, float, load-cell processing
├── actuators.cpp/.h     # Relays and servos
├── display.cpp/.h       # LCD output
├── chlorine_level.cpp/.h# Ultrasonic level monitoring
├── level_config.h       # Chlorine level settings
├── servo1_config.h      # Servo 1 feed settings
└── PoolDoserTest/       # Standalone component test sketch
```

**State machine:** `IDLE`, `ACID_FILL`, `ACID_WATER`, `ACID_DISPENSE`, `CL_FEED`, `CL_DROP`, `CL_SETTLE`, `CL_MIX`, `CL_DISPENSE`, `ACID_EMPTY`, `CL_EMPTY`, `FAULT`.

**Safety features:** all relays off at boot; acid/chlorine relay interlock; timeout on every waiting step; `FAULT` turns everything off and shows the reason on the LCD (cleared by reset); three consecutive bad pH samples or an unresponsive HX711 trigger a fault; platform-not-empty check after the chlorine drop; Servo 1 stops at target weight.

### Servo 1 settings
Stop 90, spin 180, normal pulse 300 ms, slow pulse 100 ms, pause 700 ms, target 255 g. These are set in `servo1_config.h`.

## config.h configuration guide

All times are in **milliseconds** and written with a `UL` suffix (for example `60000UL`). Values marked *test* are temporary. All values are samples: calibrate on the real hardware.

### Time conversion cheat sheet

| Time | Value |
|---|---|
| 1 second | `1000UL` |
| 5 seconds | `5000UL` |
| 30 seconds | `30000UL` |
| 1 minute | `60000UL` |
| 90 seconds | `90000UL` |
| 2 minutes | `120000UL` |
| 3 minutes | `180000UL` |
| 3 min 20 s | `200000UL` |
| 9 minutes | `540000UL` |
| 10 minutes | `600000UL` |
| 12 minutes | `720000UL` |
| 15 minutes | `900000UL` |
| 60 minutes | `3600000UL` |

Formula: minutes x 60 x 1000.

### Hardware options

| Setting | Current | What it does |
|---|---|---|
| `SERIAL_BAUD` | 115200 | Serial Monitor speed. |
| `RELAY_ACTIVE_LOW` | 1 | 1 if the relay board turns ON with a LOW signal. |
| `FLOAT_ACTIVE_LOW` | 0 | Floats 1 and 2: 1 = "reached" when the pin reads LOW, 0 = when HIGH (switch open). |
| `EMPTY_ACTIVE_LOW` | 1 | Acid-empty float: 1 = EMPTY when the pin reads LOW, 0 = when HIGH. Test the float physically; the history conflicts. |
| `USE_I2C_LCD` | 1 | 1 = I2C backpack, 0 = parallel LCD (see `pins.h`). |
| `LCD_I2C_ADDR` | 0x27 | I2C address. Common alternative: 0x3F. |
| `USE_WATCHDOG` | 0 | Enable only after reading the notes (old Mega bootloaders can boot-loop). |
| `BOOT_TITLE_MS` | 5000UL | How long the boot title stays on the LCD. |

### pH

| Setting | Current | What it does |
|---|---|---|
| `PH_THRESHOLD` | 8.2f | Above = acid, below = chlorine. |
| `PH_SLOPE`, `PH_OFFSET` | see [pH calibration](#ph-new-electrode-nov-22-2026) | `pH = PH_SLOPE x volts + PH_OFFSET`. Recalibrate whenever the electrode changes. |
| `PH_SAMPLES` | 15 | ADC reads per pH sample (median of N, about 150 ms). |
| `PH_SAMPLE_DELAY_MS` | 10 | Delay between those ADC reads. |
| `PH_CHECK_INTERVAL_MS` | 5000UL | Pause before the next pH check when the last one did not start a dose. |
| `PH_ERROR_LIMIT` | 3 | Consecutive bad samples before FAULT. |

### pH settle-and-confirm

A check takes one sample every `PH_SAMPLE_INTERVAL_MS`. It is accepted only when (1) at least `PH_SETTLE_MS` have passed, (2) the last `PH_WINDOW_SAMPLES` samples are all valid, and (3) their spread (max minus min) is at most `PH_STABLE_BAND`. The accepted pH is the median of that window. There is no timeout: an unstable probe keeps sampling and never doses.

| Setting | Current | What it does |
|---|---|---|
| `PH_SETTLE_MS` | 10000UL (*test*) | Minimum settle time. Intended value: 60000UL. |
| `PH_SAMPLE_INTERVAL_MS` | 1000UL | One sample per second. |
| `PH_WINDOW_SAMPLES` | 10 | Samples in the stability window (max 20). |
| `PH_STABLE_BAND` | 1.00f | Allowed max-minus-min spread in the window. 1.00 is very loose (*test*). Intended about 0.10 to 0.15; one ADC count is about 0.036 pH. |
| `COOLDOWN_MS` | 600000UL | Wait after each dose. This is 10 minutes. The intended 60 minutes is `3600000UL`. |

### Acid mechanism

| Setting | Current | What it does |
|---|---|---|
| `ACID_FILL_TIMEOUT_MS` | 360000UL (6 min) | R1 (acid IN pump) maximum run while waiting for Float 1. FAULT if exceeded. The timeout does not run while paused for an empty container. |
| `ACID_WATER_TIMEOUT_MS` | 360000UL (6 min) | R2 (water pump A) maximum run while waiting for Float 2. FAULT if exceeded. |
| `ACID_DISPENSE_MS` | 200000UL (3 min 20 s) | R3 (acid dispensing pump) fixed run time. |
| `ACID_REFILLED_MS` | 3000UL | After a refill during `ACID_FILL` is confirmed, wait this long before R1 restarts. 0 = resume at once. |
| `FLOAT_DEBOUNCE_MS` | 300UL | Float debounce. Affects when the floats read as reached or empty. |

R1 and R2 stop early when their float is reached. R3 always runs its full time.

### Chlorine mechanism

| Setting | Current | What it does |
|---|---|---|
| `TARGET_WEIGHT_G` | 255.0f | Powder weight to feed. |
| `SLOW_BAND_G` | 20.0f | Within this many grams of the target, Servo 1 switches to short pulses. |
| `PULSE_OPEN_MS`, `PULSE_SETTLE_MS` | 150UL, 500UL | Present in `config.h` but not used by `PoolDoser.ino`. Servo 1 step timing is in `servo1_config.h`. |
| `CL_FEED_TIMEOUT_MS` | 90000UL | Maximum time to reach the target weight. Does not run while `CL_EMPTY` pauses the feed. |
| `CL_REFILLED_MS` | 3000UL | Refill confirmation time before feeding resumes. |
| `LOADCELL_SCALE` | see [Load cell](#load-cell-nov-22-2026) | Counts per gram. Recalibrate with a known weight. |
| `SERVO_CLOSED_DEG`, `SERVO_OPEN_DEG` | 0, 90 | Servo 2 closed and open angles. |
| `SERVO2_OPEN_MS` | 4000UL | Servo 2 open time (drop onto the mixing container). |
| `CL_SETTLE_MS` | 1500UL | Wait after Servo 2 closes before the platform check. |
| `CHECK_PLATFORM_EMPTY` | 1 | FAULT if the platform is not empty after the drop. |
| `PLATFORM_EMPTY_TOL_G` | 100.0f | Platform weight above this counts as not empty. |
| `CL_R4_MS` | 70000UL (70 s) | Relay 4 water pump B run time (timer only, no float). |
| `CL_R5_MS` | 900000UL (15 min) | Relay 5 mixer motor run time. Must be longer than the dissolve time. |
| `CL_R6_MS` | 90000UL (1 min 30 s) | Relay 6 dispensing pump B run time. Starts after BOTH R4 and R5 finish. |

Tuning notes:
- If leftover mixture stays in the mixing container after R6 finishes, lengthen `CL_R6_MS` or shorten `CL_R4_MS` a few seconds at a time. Leftovers build up over cycles and can overflow.
- Setting a timer to `0UL` still switches that relay on for a few milliseconds. To skip a pump completely, use the switches below.

### Test switches

| Setting | Normal | What it does |
|---|---|---|
| `CL_SKIP_FEED` | 0 | 1 = skip Servo 1 feeding, the Servo 2 drop and the platform check, and go straight to mixing (R4 and R5). Use when the powder is already in the mixing container, for example to time how long it takes to dissolve. |
| `CL_DISPENSE_ENABLED` | 1 | 0 = Relay 6 never turns on and the cycle ends after mixing. Use when you do not want the mixture pumped into the pool. |

Both switches are for bench testing only. Set them back to the normal values before real operation. The cooldown still starts after the cycle ends.

## Setup

**Libraries**
- [HX711](https://github.com/bogde/HX711) by bogde
- LiquidCrystal_I2C by Frank de Brabander
- `Servo` and `Wire` (built in)

1. Open `PoolDoser/PoolDoser.ino` in the Arduino IDE and select **Arduino Mega or Mega 2560**.
2. Install the libraries above.
3. Confirm `config.h` values (pH constants, `LOADCELL_SCALE`, `PH_STABLE_BAND`, float polarity, cooldown, test switches) before uploading.
4. Upload, then bench test **without chemicals or pumps connected**.

## Testing

`PoolDoserTest/` is a separate sketch for testing components independently. Serial Monitor: 115200 baud, newline line ending.

| Command | Function |
|---|---|
| `r1` to `r6`, `rall` | Test individual / all relays |
| `s1`, `s2` | Test Servo 1 / Servo 2 |
| `f`, `w` | Float and level inputs |
| `t`, `p`, `l`, `i` | Timer, pH, load cell, input status |
| `all` | General test |

`LoadCellCalibration/` is a separate sketch for calibrating the load cell. Commands: `t` (tare), `c <grams>` (calibrate with a known weight), `s <factor>` (set scale), `r` (raw reading), `v` (toggle live view).

A Wokwi simulation also exists with shortened timers. Its simulated load cell only reaches 50 g, so lower the target weight there.

## LCD messages (16x2)

| State | Line 1 | Line 2 |
|---|---|---|
| Reading pH | `READING PH` | `PH: 4.67` |
| Idle | `IDLE` | `PH 7.35` |
| Cooldown | `COOLDOWN 59:42` | `LAST PH 7.35` |
| Acid dosing | `DOSING: M. ACID` | `FILL ACID` / `ADD WATER` / `DISPENSING` |
| Chlorine dosing | `DOSING: CHLORINE` | `W: 143/255g` / `DROPPING` / `MIXING` / `DISPENSING` |
| Empty | `EMPTY: ACID` / `EMPTY:CHLORINE` / `EMPTY:CONTAINERS` | `REFILL` (or `W: 143g` mid-feed) |
| Refilled (acid or chlorine) | `REFILLED` | `RESUMING...` |
| Fault | e.g. `PH READ FAIL` | `RESET TO CLEAR` |

## Project status

Implemented in source: pH settle-and-confirm (10 s window), Servo 1 step feeding, chlorine empty pause/resume, acid empty pause/resume during `ACID_FILL`, LCD status displays, bench test switches.

Recent changes (Nov 22, 2026):
- Acid pause and resume during `ACID_FILL` (new `ACID_REFILLED_MS`).
- Test switches `CL_SKIP_FEED` and `CL_DISPENSE_ENABLED`.
- Timings updated: acid fill and water timeouts 6 min, acid dispense 3 min 20 s, chlorine water 70 s, mixer 15 min, chlorine dispense 1 min 30 s.
- New pH electrode: recalibrated. Load cell calibrated with a known weight.

**Not yet done:**
- The complete sketch has not been compiled or tested on the physical board.
- Make sure `config.h` carries the new `PH_SLOPE`, `PH_OFFSET` and `LOADCELL_SCALE` values from the Calibration section.
- Recheck the pH 9.18 reading (noisy) and verify all three buffers after updating the firmware.
- Final pH settling window of 60 s (currently 10 s) is pending.
- `PH_STABLE_BAND` is 1.00 for testing. Choose the final value (0.10 failed on the old bench at 0.11 spread; 0.15 was proposed) and verify it with the new electrode.
- `COOLDOWN_MS` is 10 minutes in `config.h`; the intended value is 60 minutes (`3600000UL`).
- `EMPTY_ACTIVE_LOW` for the acid-empty float has conflicting history. Test the float physically, including the new acid resume behavior.
- Servo 1 type confirmation and ultrasonic empty-distance calibration (`CL_EMPTY_DISTANCE_CM`).
- Full hardware validation with real pool-water pH checks.

**Ideas under consideration (not approved):** pH hysteresis around 8.2, longer cooldown, max doses per day, circulation-pump interlock, pool-volume-based dosing, no-progress detection during `CL_FEED`.

## Safety checklist

- Use chemical-compatible pumps, tubing and containers.
- Verify relay wiring and interlocks before connecting chemical pumps.
- Independently verify pH calibration with buffers and pool water.
- Validate load-cell weighing and empty/refill behavior before enabling automatic feeding.
- Set the test switches to their normal values before real operation.
- Do not operate unattended until dosing quantities, mixing and lockouts are validated.
