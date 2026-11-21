# Arduino Pool Doser

An Arduino Mega 2560 automatic pool chemical dosing system. It reads pool pH and decides which of two physically separate dosing mechanisms to run: **muriatic acid** (pH too high) or **chlorine powder** (pH too low).

> **Safety:** Acid and chlorine must never mix. Keep their containers, plumbing, mixing containers and discharge lines physically separate. Do not run the system unattended or with real chemicals until it has been fully validated (see [Status](#project-status)).

## How it works

| Accepted pH | Action |
|---|---|
| Above 7.2 | Muriatic acid mechanism |
| Below 7.2 | Chlorine mechanism |

- Single 7.2 threshold, no hysteresis.
- Only a *settled and confirmed* pH reading (median of the last 10 samples, taken about once per second, within a stability band) can trigger dosing.
- After a completed dosing cycle, a **60-minute cooldown** starts. The LCD shows the countdown. Cooldown does not persist across power cycles, so the first pH check after power-up runs immediately.
- No cooldown starts after refilling an empty container.

### Muriatic acid cycle
1. Relay 1 pumps acid into the mixing container until Float 1 is reached.
2. Relay 2 adds water until Float 2 is reached.
3. Relay 3 dispenses the diluted mixture into the pool on a timer.

### Chlorine cycle
1. Servo 1 step-feeds chlorine powder onto the load-cell platform until **255 g**.
2. Servo 2 opens the platform flapper and drops the powder into the mixing container.
3. Relay 4 (water pump B) and Relay 5 (mixer) run simultaneously on separate timers.
4. Relay 6 pumps the mixture into the pool once both have finished.

### Empty-container handling
- **Acid empty** (float on A10): blocks both mechanisms at IDLE and stops the acid pump if it is running.
- **Chlorine empty** (HC-SR04 ultrasonic, 5 confirmations at 0.5 s): blocks both mechanisms at IDLE. During `CL_FEED` it pauses feeding, holds the current weight (no re-tare) and pauses the feed timeout. After a confirmed refill it resumes toward 255 g. If empty is detected again during the 3-second refill confirmation, the resume is cancelled.
- Empty during later chlorine stages (`CL_DROP` through `CL_DISPENSE`) does not interrupt the cycle.

## Hardware

| Component | Purpose |
|---|---|
| Arduino Mega 2560 | Main controller |
| PH-4502C + E201-BNC electrode | pH sensing |
| HX711 + load cell | Chlorine powder weighing |
| MG996R Servo 1 | Chlorine hopper flapper |
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
| 1 | D9 | IN6 | Muriatic acid pump |
| 2 | D7 | IN4 | Water pump A |
| 3 | D6 | IN3 | Acid dispensing pump |
| 4 | D8 | IN5 | Water pump B |
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

## pH calibration

Three-point calibration at 25 °C (pH 4.01 = 3.7065 V, pH 6.86 = 3.3206 V, pH 9.18 = 3.0144 V):

```
pH = (-7.46641 x Voltage) + 31.67465
```

```c
#define PH_SLOPE  -7.46641f
#define PH_OFFSET 31.67465f
```

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
Stop 90, spin 180, normal pulse 300 ms, slow pulse 100 ms, pause 700 ms, target 255 g.

## Setup

**Libraries**
- [HX711](https://github.com/bogde/HX711) by bogde
- LiquidCrystal_I2C by Frank de Brabander
- `Servo` and `Wire` (built in)

1. Open `PoolDoser/PoolDoser.ino` in the Arduino IDE and select **Arduino Mega or Mega 2560**.
2. Install the libraries above.
3. Confirm `config.h` values (pH constants, `PH_STABLE_BAND`, float polarity) before uploading.
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
| Refilled | `REFILLED` | `RESUMING...` |
| Fault | e.g. `PH READ FAIL` | `RESET TO CLEAR` |

## Project status

Implemented in source: pH settle-and-confirm (10 s window), Servo 1 step feeding, chlorine empty pause/resume, LCD status displays.

**Not yet done:**
- Complete sketch has not been compiled or tested on the physical board.
- Final pH settling window of 60 s (currently 10 s) is pending.
- `PH_STABLE_BAND`: 0.10 failed on bench (0.11 spread); 0.15 is proposed, unverified.
- `EMPTY_ACTIVE_LOW` for the acid-empty float has conflicting history. Inspect `config.h` and test the float physically.
- Load cell calibration with a known weight, Servo 1 type confirmation, ultrasonic empty-distance calibration (`CL_EMPTY_DISTANCE_CM`).
- Full hardware validation with real pool-water pH checks.

**Ideas under consideration (not approved):** pH hysteresis around 7.2, longer cooldown, max doses per day, circulation-pump interlock, pool-volume-based dosing, no-progress detection during `CL_FEED`.

## Safety checklist

- Use chemical-compatible pumps, tubing and containers.
- Verify relay wiring and interlocks before connecting chemical pumps.
- Independently verify pH calibration with buffers and pool water.
- Validate load-cell weighing and empty/refill behavior before enabling automatic feeding.
- Do not operate unattended until dosing quantities, mixing and lockouts are validated.
