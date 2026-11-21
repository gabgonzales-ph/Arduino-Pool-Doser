# PoolDoserTest

Bench test for every pin and component. Same `pins.h` / `config.h` as the main sketch (re-copy them here whenever you change them in PoolDoser).

1. Disconnect pumps, mixer and chemicals.
2. Put this folder in your Arduino sketchbook (folder name must be `PoolDoserTest`), open `PoolDoserTest.ino`, select Mega 2560, upload.
3. Serial Monitor: 115200 baud, line ending **Newline**. Type `help`.

| Command | Tests |
|---|---|
| r1..r6, rall | Relays (click and LED, in order) |
| s1, s2 | Servos |
| f | Floats (short the pin to GND to trigger) |
| w, t | Load cell weight / tare |
| p | pH raw, volts and pH |
| l, i | LCD text / I2C scan |
| all | Relays, servos, LCD and I2C in one go |
