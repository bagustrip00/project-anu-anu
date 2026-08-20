#include "rpm.h"
#include "config.h"

static bool simEnabled = true;   // V1: simulator is the only RPM source
static uint16_t simRPM = 3250;

static bool sweepRunning = false;
static uint16_t sweepStartRpm = RPM_MIN;
static uint16_t sweepEndRpm   = RPM_MAX;
static uint16_t sweepStep     = RPM_STEP;
static uint16_t sweepInterval = 500;
static uint16_t sweepCurrentRpm = RPM_MIN;
static unsigned long sweepLastStepMs = 0;

void rpmInit() {
    simEnabled = true;
    simRPM = 3250;
    sweepRunning = false;
}

void rpmUpdate() {
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

uint16_t readRPM() {
    // V1: only the simulator provides RPM. When a real pickup sensor
    // is added, branch here on getRpmSource() without touching any
    // ignition calculation code.
    return simRPM;
}

RpmSource getRpmSource() {
    return simEnabled ? RpmSource::SIMULATOR : RpmSource::SIMULATOR;
}

void simulatorSetEnabled(bool enabled) { simEnabled = enabled; }
bool simulatorIsEnabled() { return simEnabled; }

void simulatorSetRPM(uint16_t rpm) {
    if (rpm < RPM_MIN) rpm = RPM_MIN;
    if (rpm > RPM_MAX) rpm = RPM_MAX;
    simRPM = rpm;
    sweepRunning = false; // manual set cancels an active sweep
}

uint16_t simulatorGetRPM() { return simRPM; }

void simulatorStepRPM(int16_t deltaSteps) {
    int32_t newRpm = (int32_t)simRPM + (int32_t)deltaSteps * (int32_t)RPM_STEP;
    if (newRpm < RPM_MIN) newRpm = RPM_MIN;
    if (newRpm > RPM_MAX) newRpm = RPM_MAX;
    simRPM = (uint16_t)newRpm;
    sweepRunning = false;
}

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
