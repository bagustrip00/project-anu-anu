#pragma once
// =============================================================
// rpm.h
// RPM input abstraction.
//
// V2 REAL HARDWARE:
//   - Primary: Real pulser input on D4 (interrupt-driven)
//   - Fallback: Software RPM simulator (for testing)
//
// readRPM() is called every loop() and returns the current RPM
// from either source, without the ignition engine needing to know
// the difference.
// =============================================================

#include <Arduino.h>

enum class RpmSource : uint8_t {
    SIMULATOR,
    PICKUP  // Real pulser on D4
};

// Must be called once from setup().
void rpmInit();

// Non-blocking. Should be called every loop() iteration.
// Handles simulator sweep timing and checks for pulser timeout.
void rpmUpdate();

// Returns the current RPM value used by the ignition engine.
// Sources: Real pulser (D4 interrupt) or simulator.
uint16_t readRPM();

RpmSource getRpmSource();

// --- Simulator control (for testing/bench) ---
void simulatorSetEnabled(bool enabled);
bool simulatorIsEnabled();

void simulatorSetRPM(uint16_t rpm);
uint16_t simulatorGetRPM();

void simulatorStepRPM(int16_t deltaSteps); // deltaSteps in units of RPM_STEP

// --- Auto RPM sweep (test aid) ---
void sweepStart(uint16_t startRpm, uint16_t endRpm, uint16_t step, uint16_t intervalMs);
void sweepStop();
bool sweepIsRunning();

// --- Pulser statistics (for debugging) ---
unsigned long getPulserLastEdgeUs();
unsigned long getPulserPeriodUs();
