#include "ignition.h"
#include "hardware.h"

// Backfire pulse state (non-blocking, millis()-based).
static bool backfirePulseActive = false;
static unsigned long backfirePulseStartMs = 0;
static unsigned long backfireLastPulseEndMs = 0;

void ignitionInit() {
    backfirePulseActive = false;
    backfirePulseStartMs = 0;
    backfireLastPulseEndMs = 0;
}

float interpolateTiming(float rpm, const IgnitionPoint &p1, const IgnitionPoint &p2) {
    if (p2.rpm == p1.rpm) return p1.timing; // guard against divide-by-zero
    float t = (rpm - (float)p1.rpm) / ((float)p2.rpm - (float)p1.rpm);
    if (t < 0.0f) t = 0.0f;
    if (t > 1.0f) t = 1.0f;
    return p1.timing + t * (p2.timing - p1.timing);
}

float calculateBaseTiming(uint16_t rpm, const CDIMap &map) {
    if (rpm <= map.curve[0].rpm) {
        return map.curve[0].timing;
    }
    if (rpm >= map.curve[MAP_POINTS - 1].rpm) {
        return map.curve[MAP_POINTS - 1].timing;
    }

    for (size_t i = 0; i < MAP_POINTS - 1; i++) {
        const IgnitionPoint &p1 = map.curve[i];
        const IgnitionPoint &p2 = map.curve[i + 1];
        if (rpm >= p1.rpm && rpm <= p2.rpm) {
            return interpolateTiming((float)rpm, p1, p2);
        }
    }

    // Should be unreachable given the checks above.
    return map.curve[MAP_POINTS - 1].timing;
}

float applyStartingTiming(float timing, uint16_t rpm, bool &starting) {
    if (rpm > 0 && rpm < config.startingRPMThreshold) {
        starting = true;
        return config.startingAdvance;
    }
    starting = false;
    return timing;
}

float applyIdleTiming(float timing, uint16_t rpm, const CDIMap &map, bool &idleActive) {
    if (!map.idleEnabled) {
        idleActive = false;
        return timing;
    }

    // Simple hysteresis band around idleRPM to avoid sudden transitions.
    const uint16_t band = RPM_STEP; // +/- one map step
    if (rpm >= (map.idleRPM > band ? map.idleRPM - band : 0) &&
        rpm <= map.idleRPM + band) {
        idleActive = true;
        return map.idleTiming;
    }

    idleActive = false;
    return timing;
}

float applyTSSRetard(float timing, uint16_t rpm, const CDIMap &map, bool &tssActive) {
    // HARDWARE ASSUMPTION: TSS input is on D6, read via hardware module
    TSSConfig tss = map.tss;
    bool tssInput = readTSSInput();  // Read actual GPIO
    
    // Determine if TSS should be active based on mode
    bool shouldRetard = false;
    
    switch (tss.mode) {
        case TSSMode::AUTO:
            shouldRetard = tssInput;
            break;
        case TSSMode::ON:
            shouldRetard = true;
            break;
        case TSSMode::OFF:
            shouldRetard = false;
            break;
    }
    
    tssActive = shouldRetard;
    
    if (shouldRetard) {
        return timing - tss.retardDeg;
    }
    return timing;
}

float applyRumbleIdle(float timing, uint16_t rpm, const CDIMap &map, bool &rumbleActive) {
    // PROTOTYPE ALGORITHM: this is a simple retard-in-band implementation
    // and must be validated against a real engine before any hardware
    // ignition output is ever attached to it.
    const RumbleConfig &r = map.rumble;

    if (!r.enabled || rpm < r.minRPM || rpm > r.maxRPM) {
        rumbleActive = false;
        return timing;
    }

    rumbleActive = true;
    return timing - r.retard;
}

float applyBackfire(float timing, uint16_t rpm, const CDIMap &map, bool &backfireActive) {
    const BackfireConfig &b = map.backfire;
    unsigned long now = millis();

    if (!b.enabled) {
        backfirePulseActive = false;
        backfireActive = false;
        return timing;
    }

    bool inWindow = (rpm >= b.minRPM && rpm <= b.maxRPM);

    if (backfirePulseActive) {
        if (now - backfirePulseStartMs >= b.duration) {
            backfirePulseActive = false;
            backfireLastPulseEndMs = now;
        }
    } else if (inWindow) {
        bool cooldownElapsed = (now - backfireLastPulseEndMs) >= b.cooldown;
        if (cooldownElapsed) {
            backfirePulseActive = true;
            backfirePulseStartMs = now;
        }
    }

    backfireActive = backfirePulseActive;
    return backfireActive ? (timing - b.retard) : timing;
}

LimiterState applyRevLimiter(uint16_t rpm, const CDIMap &map) {
    if (rpm >= map.limiter.hardLimitRPM) return LimiterState::HARD_LIMIT;
    if (rpm >= map.limiter.softLimitRPM) return LimiterState::SOFT_LIMIT;
    return LimiterState::NORMAL;
}

float finalIgnitionTiming(uint16_t rpm, const CDIMap &map, IgnitionResult &result) {
    result.baseTiming = calculateBaseTiming(rpm, map);
    float t = result.baseTiming;

    t = applyStartingTiming(t, rpm, result.starting);

    // Idle/TSS/rumble/backfire modifiers are skipped while cranking - the
    // starting stage already produced the timing that should be used.
    if (!result.starting) {
        t = applyIdleTiming(t, rpm, map, result.idleActive);
        t = applyTSSRetard(t, rpm, map, result.tssActive);
        t = applyRumbleIdle(t, rpm, map, result.rumbleActive);
        t = applyBackfire(t, rpm, map, result.backfireActive);
    } else {
        result.idleActive = false;
        result.tssActive = false;
        result.rumbleActive = false;
        result.backfireActive = false;
    }

    result.limiterState = applyRevLimiter(rpm, map);
    result.finalTiming = t;
    return t;
}

IgnitionResult calculateIgnitionTiming(uint16_t rpm) {
    IgnitionResult result{};
    const CDIMap &map = config.maps[config.activeMap];
    finalIgnitionTiming(rpm, map, result);
    return result;
}

const char *limiterStateToString(LimiterState state) {
    switch (state) {
        case LimiterState::NORMAL:     return "NORMAL";
        case LimiterState::SOFT_LIMIT: return "SOFT_LIMIT";
        case LimiterState::HARD_LIMIT: return "HARD_LIMIT";
    }
    return "NORMAL";
}
