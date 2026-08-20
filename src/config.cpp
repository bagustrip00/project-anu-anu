#include "config.h"

CDIConfig config;

// ---------------------------------------------------------------
// SUPRA X 125 BASE IGNITION CURVE (MAP 1)
// ---------------------------------------------------------------
// OEM DATA RESEARCH:
//   Source: Typical Honda 125cc single-cylinder 4-stroke patterns
//           + Supra X 125 service manual base timing (10° BTDC @ idle)
//   Status: Idle timing (10° BTDC) is factory-confirmed; full curve
//           is based on industry-standard CDI progression for this class.
//   Uncertainty: Honda does not publish full CDI maps publicly.
//               This curve MUST be verified with timing light + dyno
//               before final deployment on any real engine.
//   Model: Supra X 125 (all generations, carburetor variant)
//
// RPM Range: 500-10000 rpm (37 points at 250 rpm steps)
//
// Progression:
//   - Idle (1000-1500 rpm): ~10-12° BTDC (factory base timing)
//   - Mid-range (3000-6000 rpm): increases to ~26-28° BTDC
//   - High RPM (7000-10000 rpm): plateaus around 28° to prevent knock
//
// This is a CONSERVATIVE curve suitable for stock or near-stock engines.
// For high-performance or heavily modified engines, adjustment via Web UI
// or additional map tuning is recommended.

static const float SUPRA_X_125_BASE_CURVE[MAP_POINTS] = {
    10.0f,  // 500 rpm
    10.2f,  // 750
    10.4f,  // 1000
    10.6f,  // 1250
    10.8f,  // 1500
    11.5f,  // 1750
    13.0f,  // 2000
    14.5f,  // 2250
    16.0f,  // 2500
    18.0f,  // 2750
    20.0f,  // 3000
    21.0f,  // 3250
    22.0f,  // 3500
    23.0f,  // 3750
    24.0f,  // 4000
    24.5f,  // 4250
    25.0f,  // 4500
    25.5f,  // 4750
    26.0f,  // 5000
    26.5f,  // 5250
    27.0f,  // 5500
    27.5f,  // 5750
    28.0f,  // 6000
    28.0f,  // 6250
    28.0f,  // 6500
    28.0f,  // 6750
    28.0f,  // 7000
    27.8f,  // 7250
    27.5f,  // 7500
    27.2f,  // 7750
    27.0f,  // 8000
    26.8f,  // 8250
    26.5f,  // 8500
    26.2f,  // 8750
    26.0f,  // 9000
    25.8f,  // 9250
    25.5f   // 9500 and above (clamped to max RPM 10000)
};

// Generic curve for MAP 2 (user-editable)
static const float DEFAULT_TIMING_DEG_MAP2[MAP_POINTS] = {
    10.0f, 10.5f, 12.0f, 13.5f, 15.0f, 16.0f, 17.0f, 18.5f, 20.0f, 21.0f,
    22.0f, 23.0f, 24.0f, 24.5f, 25.0f, 25.5f, 26.0f, 26.5f, 27.0f, 27.5f,
    28.0f, 28.0f, 28.0f, 27.5f, 27.0f, 26.5f, 26.0f, 25.5f, 25.0f, 24.5f,
    23.0f, 22.5f, 22.0f, 21.0f, 20.0f, 19.0f, 18.0f
};

// Generic curve for MAP 3 (user-editable)
static const float DEFAULT_TIMING_DEG_MAP3[MAP_POINTS] = {
    10.0f, 10.5f, 12.0f, 13.5f, 15.0f, 16.0f, 17.0f, 18.5f, 20.0f, 21.0f,
    22.0f, 23.0f, 24.0f, 24.5f, 25.0f, 25.5f, 26.0f, 26.5f, 27.0f, 27.5f,
    28.0f, 28.0f, 28.0f, 27.5f, 27.0f, 26.5f, 26.0f, 25.5f, 25.0f, 24.5f,
    23.0f, 22.5f, 22.0f, 21.0f, 20.0f, 19.0f, 18.0f
};

static void fillCurve(IgnitionPoint *curve, const float *timingValues) {
    for (size_t i = 0; i < MAP_POINTS; i++) {
        curve[i].rpm    = RPM_MIN + (uint16_t)(i * RPM_STEP);
        curve[i].timing = timingValues[i];
    }
}

void setDefaultConfig() {
    config.version = CONFIG_VERSION;
    config.activeMap = 0;
    config.startingAdvance = 5.0f;
    config.startingRPMThreshold = 500;

    // MAP 1: Supra X 125 base curve (OEM-based)
    {
        CDIMap &map = config.maps[0];
        fillCurve(map.curve, SUPRA_X_125_BASE_CURVE);

        // TSS default: AUTO mode with conservative retard
        map.tss.mode = TSSMode::AUTO;
        map.tss.retardDeg = 5.0f;  // REQUIRES CALIBRATION

        map.rumble.enabled   = false;
        map.rumble.targetRPM = 1500;
        map.rumble.minRPM    = 1200;
        map.rumble.maxRPM    = 1800;
        map.rumble.retard    = 5.0f;

        map.backfire.enabled  = false;
        map.backfire.minRPM   = 3000;
        map.backfire.maxRPM   = 6000;
        map.backfire.retard   = 10.0f;
        map.backfire.duration = 100;
        map.backfire.cooldown = 500;

        map.idleEnabled = true;
        map.idleRPM     = 1500;
        map.idleTiming  = 10.0f;

        map.limiter.minRPM       = RPM_MIN;
        map.limiter.maxRPM       = RPM_MAX;
        map.limiter.softLimitRPM = 9500;
        map.limiter.hardLimitRPM = 10000;
    }

    // MAP 2: Generic user-editable
    {
        CDIMap &map = config.maps[1];
        fillCurve(map.curve, DEFAULT_TIMING_DEG_MAP2);

        map.tss.mode = TSSMode::AUTO;
        map.tss.retardDeg = 5.0f;

        map.rumble.enabled   = false;
        map.rumble.targetRPM = 1500;
        map.rumble.minRPM    = 1200;
        map.rumble.maxRPM    = 1800;
        map.rumble.retard    = 5.0f;

        map.backfire.enabled  = false;
        map.backfire.minRPM   = 3000;
        map.backfire.maxRPM   = 6000;
        map.backfire.retard   = 10.0f;
        map.backfire.duration = 100;
        map.backfire.cooldown = 500;

        map.idleEnabled = true;
        map.idleRPM     = 1500;
        map.idleTiming  = 10.0f;

        map.limiter.minRPM       = RPM_MIN;
        map.limiter.maxRPM       = RPM_MAX;
        map.limiter.softLimitRPM = 9500;
        map.limiter.hardLimitRPM = 10000;
    }

    // MAP 3: Generic user-editable
    {
        CDIMap &map = config.maps[2];
        fillCurve(map.curve, DEFAULT_TIMING_DEG_MAP3);

        map.tss.mode = TSSMode::AUTO;
        map.tss.retardDeg = 5.0f;

        map.rumble.enabled   = false;
        map.rumble.targetRPM = 1500;
        map.rumble.minRPM    = 1200;
        map.rumble.maxRPM    = 1800;
        map.rumble.retard    = 5.0f;

        map.backfire.enabled  = false;
        map.backfire.minRPM   = 3000;
        map.backfire.maxRPM   = 6000;
        map.backfire.retard   = 10.0f;
        map.backfire.duration = 100;
        map.backfire.cooldown = 500;

        map.idleEnabled = true;
        map.idleRPM     = 1500;
        map.idleTiming  = 10.0f;

        map.limiter.minRPM       = RPM_MIN;
        map.limiter.maxRPM       = RPM_MAX;
        map.limiter.softLimitRPM = 9500;
        map.limiter.hardLimitRPM = 10000;
    }
}

bool validateCurve(const IgnitionPoint *points, size_t count) {
    if (count != MAP_POINTS) return false;

    for (size_t i = 0; i < count; i++) {
        uint16_t expectedRpm = RPM_MIN + (uint16_t)(i * RPM_STEP);
        if (points[i].rpm != expectedRpm) return false; // enforces ascending + step
        if (points[i].timing < TIMING_MIN_DEG || points[i].timing > TIMING_MAX_DEG) {
            return false;
        }
    }
    return true;
}

bool validateConfig(const CDIConfig &cfg) {
    if (cfg.version != CONFIG_VERSION) return false;
    if (cfg.activeMap >= MAP_COUNT) return false;

    for (uint8_t m = 0; m < MAP_COUNT; m++) {
        if (!validateCurve(cfg.maps[m].curve, MAP_POINTS)) return false;

        const LimiterConfig &l = cfg.maps[m].limiter;
        if (l.minRPM >= l.maxRPM) return false;
        if (l.minRPM > l.softLimitRPM) return false;
        if (l.softLimitRPM > l.hardLimitRPM) return false;
        if (l.hardLimitRPM > l.maxRPM) return false;

        // TSS retard sanity check
        if (cfg.maps[m].tss.retardDeg < 0.0f || cfg.maps[m].tss.retardDeg > 20.0f) {
            return false;
        }
    }
    return true;
}
