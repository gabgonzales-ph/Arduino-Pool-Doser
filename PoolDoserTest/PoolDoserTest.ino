// ============================================================
//  PoolDoser - hardware bench test
//  Uses the same pins.h / config.h as the main sketch.
//  Open Serial Monitor at 115200, line ending "Newline", type "help".
//  RUN WITH PUMPS / CHEMICALS DISCONNECTED.
// ============================================================
#include <Arduino.h>
#include <Wire.h>
#include <Servo.h>
#include <HX711.h>
#include <LiquidCrystal_I2C.h>
#include <math.h>
#include "config.h"
#include "pins.h"

#define RELAY_TEST_MS 1500UL
#define STREAM_MS     15000UL   // streaming tests run this long (any key stops them)

static HX711 scale;
static LiquidCrystal_I2C lcd(LCD_I2C_ADDR, 16, 2);
static bool scaleReady = false;

static const char *RELAY_NAMES[6] = {
  "R1 acid pump", "R2 water pump A", "R3 acid dispensing pump",
  "R4 water pump B", "R5 mixer motor", "R6 dispensing pump B"
};

// ---------------- helpers ----------------
static void relayWrite(uint8_t i, bool on) {
  digitalWrite(PIN_RELAY[i], (on != (bool)RELAY_ACTIVE_LOW) ? HIGH : LOW);
}

static void allRelaysOff() {
  for (uint8_t i = 0; i < 6; i++) relayWrite(i, false);
}

static bool abortRequested() {
  if (Serial.available()) {
    while (Serial.available()) Serial.read();
    Serial.println(F("(stopped)"));
    return true;
  }
  return false;
}

static void help() {
  Serial.println(F("\n=== PoolDoser bench test ==="));
  Serial.println(F("r1..r6  pulse one relay for 1.5 s (listen for the click)"));
  Serial.println(F("rall    pulse relays 1 to 6 in order"));
  Serial.println(F("s1 / s2 move servo 1 / 2 (open, hold, close)"));
  Serial.println(F("f       show float switches (15 s, short a pin to GND to test)"));
  Serial.println(F("w       show load cell weight (15 s)"));
  Serial.println(F("t       tare the load cell"));
  Serial.println(F("p       show pH raw value, volts and pH (15 s)"));
  Serial.println(F("l       LCD test"));
  Serial.println(F("i       I2C scan (find the LCD address)"));
  Serial.println(F("all     run every automatic test (relays, servos, LCD, I2C)"));
  Serial.println(F("help    this list"));
  Serial.println(F("Any key stops a running stream test.\n"));
}

// ---------------- tests ----------------
static void testRelay(uint8_t i) {
  Serial.print(F("Relay ")); Serial.print(i + 1);
  Serial.print(F(" (pin D")); Serial.print(PIN_RELAY[i]);
  Serial.print(F(", ")); Serial.print(RELAY_NAMES[i]);
  Serial.println(F("): ON"));
  relayWrite(i, true);
  delay(RELAY_TEST_MS);
  relayWrite(i, false);
  Serial.println(F("  OFF"));
  delay(500);
}

static void testServo(uint8_t n) {
  Servo sv;
  uint8_t pin = (n == 1) ? PIN_SERVO1 : PIN_SERVO2;
  Serial.print(F("Servo ")); Serial.print(n);
  Serial.print(F(" (pin D")); Serial.print(pin); Serial.println(F("): opening"));
  sv.attach(pin);
  sv.write(SERVO_CLOSED_DEG);
  delay(800);
  sv.write(SERVO_OPEN_DEG);
  delay(1500);
  Serial.println(F("  closing"));
  sv.write(SERVO_CLOSED_DEG);
  delay(1200);
  sv.detach();
  Serial.println(F("  done"));
}

static void testFloats() {
  pinMode(PIN_FLOAT1, INPUT_PULLUP);
  pinMode(PIN_FLOAT2, INPUT_PULLUP);
  Serial.print(F("Floats: F1=D")); Serial.print(PIN_FLOAT1);
  Serial.print(F("  F2=D")); Serial.println(PIN_FLOAT2);
  int last1 = -1, last2 = -1;
  uint32_t start = millis();
  while (millis() - start < STREAM_MS) {
    if (abortRequested()) return;
    int l1 = digitalRead(PIN_FLOAT1);
    int l2 = digitalRead(PIN_FLOAT2);
    if (l1 != last1 || l2 != last2) {
      last1 = l1; last2 = l2;
      bool r1 = FLOAT_ACTIVE_LOW ? !l1 : l1;
      bool r2 = FLOAT_ACTIVE_LOW ? !l2 : l2;
      Serial.print(F("  Float1: ")); Serial.print(r1 ? F("REACHED") : F("not reached"));
      Serial.print(F("   Float2: ")); Serial.println(r2 ? F("REACHED") : F("not reached"));
    }
    delay(20);
  }
  Serial.println(F("  done"));
}

static void tareScale() {
  if (!scale.wait_ready_timeout(1500)) {
    scaleReady = false;
    Serial.println(F("  HX711 NOT RESPONDING (check DT/SCK wiring and 5V/GND)"));
    return;
  }
  scale.tare(10);
  scaleReady = true;
  Serial.println(F("  tared (current load is now 0)"));
}

static void testWeight() {
  if (!scaleReady) tareScale();
  if (!scaleReady) return;
  Serial.print(F("Load cell: DT=D")); Serial.print(PIN_HX_DT);
  Serial.print(F(" SCK=D")); Serial.print(PIN_HX_SCK);
  Serial.print(F(" scale=")); Serial.println(LOADCELL_SCALE, 1);
  uint32_t start = millis();
  while (millis() - start < STREAM_MS) {
    if (abortRequested()) return;
    if (scale.is_ready()) {
      Serial.print(F("  Weight: "));
      Serial.print(scale.get_units(3), 1);
      Serial.print(F(" g   (raw "));
      Serial.print(scale.get_value(1), 0);
      Serial.println(F(")"));
    } else {
      Serial.println(F("  HX711 not ready"));
    }
    delay(500);
  }
  Serial.println(F("  done"));
}

static void testPH() {
  Serial.print(F("pH sensor on pin A0, slope=")); Serial.print(PH_SLOPE, 2);
  Serial.print(F(" offset=")); Serial.println(PH_OFFSET, 2);
  uint32_t start = millis();
  while (millis() - start < STREAM_MS) {
    if (abortRequested()) return;
    long sum = 0;
    for (uint8_t i = 0; i < 10; i++) { sum += analogRead(PIN_PH); delay(5); }
    int raw = sum / 10;
    float volts = raw * 5.0f / 1023.0f;
    float ph = PH_SLOPE * volts + PH_OFFSET;
    Serial.print(F("  raw ")); Serial.print(raw);
    Serial.print(F("   ")); Serial.print(volts, 2);
    Serial.print(F(" V   pH ")); Serial.print(ph, 2);
    if (raw < 5 || raw > 1018 || ph < 0 || ph > 14) Serial.print(F("   (OUT OF RANGE)"));
    Serial.println();
    delay(500);
  }
  Serial.println(F("  done"));
}

static void testLcd() {
  Serial.print(F("LCD at 0x")); Serial.println(LCD_I2C_ADDR, HEX);
  lcd.init();
  lcd.backlight();
  lcd.clear();
  lcd.setCursor(0, 0); lcd.print(F("POOLDOSER TEST"));
  lcd.setCursor(0, 1); lcd.print(F("0123456789ABCDEF"));
  Serial.println(F("  Check the screen shows two lines of text."));
  Serial.println(F("  Blank? adjust the contrast screw, or try 0x3F (use 'i' to scan)."));
}

static void i2cScan() {
  Serial.println(F("I2C scan (SDA=20, SCL=21):"));
  uint8_t found = 0;
  for (uint8_t a = 1; a < 127; a++) {
    Wire.beginTransmission(a);
    if (Wire.endTransmission() == 0) {
      Serial.print(F("  found device at 0x")); Serial.println(a, HEX);
      found++;
    }
  }
  if (!found) Serial.println(F("  nothing found (check SDA on pin 20, SCL on pin 21, VCC, GND)"));
}

// ---------------- main ----------------
void setup() {
  Serial.begin(SERIAL_BAUD);
  // Relays first, OFF level before OUTPUT so nothing glitches on.
  for (uint8_t i = 0; i < 6; i++) {
    digitalWrite(PIN_RELAY[i], RELAY_ACTIVE_LOW ? HIGH : LOW);
    pinMode(PIN_RELAY[i], OUTPUT);
  }
  pinMode(PIN_FLOAT1, INPUT_PULLUP);
  pinMode(PIN_FLOAT2, INPUT_PULLUP);
  Wire.begin();
  scale.begin(PIN_HX_DT, PIN_HX_SCK);
  scale.set_scale(LOADCELL_SCALE);
  help();
}

void loop() {
  if (!Serial.available()) return;
  String cmd = Serial.readStringUntil('\n');
  cmd.trim();
  cmd.toLowerCase();
  if (cmd.length() == 0) return;

  if (cmd == "help" || cmd == "?")      help();
  else if (cmd == "rall") { for (uint8_t i = 0; i < 6; i++) testRelay(i); }
  else if (cmd.length() == 2 && cmd[0] == 'r' && cmd[1] >= '1' && cmd[1] <= '6')
                                         testRelay(cmd[1] - '1');
  else if (cmd == "s1")                  testServo(1);
  else if (cmd == "s2")                  testServo(2);
  else if (cmd == "f")                   testFloats();
  else if (cmd == "w")                   testWeight();
  else if (cmd == "t")                   tareScale();
  else if (cmd == "p")                   testPH();
  else if (cmd == "l")                   testLcd();
  else if (cmd == "i")                   i2cScan();
  else if (cmd == "all") {
    i2cScan(); testLcd();
    for (uint8_t i = 0; i < 6; i++) testRelay(i);
    testServo(1); testServo(2);
    Serial.println(F("Automatic tests finished. Run f, w and p for the sensors."));
  }
  else { Serial.print(F("Unknown command: ")); Serial.println(cmd); }

  allRelaysOff();
  Serial.println(F("\nReady."));
}
