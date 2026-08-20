#include "hardware.h"

static float lastScheduledTiming = 0.0f;

void hardwareInit() {
    // Intentionally does not configure any GPIO for ignition output.
    // The actual pickup sensor requires an appropriate signal
    // conditioning/protection circuit before connection to the
    // ESP8266; that circuit and its driver are out of scope for V1.
    lastScheduledTiming = 0.0f;
}

void scheduleIgnition(float timing) {
    // Software placeholder only - see header comment.
    lastScheduledTiming = timing;
}

float getLastScheduledTiming() {
    return lastScheduledTiming;
}
