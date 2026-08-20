#include "config.h"

CDIConfig config;

// Reference default ignition curve, 250 RPM resolution, 1000-10000 RPM.
// These are software test values only (see WARNING in config.h).
static const float DEFAULT_TIMING_DEG[MAP_POINTS] = {
    10.0f, 10.5f, 12.0f, 13.5f, 15.0f, 16.0f, 17.0f, 18.5f, 20.0f, 21.0f,
    22.0f, 23.0f, 24.0f, 24.5f, 25.0f, 25.5f, 26.0f, 26.5f, 27.0f, 27.5f,
    28.0f, 28.0f, 28.0f, 27.5f, 27.0f, 26.5f, 26.0f, 25.5f, 25.0f, 24.5f,
    23.0f, 22.5f, 22.0f, 21.0f, 20.0f, 19.0f, 18.0f
};

static void fillDefaultCurve(IgnitionPoint *curve) {
    for (size_t i = 0; i < MAP_POINTS; i++) {
        curve[i].rpm    = RPM_MIN + (uint16_t)(i * RPM_STEP);
        curve[i].timing = DEFAULT_TIMING_DEG[i];
    }
}

void setDefaultConfig() {
    config.version = CONFIG_VERSION;
    config.activeMap = 0;
    config.startingAdvance = 5.0f;
    config.startingRPMThreshold = 500;

    for (uint8_t m = 0; m < MAP_COUNT; m++) {
        CDIMap &map = config.maps[m];

        fillDefaultCurve(map.curve);

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
    }
    return true;
}
