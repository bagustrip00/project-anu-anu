#include "rpm.h"
#include "config.h"

// ---------------------------------------------------------------
// SIMULATOR STATE (for testing/bench mode)
// ---------------------------------------------------------------
static bool simEnabled = true;   // V2: Simulator available for testing
static uint16_t simRPM = 3250;

static bool sweepRunning = false;
static uint16_t sweepStartRpm = RPM_MIN;
static uint16_t sweepEndRpm   = RPM_MAX;
static uint16_t sweepStep     = RPM_STEP;
static uint16_t sweepInterval = 500;
static uint16_t sweepCurrentRpm = RPM_MIN;
static unsigned long sweepLastStepMs = 0;

// ---------------------------------------------------------------
// REAL PULSER STATE (D4 interrupt-driven)
// ---------------------------------------------------------------
// The pulser generates one pulse per crankshaft revolution.
// We measure the period between pulses to calculate RPM.
// HARDWARE ASSUMPTION: Interrupt on D4 (GPIO2) with debounce
static volatile uint32_t pulserLastEdgeUs = 0;
static volatile uint32_t pulserPeriodUs = 0;
static volatile bool pulserEdgeDetected = false;
static unsigned long pulserLastRpmUpdateMs = 0;
static uint16_t pulserRPM = 0;

void rpmInit() {
    simEnabled = true;
    simRPM = 3250;
    sweepRunning = false;

    // Configure D4 (GPIO2) as input for pulser
    pinMode(PIN_PULSER, INPUT);
    
    // Attach interrupt on rising edge
    // HARDWARE ASSUMPTION: Pulser output is a rising edge pulse per revolution
    attachInterrupt(digitalPinToInterrupt(PIN_PULSER), pulserInterruptHandler, RISING);
    
    pulserLastEdgeUs = micros();
    pulserPeriodUs = 0;
    pulserEdgeDetected = false;
    pulserRPM = 0;
}

// ---------------------------------------------------------------
// PULSER INTERRUPT HANDLER (HIGH PRIORITY, REAL-TIME)
// ---------------------------------------------------------------
void ICACHE_RAM_ATTR pulserInterruptHandler() {
    uint32_t now = micros();
    uint32_t delta = now - pulserLastEdgeUs;
    
    // Debounce: reject pulses closer than PULSER_DEBOUNCE_US
    if (delta < PULSER_DEBOUNCE_US) {
        return;
    }
    
    pulserLastEdgeUs = now;
    pulserPeriodUs = delta;  // Time between edges in microseconds
    pulserEdgeDetected = true;
}

// ---------------------------------------------------------------
// RPM CALCULATION FROM PULSER
// ---------------------------------------------------------------
static void updatePulserRPM() {
    unsigned long now = millis();
    
    // Update RPM calculation at regular intervals (non-blocking)
    if (now - pulserLastRpmUpdateMs < 50) {
        return;  // Wait 50ms between recalculations
    }
    pulserLastRpmUpdateMs = now;
    
    if (!pulserEdgeDetected) {
        // No new edges since last check
        // Check for timeout: if no pulse in RPM_TIMEOUT_MS, RPM = 0
        if (now - pulserLastRpmUpdateMs > RPM_TIMEOUT_MS) {
            pulserRPM = 0;
        }
        return;
    }
    
    pulserEdgeDetected = false;
    
    // RPM = 60,000,000 / period_in_microseconds
    // (60 seconds * 1,000,000 microseconds per second / period)
    if (pulserPeriodUs > 0) {
        uint32_t rpm32 = 60000000UL / pulserPeriodUs;
        
        // Clamp to valid range
        if (rpm32 < RPM_MIN) rpm32 = RPM_MIN;
        if (rpm32 > RPM_MAX) rpm32 = RPM_MAX;
        
        pulserRPM = (uint16_t)rpm32;
    } else {
        pulserRPM = 0;
    }
}

void rpmUpdate() {
    // Update real pulser RPM calculation
    updatePulserRPM();
    
    // Update simulator sweep if running
    if (!sweepRunning) return;

    unsigned long now = millis();
    if (now - sweepLastStepMs < sweepInterval) return;

    sweepLastStepMs = now;
    simRPM = sweepCurrentRpm;

    if (sweepCurrentRpm >= sweepEndRpm) {
        sweepCurrentRpm = sweepStartRpm; // loop the sweep
    } else {
        sweepCurrentRpm = (uint16_t)min((uint32_t)sweepEndRpm,
                                         (uint32_t)sweepCurrentRpm + sweepStep);
    }
}

// ---------------------------------------------------------------
// PUBLIC RPM SOURCE SELECTION
// ---------------------------------------------------------------
uint16_t readRPM() {
    // V2: Primary source is real pulser if enabled
    // Fallback to simulator if no valid pulser signal
    if (simEnabled && pulserRPM == 0) {
        // Simulator active or no pulser signal yet
        return simRPM;
    }
    return pulserRPM;
}

RpmSource getRpmSource() {
    // If we have valid pulser RPM, we're using real pickup
    if (pulserRPM > 0 && !simEnabled) {
        return RpmSource::PICKUP;
    }
    return RpmSource::SIMULATOR;
}

// ---------------------------------------------------------------
// SIMULATOR CONTROL
// ---------------------------------------------------------------
void simulatorSetEnabled(bool enabled) { 
    simEnabled = enabled; 
}

bool simulatorIsEnabled() { 
    return simEnabled; 
}

void simulatorSetRPM(uint16_t rpm) {
    if (rpm < RPM_MIN) rpm = RPM_MIN;
    if (rpm > RPM_MAX) rpm = RPM_MAX;
    simRPM = rpm;
    sweepRunning = false; // manual set cancels an active sweep
}

uint16_t simulatorGetRPM() { 
    return simRPM; 
}

void simulatorStepRPM(int16_t deltaSteps) {
    int32_t newRpm = (int32_t)simRPM + (int32_t)deltaSteps * (int32_t)RPM_STEP;
    if (newRpm < RPM_MIN) newRpm = RPM_MIN;
    if (newRpm > RPM_MAX) newRpm = RPM_MAX;
    simRPM = (uint16_t)newRpm;
    sweepRunning = false;
}

// ---------------------------------------------------------------
// SWEEP CONTROL
// ---------------------------------------------------------------
void sweepStart(uint16_t startRpm, uint16_t endRpm, uint16_t step, uint16_t intervalMs) {
    if (startRpm < RPM_MIN) startRpm = RPM_MIN;
    if (endRpm   > RPM_MAX) endRpm   = RPM_MAX;
    if (step == 0) step = RPM_STEP;
    if (intervalMs == 0) intervalMs = 500;

    sweepStartRpm = startRpm;
    sweepEndRpm = endRpm;
    sweepStep = step;
    sweepInterval = intervalMs;
    sweepCurrentRpm = startRpm;
    sweepLastStepMs = millis();
    sweepRunning = true;
}

void sweepStop() {
    sweepRunning = false;
}

bool sweepIsRunning() {
    return sweepRunning;
}

// ---------------------------------------------------------------
// DEBUG / DIAGNOSTICS
// ---------------------------------------------------------------
unsigned long getPulserLastEdgeUs() {
    return pulserLastEdgeUs;
}

unsigned long getPulserPeriodUs() {
    return pulserPeriodUs;
}
