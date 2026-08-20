#pragma once
// =============================================================
// rpm.h
// RPM input abstraction.
//
// V1 only has a software RPM simulator. Later, readRPM() can be
// re-implemented on top of a real pickup sensor / trigger circuit
// WITHOUT changing the ignition calculation engine, because the
// rest of the firmware only ever calls readRPM().
// =============================================================

#include <Arduino.h>

enum class RpmSource : uint8_t {
    SIMULATOR,
    PICKUP // reserved for future hardware RPM input
};

// Must be called once from setup().
void rpmInit();

// Non-blocking. Should be called every loop() iteration.
// Currently a no-op placeholder for future pickup-sensor interrupt
// bookkeeping, but kept here so main.cpp doesn't need to change
// when real hardware is introduced.
void rpmUpdate();

// Returns the current RPM value used by the ignition engine.
// NOTE: The actual pickup sensor requires an appropriate signal
// conditioning/protection circuit before connection to the ESP8266.
// This function does not perform any GPIO reads in V1.
uint16_t readRPM();

RpmSource getRpmSource();

// --- Simulator control ---
void simulatorSetEnabled(bool enabled);
bool simulatorIsEnabled();

void simulatorSetRPM(uint16_t rpm);
uint16_t simulatorGetRPM();

void simulatorStepRPM(int16_t deltaSteps); // deltaSteps in units of RPM_STEP

// --- Auto RPM sweep (test aid) ---
void sweepStart(uint16_t startRpm, uint16_t endRpm, uint16_t step, uint16_t intervalMs);
void sweepStop();
bool sweepIsRunning();
