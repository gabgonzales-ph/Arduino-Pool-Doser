# PoolDoser

Arduino Mega pH-dosing controller. `pH > 7.2` runs the acid mechanism, `pH < 7.2` runs the chlorine mechanism.

## Files
| File | Purpose |
|---|---|
| `PoolDoser.ino` | State machine, timeouts, FAULT, LCD status |
| `config.h` | Timers (sample values), thresholds, 255 g target, calibration constants, hardware options |
| `pins.h` | Pin assignments (PLACEHOLDERS, fill in) |
| `sensors.*` | pH (median filter), debounced floats, HX711 load cell |
| `actuators.*` | Relays (with acid/chlorine interlock), servos |
| `display.*` | 16x2 LCD |

## Setup
1. Install Arduino IDE 2.x.
2. Put this folder in your Arduino sketchbook. The folder name `PoolDoser` must match `PoolDoser.ino`.
3. Library Manager: install **HX711** (by Bogdan Necula, "bogde") and **LiquidCrystal I2C** (Frank de Brabander). `Servo`, `Wire` and `LiquidCrystal` are built in.
4. Tools > Board > Arduino Mega or Mega 2560, then select the port. Clone boards may need the CH340 driver.
5. Edit `pins.h` with your real pins.
6. In `config.h` set `RELAY_ACTIVE_LOW`, `USE_I2C_LCD` (and `LCD_I2C_ADDR`), and `FLOAT_ACTIVE_LOW` to match your hardware.
7. Upload. Open the Serial Monitor at 115200 baud to watch states and pH.

## Bring-up checklist (pumps/chemicals disconnected)
1. LCD shows "POOL DOSER". If blank, adjust contrast or try `LCD_I2C_ADDR 0x3F`.
2. Relays: confirm that all six are OFF at boot and that the active level is right.
3. Floats: check that "reached" matches reality (toggle `FLOAT_ACTIVE_LOW` if inverted).
4. Servos: verify `SERVO_OPEN_DEG` / `SERVO_CLOSED_DEG` move the flappers correctly, with the buck converters powering them.
5. Load cell: calibrate `LOADCELL_SCALE` with a known weight.
6. pH probe: calibrate `PH_SLOPE` / `PH_OFFSET` with pH 4/7/10 buffers.
7. Test each sequence with water only, then tune the timers in `config.h`.

## Behavior notes
- pH is read only in IDLE, so it is never re-read mid-sequence.
- Only one mechanism runs at a time. An interlock inside `actuators.cpp` also faults if acid and chlorine relays would overlap.
- Any timeout (float never reached, target weight never reached, bad pH reads, unresponsive HX711) latches FAULT, turns everything off and shows the reason on the LCD. Reset or power-cycle to clear.
- `COOLDOWN_MS` is 0 (no cooldown), as requested. With no cooldown the system re-doses right after each cycle, so set it before running on a real pool.
- `USE_WATCHDOG` is off by default. Some older Mega bootloaders boot-loop after a watchdog reset, so enable it only after checking your board.
