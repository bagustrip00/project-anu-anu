#pragma once
// =============================================================
// hardware.h
// Hardware abstraction layer for V2.
//
// Manages:
//   - D7: SCR ignition output (CDI trigger)
//   - D8: CDI status LED (LOW=OK, HIGH=fault)
//   - D5: Limiter indicator LED (blink when active)
//   - D6: TSS input (read via this module)
//   - D3: MAP button input (edge detection, debounce)
//
// All GPIO operations are non-blocking and designed to never
// interfere with real-time ignition calculations.
// =============================================================

#include <Arduino.h>
#include "ignition.h"

// ---------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------
void hardwareInit();

// ---------------------------------------------------------------
// IGNITION OUTPUT (D7 - SCR TRIGGER)
// ---------------------------------------------------------------
// Schedule ignition pulse at the specified timing (degrees BTDC).
// HARDWARE ASSUMPTION: Real implementation will use PWM/timer
// to generate precise pulse relative to crankshaft angle.
// V2 placeholder: For now, logs the value for dashboard display.
void scheduleIgnition(float timing);
float getLastScheduledTiming();

// ---------------------------------------------------------------
// CDI STATUS LED (D8)
// ---------------------------------------------------------------
// Set system fault state. LOW = normal, HIGH = fault.
void setCdiFault(bool fault);
bool isCdiFaulty();

// ---------------------------------------------------------------
// LIMITER LED (D5)
// ---------------------------------------------------------------
// Called from main loop to update limiter indicator.
// Blinks non-blocking based on millis().
void updateLimiterLED(bool limiterActive);

// ---------------------------------------------------------------
// TSS INPUT (D6)
// ---------------------------------------------------------------
// Read current TSS input state.
bool readTSSInput();

// ---------------------------------------------------------------
// MAP BUTTON (D3)
// ---------------------------------------------------------------
// Edge detection for MAP switch with debounce.
// Returns true if button pressed (rising edge detected).
// Resets automatically after reading (one event per press).
bool mapButtonPressed();

// Call every loop iteration to update button debounce state.
void updateMapButton();
