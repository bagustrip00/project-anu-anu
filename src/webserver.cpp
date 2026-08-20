#include "webserver.h"
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <LittleFS.h>
#include <ArduinoJson.h>

#include "config.h"
#include "ignition.h"
#include "rpm.h"
#include "storage.h"
#include "hardware.h"

static ESP8266WebServer server(80);

// ---------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------

static void sendJsonError(int code, const String &message) {
    DynamicJsonDocument doc(256);
    doc["success"] = false;
    doc["error"] = message;
    String out;
    serializeJson(doc, out);
    server.send(code, "application/json", out);
}

static void sendJsonOk(JsonDocument &doc) {
    doc["success"] = true;
    String out;
    serializeJson(doc, out);
    server.send(200, "application/json", out);
}

static bool serveStaticFile(const char *path, const char *contentType) {
    if (!LittleFS.exists(path)) return false;
    File f = LittleFS.open(path, "r");
    server.streamFile(f, contentType);
    f.close();
    return true;
}

static bool readBodyJson(DynamicJsonDocument &doc) {
    if (!server.hasArg("plain")) return false;
    DeserializationError err = deserializeJson(doc, server.arg("plain"));
    return !err;
}

// ---------------------------------------------------------------
// Static file handlers
// ---------------------------------------------------------------

static void handleRoot() {
    if (!serveStaticFile("/index.html", "text/html")) {
        server.send(500, "text/plain", "index.html missing from LittleFS");
    }
}

static void handleStyleCss() {
    if (!serveStaticFile("/style.css", "text/css")) {
        server.send(404, "text/plain", "not found");
    }
}

static void handleAppJs() {
    if (!serveStaticFile("/app.js", "application/javascript")) {
        server.send(404, "text/plain", "not found");
    }
}

// ---------------------------------------------------------------
// GET /api/status
// ---------------------------------------------------------------
static void handleGetStatus() {
    uint16_t rpm = readRPM();
    IgnitionResult result = calculateIgnitionTiming(rpm);
    scheduleIgnition(result.finalTiming); // placeholder hardware call

    DynamicJsonDocument doc(512);
    doc["rpm"] = rpm;
    doc["timing"] = result.finalTiming;
    doc["baseTiming"] = result.baseTiming;
    doc["activeMap"] = config.activeMap;
    doc["mapName"] = "MAP " + String(config.activeMap + 1);
    doc["starting"] = result.starting;
    doc["idle"] = result.idleActive;
    doc["rumbleIdle"] = result.rumbleActive;
    doc["backfire"] = result.backfireActive;
    doc["limiterState"] = limiterStateToString(result.limiterState);
    doc["rpmSource"] = (getRpmSource() == RpmSource::SIMULATOR) ? "SIMULATOR" : "PICKUP";
    doc["simulatorEnabled"] = simulatorIsEnabled();
    doc["sweepRunning"] = sweepIsRunning();
    doc["systemState"] = "RUNNING";
    sendJsonOk(doc);
}

// ---------------------------------------------------------------
// GET /api/config  (full configuration, all maps, all parameters)
// ---------------------------------------------------------------
static void handleGetConfig() {
    DynamicJsonDocument doc(8192);
    doc["version"] = config.version;
    doc["activeMap"] = config.activeMap;
    doc["startingAdvance"] = config.startingAdvance;
    doc["startingRPMThreshold"] = config.startingRPMThreshold;
    doc["rpmMin"] = RPM_MIN;
    doc["rpmMax"] = RPM_MAX;
    doc["rpmStep"] = RPM_STEP;
    doc["mapPoints"] = MAP_POINTS;

    JsonArray maps = doc.createNestedArray("maps");
    for (uint8_t m = 0; m < MAP_COUNT; m++) {
        const CDIMap &map = config.maps[m];
        JsonObject mo = maps.createNestedObject();
        mo["idleEnabled"] = map.idleEnabled;
        mo["idleRPM"] = map.idleRPM;
        mo["idleTiming"] = map.idleTiming;

        JsonObject rumble = mo.createNestedObject("rumble");
        rumble["enabled"] = map.rumble.enabled;
        rumble["targetRPM"] = map.rumble.targetRPM;
        rumble["minRPM"] = map.rumble.minRPM;
        rumble["maxRPM"] = map.rumble.maxRPM;
        rumble["retard"] = map.rumble.retard;

        JsonObject backfire = mo.createNestedObject("backfire");
        backfire["enabled"] = map.backfire.enabled;
        backfire["minRPM"] = map.backfire.minRPM;
        backfire["maxRPM"] = map.backfire.maxRPM;
        backfire["retard"] = map.backfire.retard;
        backfire["duration"] = map.backfire.duration;
        backfire["cooldown"] = map.backfire.cooldown;

        JsonObject limiter = mo.createNestedObject("limiter");
        limiter["minRPM"] = map.limiter.minRPM;
        limiter["maxRPM"] = map.limiter.maxRPM;
        limiter["softLimitRPM"] = map.limiter.softLimitRPM;
        limiter["hardLimitRPM"] = map.limiter.hardLimitRPM;
    }

    sendJsonOk(doc);
}

// ---------------------------------------------------------------
// GET /api/map  (active map ignition curve only)
// ---------------------------------------------------------------
static void handleGetMap() {
    DynamicJsonDocument doc(4096);
    doc["map"] = config.activeMap;

    JsonArray points = doc.createNestedArray("points");
    const CDIMap &map = config.maps[config.activeMap];
    for (size_t i = 0; i < MAP_POINTS; i++) {
        JsonObject p = points.createNestedObject();
        p["rpm"] = map.curve[i].rpm;
        p["timing"] = map.curve[i].timing;
    }

    sendJsonOk(doc);
}

// ---------------------------------------------------------------
// POST /api/map  (update active map ignition curve)
// ---------------------------------------------------------------
static void handlePostMap() {
    DynamicJsonDocument doc(4096);
    if (!readBodyJson(doc)) {
        sendJsonError(400, "Invalid request");
        return;
    }

    JsonArrayConst points = doc["points"].as<JsonArrayConst>();
    if (points.isNull() || points.size() != MAP_POINTS) {
        sendJsonError(400, "Invalid ignition map");
        return;
    }

    IgnitionPoint newCurve[MAP_POINTS];
    size_t i = 0;
    for (JsonObjectConst p : points) {
        if (i >= MAP_POINTS) break;
        if (!p.containsKey("rpm") || !p.containsKey("timing")) {
            sendJsonError(400, "Invalid ignition map");
            return;
        }
        newCurve[i].rpm = p["rpm"].as<uint16_t>();
        newCurve[i].timing = p["timing"].as<float>();
        i++;
    }

    if (!validateCurve(newCurve, MAP_POINTS)) {
        sendJsonError(400, "Invalid ignition map");
        return;
    }

    CDIMap &map = config.maps[config.activeMap];
    memcpy(map.curve, newCurve, sizeof(newCurve));

    DynamicJsonDocument resp(128);
    sendJsonOk(resp);
}

// ---------------------------------------------------------------
// POST /api/map/select
// ---------------------------------------------------------------
static void handlePostMapSelect() {
    DynamicJsonDocument doc(128);
    if (!readBodyJson(doc) || !doc.containsKey("map")) {
        sendJsonError(400, "Invalid request");
        return;
    }

    int m = doc["map"].as<int>();
    if (m < 0 || m >= MAP_COUNT) {
        sendJsonError(400, "Invalid map index");
        return;
    }

    config.activeMap = (uint8_t)m;

    DynamicJsonDocument resp(128);
    resp["activeMap"] = config.activeMap;
    sendJsonOk(resp);
}

// ---------------------------------------------------------------
// POST /api/rumble
// ---------------------------------------------------------------
static void handlePostRumble() {
    DynamicJsonDocument doc(256);
    if (!readBodyJson(doc)) {
        sendJsonError(400, "Invalid request");
        return;
    }

    RumbleConfig &r = config.maps[config.activeMap].rumble;
    if (doc.containsKey("enabled")) r.enabled = doc["enabled"].as<bool>();
    if (doc.containsKey("targetRPM")) r.targetRPM = doc["targetRPM"].as<uint16_t>();
    if (doc.containsKey("minRPM")) r.minRPM = doc["minRPM"].as<uint16_t>();
    if (doc.containsKey("maxRPM")) r.maxRPM = doc["maxRPM"].as<uint16_t>();
    if (doc.containsKey("retard")) r.retard = doc["retard"].as<float>();

    DynamicJsonDocument resp(128);
    sendJsonOk(resp);
}

// ---------------------------------------------------------------
// POST /api/backfire
// ---------------------------------------------------------------
static void handlePostBackfire() {
    DynamicJsonDocument doc(256);
    if (!readBodyJson(doc)) {
        sendJsonError(400, "Invalid request");
        return;
    }

    BackfireConfig &b = config.maps[config.activeMap].backfire;
    if (doc.containsKey("enabled")) b.enabled = doc["enabled"].as<bool>();
    if (doc.containsKey("minRPM")) b.minRPM = doc["minRPM"].as<uint16_t>();
    if (doc.containsKey("maxRPM")) b.maxRPM = doc["maxRPM"].as<uint16_t>();
    if (doc.containsKey("retard")) b.retard = doc["retard"].as<float>();
    if (doc.containsKey("duration")) b.duration = doc["duration"].as<uint16_t>();
    if (doc.containsKey("cooldown")) b.cooldown = doc["cooldown"].as<uint16_t>();

    DynamicJsonDocument resp(128);
    sendJsonOk(resp);
}

// ---------------------------------------------------------------
// POST /api/revlimit
// ---------------------------------------------------------------
static void handlePostRevLimit() {
    DynamicJsonDocument doc(256);
    if (!readBodyJson(doc)) {
        sendJsonError(400, "Invalid request");
        return;
    }

    LimiterConfig &l = config.maps[config.activeMap].limiter;
    uint16_t minR = doc.containsKey("minRPM") ? doc["minRPM"].as<uint16_t>() : l.minRPM;
    uint16_t maxR = doc.containsKey("maxRPM") ? doc["maxRPM"].as<uint16_t>() : l.maxRPM;
    uint16_t soft = doc.containsKey("softLimitRPM") ? doc["softLimitRPM"].as<uint16_t>() : l.softLimitRPM;
    uint16_t hard = doc.containsKey("hardLimitRPM") ? doc["hardLimitRPM"].as<uint16_t>() : l.hardLimitRPM;

    if (minR >= maxR) {
        sendJsonError(400, "minRPM must be less than maxRPM");
        return;
    }
    if (minR > soft) {
        sendJsonError(400, "minRPM must not exceed softLimitRPM");
        return;
    }
    if (soft > hard) {
        sendJsonError(400, "softLimitRPM must not exceed hardLimitRPM");
        return;
    }
    if (hard > maxR) {
        sendJsonError(400, "hardLimitRPM must not exceed maxRPM");
        return;
    }

    l.minRPM = minR;
    l.maxRPM = maxR;
    l.softLimitRPM = soft;
    l.hardLimitRPM = hard;

    DynamicJsonDocument resp(128);
    sendJsonOk(resp);
}

// ---------------------------------------------------------------
// POST /api/settings  (starting advance, idle, threshold)
// ---------------------------------------------------------------
static void handlePostSettings() {
    DynamicJsonDocument doc(256);
    if (!readBodyJson(doc)) {
        sendJsonError(400, "Invalid request");
        return;
    }

    if (doc.containsKey("startingAdvance")) {
        config.startingAdvance = doc["startingAdvance"].as<float>();
    }
    if (doc.containsKey("startingRPMThreshold")) {
        config.startingRPMThreshold = doc["startingRPMThreshold"].as<uint16_t>();
    }

    CDIMap &map = config.maps[config.activeMap];
    if (doc.containsKey("idleEnabled")) map.idleEnabled = doc["idleEnabled"].as<bool>();
    if (doc.containsKey("idleRPM")) map.idleRPM = doc["idleRPM"].as<uint16_t>();
    if (doc.containsKey("idleTiming")) map.idleTiming = doc["idleTiming"].as<float>();

    DynamicJsonDocument resp(128);
    sendJsonOk(resp);
}

// ---------------------------------------------------------------
// POST /api/rpm  (manual RPM test mode)
// ---------------------------------------------------------------
static void handlePostRpm() {
    DynamicJsonDocument doc(128);
    if (!readBodyJson(doc) || !doc.containsKey("rpm")) {
        sendJsonError(400, "Invalid request");
        return;
    }

    int rpm = doc["rpm"].as<int>();
    if (rpm < 0) {
        sendJsonError(400, "Invalid RPM");
        return;
    }

    if (doc.containsKey("step")) {
        simulatorStepRPM(doc["step"].as<int16_t>());
    } else {
        simulatorSetRPM((uint16_t)rpm);
    }

    DynamicJsonDocument resp(128);
    resp["rpm"] = simulatorGetRPM();
    sendJsonOk(resp);
}

// ---------------------------------------------------------------
// POST /api/simulator  (enable/disable simulator, start/stop sweep)
// ---------------------------------------------------------------
static void handlePostSimulator() {
    DynamicJsonDocument doc(256);
    if (!readBodyJson(doc)) {
        sendJsonError(400, "Invalid request");
        return;
    }

    if (doc.containsKey("enabled")) {
        simulatorSetEnabled(doc["enabled"].as<bool>());
    }

    if (doc.containsKey("sweep")) {
        String sweepCmd = doc["sweep"].as<String>();
        if (sweepCmd == "start") {
            uint16_t startRpm = doc["startRpm"] | RPM_MIN;
            uint16_t endRpm = doc["endRpm"] | RPM_MAX;
            uint16_t step = doc["step"] | RPM_STEP;
            uint16_t interval = doc["interval"] | 500;
            sweepStart(startRpm, endRpm, step, interval);
        } else if (sweepCmd == "stop") {
            sweepStop();
        }
    }

    DynamicJsonDocument resp(128);
    resp["simulatorEnabled"] = simulatorIsEnabled();
    resp["sweepRunning"] = sweepIsRunning();
    sendJsonOk(resp);
}

// ---------------------------------------------------------------
// POST /api/config/save, /api/config/load, /api/config/reset
// ---------------------------------------------------------------
static void handlePostConfigSave() {
    DynamicJsonDocument resp(128);
    if (!saveConfig()) {
        sendJsonError(500, "Failed to save configuration");
        return;
    }
    sendJsonOk(resp);
}

static void handlePostConfigLoad() {
    DynamicJsonDocument resp(128);
    loadConfig(); // falls back to defaults internally, never fails hard
    sendJsonOk(resp);
}

static void handlePostConfigReset() {
    DynamicJsonDocument resp(128);
    resetConfig();
    sendJsonOk(resp);
}

static void handleNotFound() {
    sendJsonError(404, "Invalid request");
}

// ---------------------------------------------------------------
// Public API
// ---------------------------------------------------------------

void webServerInit() {
    WiFi.mode(WIFI_AP);
    WiFi.softAP(AP_SSID, AP_PASSWORD);

    server.on("/", HTTP_GET, handleRoot);
    server.on("/style.css", HTTP_GET, handleStyleCss);
    server.on("/app.js", HTTP_GET, handleAppJs);

    server.on("/api/status", HTTP_GET, handleGetStatus);
    server.on("/api/config", HTTP_GET, handleGetConfig);
    server.on("/api/map", HTTP_GET, handleGetMap);

    server.on("/api/map", HTTP_POST, handlePostMap);
    server.on("/api/map/select", HTTP_POST, handlePostMapSelect);

    server.on("/api/rumble", HTTP_POST, handlePostRumble);
    server.on("/api/backfire", HTTP_POST, handlePostBackfire);
    server.on("/api/revlimit", HTTP_POST, handlePostRevLimit);
    server.on("/api/settings", HTTP_POST, handlePostSettings);

    server.on("/api/rpm", HTTP_POST, handlePostRpm);
    server.on("/api/simulator", HTTP_POST, handlePostSimulator);

    server.on("/api/config/save", HTTP_POST, handlePostConfigSave);
    server.on("/api/config/load", HTTP_POST, handlePostConfigLoad);
    server.on("/api/config/reset", HTTP_POST, handlePostConfigReset);

    server.onNotFound(handleNotFound);

    server.begin();
}

void webServerHandle() {
    server.handleClient();
}
