#pragma once
// =============================================================
// ignition.h
// Ignition timing calculation engine.
//
// This module is pure computation: RPM (+ config) in, a final
// ignition timing value (+ state flags) out. It does NOT touch
// Wi-Fi, HTTP, JSON or storage, and it does NOT drive any GPIO.
//
// Priority / order of operations (documented, do not reorder
// without updating this comment):
//
//   RPM INPUT
//     -> STARTING CHECK        (below starting RPM threshold?)
//     -> BASE IGNITION CURVE   (interpolated from active map)
//     -> IDLE MODIFIER         (near idle RPM -> idle timing)
//     -> RUMBLE IDLE MODIFIER  (retard while rumble idle active)
//     -> BACKFIRE MODIFIER     (retard pulse in configured window)
//     -> REV LIMITER STATE     (does not change timing directly in
//                                V1, only reports state; timing
//                                itself is still whatever the above
//                                stages produced)
//     -> FINAL IGNITION TIMING
//
// Rationale: starting logic takes priority over the curve because
// cranking RPM is noisy and unreliable. Idle/rumble/backfire are
// applied after the base curve because they are meant to modify
// (not replace) a normally-running engine's timing. The limiter is
// evaluated last because it only reports state in V1 - a real
// hardware limiter would cut/retard spark, which is out of scope
// for V1 (software placeholder only).
// =============================================================

#include <Arduino.h>
#include "config.h"

enum class LimiterState : uint8_t {
    NORMAL,
    SOFT_LIMIT,
    HARD_LIMIT
};

struct IgnitionResult {
    float        baseTiming;   // straight from the curve, before modifiers
    float        finalTiming;  // after all modifiers
    bool         starting;
    bool         idleActive;
    bool         rumbleActive;
    bool         backfireActive;
    LimiterState limiterState;
};

// Must be called once from setup().
void ignitionInit();

// Linear interpolation between two adjacent ignition points.
float interpolateTiming(float rpm, const IgnitionPoint &p1, const IgnitionPoint &p2);

// Looks up the active map's curve and returns interpolated base timing.
float calculateBaseTiming(uint16_t rpm, const CDIMap &map);

float applyStartingTiming(float timing, uint16_t rpm, bool &starting);
float applyIdleTiming(float timing, uint16_t rpm, const CDIMap &map, bool &idleActive);
float applyRumbleIdle(float timing, uint16_t rpm, const CDIMap &map, bool &rumbleActive);
float applyBackfire(float timing, uint16_t rpm, const CDIMap &map, bool &backfireActive);
LimiterState applyRevLimiter(uint16_t rpm, const CDIMap &map);

float finalIgnitionTiming(uint16_t rpm, const CDIMap &map, IgnitionResult &result);

// Runs the full calculation pipeline for the current RPM and active map.
IgnitionResult calculateIgnitionTiming(uint16_t rpm);

const char *limiterStateToString(LimiterState state);
