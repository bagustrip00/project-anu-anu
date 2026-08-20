#pragma once
// =============================================================
// config.h
// Global configuration data structures for the Programmable CDI
// Web Controller V2 (Real Hardware Edition).
//
// Changes from V1:
//   - Added GPIO pin definitions for real hardware
//   - Added TSS (Throttle Switch Sensor) configuration per map
//   - Added calibration parameters for pulser-to-timing conversion
//   - Increased CONFIG_VERSION to handle migration
//
// This module defines ONLY data structures + default/save/load
// declarations. It does not talk to the network or to hardware.
// =============================================================

#include <Arduino.h>

// ---------------------------------------------------------------
// Wi-Fi Access Point settings (single place to edit)
// ---------------------------------------------------------------
const char AP_SSID[]     = "KONTOLODON";
const char AP_PASSWORD[] = "12345678";
// AP IP is fixed by ESP8266 core default: 192.168.4.1

// ---------------------------------------------------------------
// GPIO PIN MAPPING (Wemos D1 Mini)
// ---------------------------------------------------------------
// HARDWARE ASSUMPTION: All GPIO is driven via optocoupler for isolation.
// ESP8266 logic levels (3.3V) are safe; ensure optocoupler circuit
// is rated for these voltage levels.
//
// IMPORTANT: D8 (GPIO15) and D3 (GPIO0) are boot-mode sensitive.
// Ensure external circuitry does not interfere with boot strapping.
// HARDWARE ASSUMPTION: D8 should NOT pull low during boot (must be pulled high
// or left floating during reset). Verify your optocoupler driver circuit.

const uint8_t PIN_PULSER            = D4;  // GPIO2: Pulser input (interrupt)
const uint8_t PIN_CDI_SCR           = D7;  // GPIO13: SCR/trigger output
const uint8_t PIN_CDI_STATUS_LED    = D8;  // GPIO15: CDI status (LOW=OK, HIGH=fault)
const uint8_t PIN_TSS               = D6;  // GPIO12: TSS input (HIGH=off, LOW=on by default)
const uint8_t PIN_LIMITER_LED       = D5;  // GPIO14: Limiter indicator (blink when active)
const uint8_t PIN_MAP_SWITCH        = D3;  // GPIO0: Map selector button (external pull-down)

// TSS polarity: TSS OFF = HIGH, TSS ON = LOW
// HARDWARE ASSUMPTION: Verify this matches your optocoupler circuit.
// Can be inverted if rangkaian uses opposite logic.
const uint8_t TSS_ACTIVE_LEVEL = LOW;   // TSS ON when GPIO reads LOW

// ---------------------------------------------------------------
// RPM / ignition map resolution
// ---------------------------------------------------------------
const uint16_t RPM_MIN  = 500;
const uint16_t RPM_MAX  = 10000;
const uint16_t RPM_STEP = 250;

// Number of points is computed automatically, never hard-coded.
const size_t MAP_POINTS = ((RPM_MAX - RPM_MIN) / RPM_STEP) + 1; // 37

const uint8_t MAP_COUNT = 3;

// Software-only sanity bounds for ignition timing (NOT an engine
// recommendation - just prevents nonsensical values in storage).
const float TIMING_MIN_DEG = -30.0f;
const float TIMING_MAX_DEG =  60.0f;

const uint16_t CONFIG_VERSION = 2;  // Bumped for TSS addition

// ---------------------------------------------------------------
// PULSER / RPM CALCULATION CALIBRATION
// ---------------------------------------------------------------
// The pulser generates one pulse per crankshaft revolution.
// HARDWARE ASSUMPTION: Verify with oscilloscope that pulser signal
// is a single clean pulse per revolution. Adjust debounce/filtering
// as needed based on actual signal quality.
const uint16_t PULSER_DEBOUNCE_US = 100;  // Minimum microseconds between edges

// RPM calculation: RPM = 60000000 / (period_in_microseconds)
// REQUIRES HARDWARE CALIBRATION: Verify with tachometer after integration

// Timeout: If no pulsa received for this duration, RPM = 0
const unsigned long RPM_TIMEOUT_MS = 500;

// ---------------------------------------------------------------
// TSS / THROTTLE SWITCH SENSOR
// ---------------------------------------------------------------
// Modes: AUTO = follow input, ON = force active, OFF = force inactive
enum class TSSMode : uint8_t {
    AUTO = 0,  // Read actual D6 input
    ON   = 1,  // Force TSS active regardless of input
    OFF  = 2   // Force TSS inactive regardless of input
};

// TSS timing retard (degrees BTDC to subtract from base timing)
// HARDWARE ASSUMPTION: Typical Honda CDI applies 3-10° retard on deselerasi.
// REQUIRES HARDWARE CALIBRATION: Measure with timing light to confirm.
// For now, use conservative default.
struct TSSConfig {
    TSSMode mode;
    float   retardDeg;  // degrees subtracted when TSS active
};

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

    TSSConfig      tss;
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

// Fills `config` with V2 default/test values.
// MAP 1 uses Supra X 125 base curve (research-based).
// MAP 2 and MAP 3 are empty/generic for user tuning.
// WARNING: Default ignition values should be verified with timing light
// before using on any real engine.
void setDefaultConfig();

// Basic structural validation of a whole CDIConfig (version, ranges).
bool validateConfig(const CDIConfig &cfg);

// Basic structural validation of a single ignition curve array.
// Enforces: ascending RPM, correct RPM_STEP spacing, count == MAP_POINTS,
// timing within [TIMING_MIN_DEG, TIMING_MAX_DEG].
bool validateCurve(const IgnitionPoint *points, size_t count);
