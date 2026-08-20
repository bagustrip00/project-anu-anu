#pragma once
// =============================================================
// config.h
// Global configuration data structures for the Programmable CDI
// Web Controller V1.
//
// This module defines ONLY data structures + default/save/load
// declarations. It does not talk to the network or to hardware.
// =============================================================

#include <Arduino.h>

// ---------------------------------------------------------------
// Wi-Fi Access Point settings (single place to edit)
// ---------------------------------------------------------------
constexpr char AP_SSID[]     = "KONTOLODON";
constexpr char AP_PASSWORD[] = "12345678";
// AP IP is fixed by ESP8266 core default: 192.168.4.1

// ---------------------------------------------------------------
// RPM / ignition map resolution
// ---------------------------------------------------------------
constexpr uint16_t RPM_MIN  = 500;
constexpr uint16_t RPM_MAX  = 10000;
constexpr uint16_t RPM_STEP = 250;

// Number of points is computed automatically, never hard-coded.
constexpr size_t MAP_POINTS = ((RPM_MAX - RPM_MIN) / RPM_STEP) + 1; // 37

constexpr uint8_t MAP_COUNT = 3;

// Software-only sanity bounds for ignition timing (NOT an engine
// recommendation - just prevents nonsensical values in storage).
constexpr float TIMING_MIN_DEG = -30.0f;
constexpr float TIMING_MAX_DEG =  60.0f;

constexpr uint16_t CONFIG_VERSION = 1;

// ---------------------------------------------------------------
// Data structures
// ---------------------------------------------------------------
struct IgnitionPoint {
    uint16_t rpm;
    float    timing; // degrees BTDC
};

struct RumbleConfig {
    bool     enabled;
    uint16_t targetRPM;
    uint16_t minRPM;
    uint16_t maxRPM;
    float    retard; // degrees subtracted from base timing
};

struct BackfireConfig {
    bool     enabled;
    uint16_t minRPM;
    uint16_t maxRPM;
    float    retard;   // degrees
    uint16_t duration;  // ms, how long the retard pulse lasts
    uint16_t cooldown;  // ms, minimum time between pulses
};

struct LimiterConfig {
    // Operating window for this map's rev limiter. These bound the
    // valid range for softLimitRPM/hardLimitRPM below - they are NOT
    // an ignition cutoff by themselves (see applyRevLimiter() in
    // ignition.cpp for how limiter state is actually derived).
    uint16_t minRPM;
    uint16_t maxRPM;
    uint16_t softLimitRPM;
    uint16_t hardLimitRPM;
};

struct CDIMap {
    IgnitionPoint curve[MAP_POINTS];

    RumbleConfig   rumble;
    BackfireConfig backfire;

    uint16_t idleRPM;
    float    idleTiming;
    bool     idleEnabled;

    LimiterConfig limiter;
};

struct CDIConfig {
    uint16_t version;

    CDIMap maps[MAP_COUNT];

    uint8_t activeMap;

    float    startingAdvance;
    uint16_t startingRPMThreshold;
};

// Global configuration instance (defined in config.cpp)
extern CDIConfig config;

// ---------------------------------------------------------------
// Public API
// ---------------------------------------------------------------

// Fills `config` with V1 default/test values.
// WARNING: Default ignition values are software test values only.
// They are NOT a recommended ignition map for any specific engine.
void setDefaultConfig();

// Basic structural validation of a whole CDIConfig (version, ranges).
bool validateConfig(const CDIConfig &cfg);

// Basic structural validation of a single ignition curve array.
// Enforces: ascending RPM, correct RPM_STEP spacing, count == MAP_POINTS,
// timing within [TIMING_MIN_DEG, TIMING_MAX_DEG].
bool validateCurve(const IgnitionPoint *points, size_t count);
