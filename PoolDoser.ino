// ============================================================
//  PoolDoser - pH-controlled acid / chlorine dosing (Arduino Mega)
//  pH > PH_THRESHOLD -> acid mechanism
//  pH < PH_THRESHOLD -> chlorine mechanism
//  Non-blocking millis() state machine, per-step timeouts, FAULT state.
// ============================================================
#include <Arduino.h>
#include <math.h>
#include "config.h"
#include "pins.h"
#include "sensors.h"
#include "actuators.h"
#include "display.h"
#include "chlorine_level.h"
#include "servo1_config.h"
#if USE_WATCHDOG
  #include <avr/wdt.h>
#endif

enum State : uint8_t {
  ST_IDLE,
  ST_ACID_FILL,      // R1 on until float 1
  ST_ACID_WATER,     // R2 on until float 2
  ST_ACID_DISPENSE,  // R3 on (timed)
  ST_CL_FEED,        // Servo 1 feeds until 255 g
  ST_CL_DROP,        // Servo 2 open (timed)
  ST_CL_SETTLE,      // Servo 2 closed, check platform empty
  ST_CL_MIX,         // R4 + R5 in parallel, separate timers
  ST_CL_DISPENSE,    // R6 on (timed), after BOTH R4 and R5 finished
  ST_ACID_EMPTY,     // acid container empty: ALL dosing blocked until refilled
  ST_CL_EMPTY,       // chlorine container empty: ALL dosing blocked until refilled
  ST_FAULT
};

static const char *STATE_NAMES[] = {
  "IDLE", "ACID_FILL", "ACID_WATER", "ACID_DISPENSE",
  "CL_FEED", "CL_DROP", "CL_SETTLE", "CL_MIX", "CL_DISPENSE", "ACID_EMPTY", "CL_EMPTY", "FAULT"
};

static State    state = ST_IDLE;
static uint32_t stateStart = 0;
static uint32_t cycleEnd = 0;
static uint32_t lastPhCheck = 0;
static uint32_t lastDisplay = 0;
static float    lastPH = NAN;
static float    lastWeight = 0;
static uint32_t lastWeightLog = 0;
static const char *faultL1 = "";       // FAULT screen line 1 (what failed)
static const char *faultL2 = "";       // FAULT screen line 2

// chlorine mixing flags
static bool r4Done = false, r5Done = false;
// Servo 1 step feeding (continuous-rotation servo: spin a step, stop, pause, repeat)
static bool     servo1Spinning = false;
static uint32_t stepStart = 0;
// Chlorine container empty DURING feeding: the cycle pauses (Servo 1 stopped, weight held,
// no re-tare) and resumes after the refill.
static bool     clPaused = false;      // true when ST_CL_EMPTY was entered from ST_CL_FEED
static bool     clRefilled = false;    // refill confirmed, CL_REFILLED delay running
static uint32_t clRefilledAt = 0;
static uint32_t clFeedElapsed = 0;     // feed time used before the pause (feed timeout does not run while paused)
// Acid container empty DURING ACID_FILL: the cycle pauses (R1 off) and resumes after the refill,
// using the same accepted pH reading (no new pH check).
static bool     acidPaused = false;    // true when ST_ACID_EMPTY was entered from ST_ACID_FILL
static bool     acidRefilled = false;  // refill confirmed, ACID_REFILLED_MS delay running
static uint32_t acidRefilledAt = 0;
static uint32_t acidFillElapsed = 0;   // fill time used before the pause (fill timeout does not run while paused)

static uint32_t elapsed() { return millis() - stateStart; }

// true while the cooldown that follows a FINISHED dosing cycle is still running
static bool cooldownActive() {
  return COOLDOWN_MS > 0 && (millis() - cycleEnd) < COOLDOWN_MS;
}

static void enter(State s) {
  state = s;
  stateStart = millis();
  if (s == ST_IDLE) Sensors::phCheckReset();   // every return to IDLE starts the next pH check fresh
  Serial.print(F("-> "));
  Serial.println(STATE_NAMES[s]);
}

// line1/line2 are shown on the LCD (max 16 characters each).
static void fault(const char *line1, const char *line2 = "RESET TO CLEAR") {
  Actuators::allOff();
  faultL1 = line1;
  faultL2 = line2;
  Serial.print(F("FAULT: "));
  Serial.println(line1);
  enter(ST_FAULT);
}

// Relay helper: an interlock violation is a fault.
static bool relay(RelayId r, bool on) {
  if (!Actuators::setRelay(r, on)) {
    fault("INTERLOCK");
    return false;
  }
  return true;
}

// The cooldown starts ONLY here, when a whole acid or chlorine process has ended.
static void finishCycle() {
  cycleEnd = millis();
  enter(ST_IDLE);
}

// ---------------- sequence starters ----------------
static void startAcid() {
  if (Sensors::acidEmpty()) {
    Serial.println(F("Acid container EMPTY - acid dosing blocked"));
    Actuators::allOff();
    acidPaused = false;                      // found before the cycle started: nothing to resume
    enter(ST_ACID_EMPTY);
    return;
  }
  Serial.println(F("pH high -> acid"));
  if (!relay(R1_ACID_PUMP, true)) return;
  enter(ST_ACID_FILL);
}

static void startChlorine() {
  Serial.println(F("pH low -> chlorine"));
#if CL_SKIP_FEED
  Serial.println(F("TEST: feed/drop skipped, going straight to mixing"));
  if (!relay(R4_WATER_B, true)) return;
  if (!relay(R5_MIXER, true)) return;
  r4Done = r5Done = false;
  enter(ST_CL_MIX);
#else
  Actuators::servo2Close();
  if (!Sensors::tare()) { fault("HX711 NO RESP"); return; }
  lastWeight = 0;
  Actuators::servo1Spin();
  servo1Spinning = true;
  stepStart = millis();
  enter(ST_CL_FEED);
#endif
}

// ---------------- display ----------------
static void updateDisplay() {
  if (millis() - lastDisplay < 300) return;
  lastDisplay = millis();

  char l1[20], l2[20], ph[8];
  if (isnan(lastPH)) strcpy(ph, "--");
  else dtostrf(lastPH, 4, 2, ph);          // e.g. "7.35"

  l2[0] = '\0';
  switch (state) {
    case ST_IDLE:
      if (cooldownActive()) {
        // countdown MM:SS, starts at 60:00 and runs down to 00:00
        uint32_t left = (COOLDOWN_MS - (millis() - cycleEnd) + 999UL) / 1000UL;
        bool acidE = Sensors::acidEmpty();
        bool clE   = ChlorineLevel::isEmpty();
        if (acidE || clE) {
          // an empty container overlays the timer (the cooldown keeps counting underneath)
          if (acidE && clE) snprintf(l1, sizeof(l1), "EMPTY:CONTAINERS");
          else if (clE)     snprintf(l1, sizeof(l1), "EMPTY: CHLORINE");
          else              snprintf(l1, sizeof(l1), "EMPTY: ACID");
          snprintf(l2, sizeof(l2), "REFILL");
        } else {
          snprintf(l1, sizeof(l1), "COOLDOWN %02lu:%02lu",
                   (unsigned long)(left / 60UL), (unsigned long)(left % 60UL));
          snprintf(l2, sizeof(l2), "LAST PH %s", ph);
        }
      } else if (Sensors::phCheckActive()) {
        // settle-and-confirm in progress (boot and every check): show the latest pH sample
        char sp[8];
        float sample = Sensors::phCheckLastValue();
        if (isnan(sample)) strcpy(sp, "--");
        else dtostrf(sample, 4, 2, sp);
        snprintf(l1, sizeof(l1), "READING PH");
        snprintf(l2, sizeof(l2), "PH: %s", sp);
      } else {
        snprintf(l1, sizeof(l1), "IDLE");
        snprintf(l2, sizeof(l2), "PH %s", ph);
      }
      break;
    case ST_ACID_FILL:
      snprintf(l1, sizeof(l1), "DOSE: M. ACID");
      snprintf(l2, sizeof(l2), "FILL ACID");
      break;
    case ST_ACID_WATER:
      snprintf(l1, sizeof(l1), "DOSE: M. ACID");
      snprintf(l2, sizeof(l2), "ADD WATER");
      break;
    case ST_ACID_DISPENSE:
      snprintf(l1, sizeof(l1), "DOSE: M. ACID");
      snprintf(l2, sizeof(l2), "DISPENSING");
      break;
    case ST_CL_FEED:
      snprintf(l1, sizeof(l1), "DOSE: CHLORINE");
      snprintf(l2, sizeof(l2), "W: %d/%dg", (int)lastWeight, (int)TARGET_WEIGHT_G);
      break;
    case ST_CL_DROP:
    case ST_CL_SETTLE:                       // settle shows DROPPING too (by design)
      snprintf(l1, sizeof(l1), "DOSE: CHLORINE");
      snprintf(l2, sizeof(l2), "DROPPING");
      break;
    case ST_CL_MIX:
      snprintf(l1, sizeof(l1), "DOSE: CHLORINE");
      snprintf(l2, sizeof(l2), "MIXING");
      break;
    case ST_CL_DISPENSE:
      snprintf(l1, sizeof(l1), "DOSE: CHLORINE");
      snprintf(l2, sizeof(l2), "DISPENSING");
      break;
    case ST_ACID_EMPTY:
      if (acidPaused && acidRefilled) {
        snprintf(l1, sizeof(l1), "REFILLED");
        snprintf(l2, sizeof(l2), "RESUMING...");
      } else {
        snprintf(l1, sizeof(l1), "EMPTY: ACID");
        snprintf(l2, sizeof(l2), "REFILL");
      }
      break;
    case ST_CL_EMPTY:
      if (clPaused && clRefilled) {
        snprintf(l1, sizeof(l1), "REFILLED");
        snprintf(l2, sizeof(l2), "RESUMING...");
      } else {
        snprintf(l1, sizeof(l1), "EMPTY: CHLORINE");
        if (clPaused) snprintf(l2, sizeof(l2), "W: %dg", (int)lastWeight);   // held weight
        else          snprintf(l2, sizeof(l2), "REFILL");
      }
      break;
    case ST_FAULT:
      snprintf(l1, sizeof(l1), "%s", faultL1);
      snprintf(l2, sizeof(l2), "%s", faultL2);
      break;
  }
  Display::show(l1, l2);
}

// ---------------- setup / loop ----------------
void setup() {
  Serial.begin(SERIAL_BAUD);
  Actuators::begin();       // relays OFF first
  Display::begin();
  Display::show(" Pool Dispenser", "");   // title appears right away
  uint32_t titleStart = millis();
  Serial.println(F("PoolDoser starting"));

  Sensors::begin();
  ChlorineLevel::begin();   // settles the chlorine level before any dosing

  // Keep the title up for the full BOOT_TITLE_MS, counted from when it first appeared
  while (millis() - titleStart < BOOT_TITLE_MS) { }

  lastPhCheck = millis() - PH_CHECK_INTERVAL_MS;  // first pH read right away
  cycleEnd    = millis() - COOLDOWN_MS;           // no cooldown at power-up
  enter(ST_IDLE);

#if USE_WATCHDOG
  wdt_enable(WDTO_8S);
#endif
}

void loop() {
  Sensors::updateFloats();
  // A level reading blocks for up to about 0.3 s. Do not take one while Servo 1 is spinning a
  // feed step, or the step would run longer than SERVO1_STEP_MS. Readings happen in the pauses.
  if (!(state == ST_CL_FEED && servo1Spinning)) ChlorineLevel::update();

  switch (state) {

    // ---------------- IDLE: only place pH is read ----------------
    case ST_IDLE:
      // During the cooldown nothing starts. The timer keeps counting and the display
      // shows any empty container under it (see updateDisplay).
      if (cooldownActive()) break;

      // Cooldown over: an empty container blocks EVERYTHING (acid and chlorine).
      if (Sensors::acidEmpty()) {
        Actuators::allOff();
        Serial.println(F("Acid container EMPTY - all dosing blocked"));
        acidPaused = false;                  // found at IDLE: no cycle to resume
        enter(ST_ACID_EMPTY);
        break;
      }
      if (ChlorineLevel::isEmpty()) {
        Actuators::allOff();
        Serial.println(F("Chlorine container EMPTY - all dosing blocked"));
        clPaused = false;                    // found at IDLE: no cycle to resume
        enter(ST_CL_EMPTY);
        break;
      }
      // pH check = settle-and-confirm (see config.h). A new check starts PH_CHECK_INTERVAL_MS
      // after the last accepted one (immediately at boot / after cooldown); while one is
      // running it is updated every loop.
      if ((millis() - lastPhCheck) >= PH_CHECK_INTERVAL_MS || Sensors::phCheckActive()) {
        float ph;
        Sensors::PhCheckStatus st = Sensors::phCheckUpdate(ph);
        if (st == Sensors::PHC_READY) {
          lastPhCheck = millis();
          lastPH = ph;
          Serial.print(F("pH: "));
          Serial.println(ph);
          if (ph > PH_THRESHOLD)      startAcid();
          else if (ph < PH_THRESHOLD) startChlorine();
        } else if (st == Sensors::PHC_BAD_LIMIT) {
          lastPH = NAN;
          fault("PH READ FAIL");
        }
      }
      break;

    // ---------------- ACID ----------------
    case ST_ACID_FILL:
      if (Sensors::acidEmpty()) {           // container ran dry while pumping
        if (!relay(R1_ACID_PUMP, false)) break;
        acidFillElapsed = elapsed();         // fill time used so far; the timeout does not run while paused
        acidPaused = true;
        acidRefilled = false;
        Serial.println(F("Acid container EMPTY while filling - paused"));
        enter(ST_ACID_EMPTY);
      } else if (Sensors::float1Reached()) {
        if (!relay(R1_ACID_PUMP, false)) break;
        if (!relay(R2_WATER_A, true)) break;
        enter(ST_ACID_WATER);
      } else if (elapsed() > ACID_FILL_TIMEOUT_MS) {
        fault("TIMEOUT:ACIDFILL");
      }
      break;

    // Acid empty. Two cases:
    //  - found at IDLE / before the cycle started (acidPaused false): everything blocked until
    //    refilled, then back to IDLE (a fresh pH check follows);
    //  - found while filling (acidPaused true): R1 stopped. After the refill is confirmed and
    //    ACID_REFILLED_MS has passed, ACID_FILL resumes with the SAME accepted pH (no new reading),
    //    and the fill timeout continues from the time already used.
    // No cooldown is started here: it only starts when a whole dosing process ends.
    case ST_ACID_EMPTY:
      if (!acidPaused) {
        if (!Sensors::acidEmpty()) {
          Serial.println(F("Acid container refilled"));
          enter(ST_IDLE);
        }
      } else if (Sensors::acidEmpty()) {
        acidRefilled = false;                // empty (again): cancel any resume delay
      } else if (!acidRefilled) {
        acidRefilled = true;                 // refill confirmed: start the resume delay
        acidRefilledAt = millis();
        Serial.println(F("Acid container refilled - resuming shortly"));
      } else if ((millis() - acidRefilledAt) >= ACID_REFILLED_MS) {
        acidPaused = false;
        acidRefilled = false;
        if (!relay(R1_ACID_PUMP, true)) break;
        enter(ST_ACID_FILL);
        stateStart = millis() - acidFillElapsed;   // fill timeout continues where it stopped
        Serial.println(F("Resuming acid fill"));
      }
      break;

    case ST_ACID_WATER:
      if (Sensors::float2Reached()) {
        if (!relay(R2_WATER_A, false)) break;
        if (!relay(R3_DISP_A, true)) break;
        enter(ST_ACID_DISPENSE);
      } else if (elapsed() > ACID_WATER_TIMEOUT_MS) {
        fault("WATER FILL T/O");
      }
      break;

    case ST_ACID_DISPENSE:
      if (elapsed() >= ACID_DISPENSE_MS) {
        if (!relay(R3_DISP_A, false)) break;
        finishCycle();
      }
      break;

    // Chlorine empty. Two cases:
    //  - found at IDLE (clPaused false): everything blocked until refilled, then back to IDLE;
    //  - found while feeding (clPaused true): Servo 1 stopped, weight held. After the refill is
    //    confirmed, CL_REFILLED is shown for CL_REFILLED_MS and the feed resumes from the held weight.
    case ST_CL_EMPTY:
      if (!clPaused) {
        if (!ChlorineLevel::isEmpty()) {
          Serial.println(F("Chlorine container refilled"));
          enter(ST_IDLE);
        }
      } else if (ChlorineLevel::isEmpty()) {
        clRefilled = false;                  // empty (again): cancel any CL_REFILLED delay
      } else if (!clRefilled) {
        clRefilled = true;                   // refill confirmed: start the CL_REFILLED delay
        clRefilledAt = millis();
        Serial.println(F("Chlorine container refilled - resuming shortly"));
      } else if ((millis() - clRefilledAt) >= CL_REFILLED_MS) {
        clPaused = false;
        clRefilled = false;
        enter(ST_CL_FEED);
        stateStart = millis() - clFeedElapsed;   // feed timeout continues where it stopped
        Actuators::servo1Spin();
        servo1Spinning = true;
        stepStart = millis();
        Serial.print(F("Resuming feed from "));
        Serial.print(lastWeight, 1);
        Serial.println(F(" g"));
      }
      break;

    // ---------------- CHLORINE ----------------
    case ST_CL_FEED: {
      float w;
      if (Sensors::readWeight(w)) lastWeight = w;
      if (millis() - lastWeightLog >= 500) {   // print twice per second
        lastWeightLog = millis();
        Serial.print(F("Weight: "));
        Serial.print(lastWeight, 1);
        Serial.println(F(" g"));
      }

      if (lastWeight >= TARGET_WEIGHT_G) {
        Serial.print(F("TARGET REACHED: "));
        Serial.print(lastWeight, 1);
        Serial.print(F(" g (target "));
        Serial.print(TARGET_WEIGHT_G, 0);
        Serial.println(F(" g)"));
        Actuators::servo1Stop();
        servo1Spinning = false;
        Actuators::servo2Open();
        enter(ST_CL_DROP);
      } else if (ChlorineLevel::isEmpty()) {
        // Container ran out while feeding: pause here. Weight is held and NOT re-tared.
        // (Only Servo 1 is touched: allOff() is not needed and could disturb the platform.)
        Actuators::servo1Stop();
        servo1Spinning = false;
        clFeedElapsed = elapsed();
        clPaused = true;
        clRefilled = false;
        Serial.print(F("Chlorine EMPTY while feeding - paused at "));
        Serial.print(lastWeight, 1);
        Serial.println(F(" g"));
        enter(ST_CL_EMPTY);
      } else {
        // Step feeding: spin one step, stop, pause (powder settles), repeat.
        // Steps get shorter within SLOW_BAND_G of the target so it doesn't overshoot.
        uint32_t now = millis();
        bool nearTarget = lastWeight >= TARGET_WEIGHT_G - SLOW_BAND_G;
        uint32_t spinMs = nearTarget ? SERVO1_STEP_SLOW_MS : SERVO1_STEP_MS;
        if (servo1Spinning && (now - stepStart) >= spinMs) {
          Actuators::servo1Stop();
          servo1Spinning = false;
          stepStart = now;
        } else if (!servo1Spinning && (now - stepStart) >= SERVO1_PAUSE_MS) {
          Actuators::servo1Spin();
          servo1Spinning = true;
          stepStart = now;
        }
        if (elapsed() > CL_FEED_TIMEOUT_MS) {
          Actuators::servo1Stop();
          fault("TIMEOUT:CL FEED");
        }
      }
      break;
    }

    case ST_CL_DROP:
      if (elapsed() >= SERVO2_OPEN_MS) {
        Actuators::servo2Close();
        enter(ST_CL_SETTLE);
      }
      break;

    case ST_CL_SETTLE:
      if (elapsed() >= CL_SETTLE_MS) {
        float w;
        if (Sensors::readWeight(w)) {
          lastWeight = w;
#if CHECK_PLATFORM_EMPTY
          if (w > PLATFORM_EMPTY_TOL_G) { fault("PLATFORM LOADED"); break; }
#endif
          // Start mixing: R4 (water) and R5 (mixer) together, separate timers.
          if (!relay(R4_WATER_B, true)) break;
          if (!relay(R5_MIXER, true)) break;
          r4Done = r5Done = false;
          enter(ST_CL_MIX);
        } else if (elapsed() > CL_SETTLE_MS + 2000UL) {
          fault("HX711 NO RESP");
        }
      }
      break;

    case ST_CL_MIX:
      if (!r4Done && elapsed() >= CL_R4_MS) {
        if (!relay(R4_WATER_B, false)) break;
        r4Done = true;
      }
      if (!r5Done && elapsed() >= CL_R5_MS) {
        if (!relay(R5_MIXER, false)) break;
        r5Done = true;
      }
      if (r4Done && r5Done) {            // R6 waits for BOTH
#if CL_DISPENSE_ENABLED
        if (!relay(R6_DISP_B, true)) break;
        enter(ST_CL_DISPENSE);
#else
        Serial.println(F("TEST: dispense pump disabled, ending cycle"));
        finishCycle();
#endif
      }
      break;

    case ST_CL_DISPENSE:
      if (elapsed() >= CL_R6_MS) {
        if (!relay(R6_DISP_B, false)) break;
        finishCycle();
      }
      break;

    // ---------------- FAULT: latched until reset / power cycle ----------------
    case ST_FAULT:
      break;
  }

  updateDisplay();

#if USE_WATCHDOG
  wdt_reset();
#endif
}