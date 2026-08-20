#pragma once
// =============================================================
// hardware.h
// Hardware abstraction layer.
//
// V1 DOES NOT drive any ignition coil or high-voltage output.
// scheduleIgnition() only records the value it was given so it can
// be shown on the dashboard / debug serial. Nothing in this file
// touches GPIO for ignition purposes.
//
// Hardware ignition driver must be implemented separately according
// to the specific CDI/coil/trigger circuit.
// =============================================================

#include <Arduino.h>

void hardwareInit();

// V1 placeholder: stores `timing` as lastScheduledTiming and nothing
// else. Real ignition timing requires a crank/trigger angle
// reference, not just an RPM-derived degree value - that scheduler
// belongs to a future hardware-timer-based implementation.
void scheduleIgnition(float timing);

float getLastScheduledTiming();
