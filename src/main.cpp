// =============================================================
// Programmable CDI Web Controller V1
// Target: Wemos D1 Mini (ESP8266), Arduino framework
//
// Architecture:
//   Browser -> Wi-Fi AP -> Web Server -> Configuration
//            -> Ignition Engine -> RPM Simulator -> Hardware Abstraction
//
// V1 does NOT drive any high-voltage ignition coil. Ignition output
// is a software placeholder only (see hardware.h/.cpp).
//
// main.cpp intentionally contains no business logic: it wires the
// modules together and runs a non-blocking loop. Real-time work
// (RPM read + ignition calc) and non-real-time work (Wi-Fi/HTTP)
// both run every iteration; no delay() is used anywhere.
// =============================================================

#include <Arduino.h>
#include <ESP8266Wifi.h>

#include "config.h"
#include "rpm.h"
#include "ignition.h"
#include "hardware.h"
#include "storage.h"
#include "webserver.h"

#define DEBUG 1

#if DEBUG
static unsigned long lastDebugMs = 0;
static const unsigned long DEBUG_INTERVAL_MS = 1000;
#endif

void setup() {
#if DEBUG
    Serial.begin(115200);
    Serial.println();
    Serial.println(F("Programmable CDI Web Controller V1 booting..."));
#endif

    // Config: start from defaults, then try to load persisted config.
    setDefaultConfig();

    if (!storageInit()) {
#if DEBUG
        Serial.println(F("LittleFS mount failed, using defaults only"));
#endif
    } else if (loadConfig()) {
#if DEBUG
        Serial.println(F("Configuration loaded from LittleFS"));
#endif
    } else {
#if DEBUG
        Serial.println(F("No valid saved configuration, using defaults"));
#endif
    }

    hardwareInit();
    rpmInit();
    ignitionInit();
    webServerInit();

#if DEBUG
    Serial.print(F("AP SSID: "));
    Serial.println(AP_SSID);
    Serial.print(F("AP IP: "));
    Serial.println(WiFi.softAPIP());
#endif
}

void loop() {
    // --- HIGH PRIORITY / REAL-TIME-ISH WORK ---
    rpmUpdate();
    uint16_t rpm = readRPM();
    IgnitionResult result = calculateIgnitionTiming(rpm);
    scheduleIgnition(result.finalTiming);

    // --- LOW PRIORITY / NON-REAL-TIME WORK ---
    webServerHandle();

#if DEBUG
    unsigned long now = millis();
    if (now - lastDebugMs >= DEBUG_INTERVAL_MS) {
        lastDebugMs = now;
        Serial.print(F("RPM=")); Serial.print(rpm);
        Serial.print(F(" MAP=")); Serial.print(config.activeMap + 1);
        Serial.print(F(" BASE=")); Serial.print(result.baseTiming);
        Serial.print(F(" FINAL=")); Serial.print(result.finalTiming);
        Serial.print(F(" RUMBLE=")); Serial.print(result.rumbleActive ? "ON" : "OFF");
        Serial.print(F(" BACKFIRE=")); Serial.print(result.backfireActive ? "ON" : "OFF");
        Serial.print(F(" LIMITER=")); Serial.print(limiterStateToString(result.limiterState));
        Serial.print(F(" SRC=")); Serial.println(getRpmSource() == RpmSource::SIMULATOR ? "SIMULATOR" : "PICKUP");
    }
#endif

    // No delay() here - yield() is implicit via loop() return, which
    // lets the ESP8266 Wi-Fi stack run in the background.
}
