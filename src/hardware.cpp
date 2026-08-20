#include "hardware.h"
#include "config.h"

// ---------------------------------------------------------------
// IGNITION TIMING STATE
// ---------------------------------------------------------------
static float lastScheduledTiming = 0.0f;

// ---------------------------------------------------------------
// CDI FAULT STATE (D8)
// ---------------------------------------------------------------
static bool cdiFault = false;
static unsigned long faultSetTime = 0;

// ---------------------------------------------------------------
// LIMITER LED STATE (D5)
// ---------------------------------------------------------------
static bool limiterActive = false;
static unsigned long limiterBlinkLastMs = 0;
static bool limiterBlinkState = false;
const unsigned long LIMITER_BLINK_PERIOD_MS = 200;  // 200ms on + 200ms off = 500ms total

// ---------------------------------------------------------------
// MAP BUTTON STATE (D3)
// ---------------------------------------------------------------
static bool mapButtonLastState = LOW;
static unsigned long mapButtonLastChangeMs = 0;
static bool mapButtonEventPending = false;
const unsigned long MAP_BUTTON_DEBOUNCE_MS = 50;

// ---------------------------------------------------------------
// INITIALIZATION
// ---------------------------------------------------------------
void hardwareInit() {
    // Configure GPIO outputs - start in SAFE state
    pinMode(PIN_CDI_SCR, OUTPUT);
    digitalWrite(PIN_CDI_SCR, LOW);  // SCR trigger: LOW (safe)

    pinMode(PIN_CDI_STATUS_LED, OUTPUT);
    digitalWrite(PIN_CDI_STATUS_LED, LOW);  // Status: OK

    pinMode(PIN_LIMITER_LED, OUTPUT);
    digitalWrite(PIN_LIMITER_LED, LOW);  // Limiter: not active

    // Configure GPIO inputs
    pinMode(PIN_TSS, INPUT);         // TSS input from optocoupler
    pinMode(PIN_MAP_SWITCH, INPUT);  // MAP button (external pull-down)

    // Initialize state
    lastScheduledTiming = 0.0f;
    cdiFault = false;
    faultSetTime = 0;
    limiterActive = false;
    limiterBlinkLastMs = millis();
    limiterBlinkState = false;
    mapButtonLastState = LOW;
    mapButtonLastChangeMs = millis();
    mapButtonEventPending = false;
}

// ---------------------------------------------------------------
// IGNITION OUTPUT (D7)
// ---------------------------------------------------------------
void scheduleIgnition(float timing) {
    // HARDWARE ASSUMPTION: This is a placeholder.
    // Real implementation requires:
    //   1. Crank angle reference from pulser
    //   2. PWM or timer to generate pulse at precise angle
    //   3. Proper isolation/driver circuit
    //
    // For now, just store the value for dashboard display.
    lastScheduledTiming = timing;

    // TODO: Implement real SCR trigger based on crankshaft position
    // Example (pseudocode):
    //   if (timingShouldFire(timing)) {
    //       digitalWrite(PIN_CDI_SCR, HIGH);
    //       delayMicroseconds(PULSE_WIDTH_US);
    //       digitalWrite(PIN_CDI_SCR, LOW);
    //   }
}

float getLastScheduledTiming() {
    return lastScheduledTiming;
}

// ---------------------------------------------------------------
// CDI STATUS LED (D8)
// ---------------------------------------------------------------
void setCdiFault(bool fault) {
    if (fault && !cdiFault) {
        cdiFault = true;
        faultSetTime = millis();
        digitalWrite(PIN_CDI_STATUS_LED, HIGH);
    } else if (!fault && cdiFault) {
        cdiFault = false;
        digitalWrite(PIN_CDI_STATUS_LED, LOW);
    }
}

bool isCdiFaulty() {
    return cdiFault;
}

// ---------------------------------------------------------------
// LIMITER LED (D5) - NON-BLOCKING BLINK
// ---------------------------------------------------------------
void updateLimiterLED(bool limiterActive_) {
    limiterActive = limiterActive_;
    unsigned long now = millis();

    if (!limiterActive) {
        // Limiter not active: LED off
        digitalWrite(PIN_LIMITER_LED, LOW);
        limiterBlinkLastMs = now;
        limiterBlinkState = false;
        return;
    }

    // Limiter active: blink LED
    unsigned long elapsed = now - limiterBlinkLastMs;
    if (elapsed >= LIMITER_BLINK_PERIOD_MS) {
        limiterBlinkLastMs = now;
        limiterBlinkState = !limiterBlinkState;
    }

    digitalWrite(PIN_LIMITER_LED, limiterBlinkState ? HIGH : LOW);
}

// ---------------------------------------------------------------
// TSS INPUT (D6)
// ---------------------------------------------------------------
bool readTSSInput() {
    // TSS OFF = HIGH, TSS ON = LOW (from optocoupler)
    // Return true if TSS is active (ON = LOW)
    return digitalRead(PIN_TSS) == TSS_ACTIVE_LEVEL;
}

// ---------------------------------------------------------------
// MAP BUTTON (D3) - EDGE DETECTION WITH DEBOUNCE
// ---------------------------------------------------------------
bool mapButtonPressed() {
    bool result = mapButtonEventPending;
    mapButtonEventPending = false;
    return result;
}

void updateMapButton() {
    unsigned long now = millis();
    bool currentState = digitalRead(PIN_MAP_SWITCH);

    // Check if state has changed
    if (currentState != mapButtonLastState) {
        // State changed, start debounce timer
        mapButtonLastChangeMs = now;
        mapButtonLastState = currentState;
        return;
    }

    // State stable for debounce period?
    if (now - mapButtonLastChangeMs < MAP_BUTTON_DEBOUNCE_MS) {
        return;  // Still debouncing
    }

    // Debounce complete - check for rising edge (button pressed)
    // Pin goes HIGH when button is pressed (external pull-down)
    if (currentState == HIGH && !mapButtonEventPending) {
        mapButtonEventPending = true;
    }
}
