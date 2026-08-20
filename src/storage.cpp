#include "storage.h"
#include <LittleFS.h>
#include <ArduinoJson.h>

static const char *CONFIG_PATH = "/config.json";

// Sized generously for MAP_COUNT * MAP_POINTS ignition points plus
// scalar fields. 37 points * 3 maps * ~2 fields each is the bulk of
// the document. Measured comfortably under 12KB for MAP_POINTS=37,
// MAP_COUNT=3; ESP8266 has enough heap for a short-lived document
// like this (it is not kept around after save/load returns).
static const size_t JSON_CAPACITY = 16384;

static void mapToJson(const CDIMap &map, JsonObject obj) {
    JsonArray points = obj.createNestedArray("points");
    for (size_t i = 0; i < MAP_POINTS; i++) {
        JsonObject p = points.createNestedObject();
        p["rpm"] = map.curve[i].rpm;
        p["timing"] = map.curve[i].timing;
    }

    JsonObject rumble = obj.createNestedObject("rumble");
    rumble["enabled"] = map.rumble.enabled;
    rumble["targetRPM"] = map.rumble.targetRPM;
    rumble["minRPM"] = map.rumble.minRPM;
    rumble["maxRPM"] = map.rumble.maxRPM;
    rumble["retard"] = map.rumble.retard;

    JsonObject backfire = obj.createNestedObject("backfire");
    backfire["enabled"] = map.backfire.enabled;
    backfire["minRPM"] = map.backfire.minRPM;
    backfire["maxRPM"] = map.backfire.maxRPM;
    backfire["retard"] = map.backfire.retard;
    backfire["duration"] = map.backfire.duration;
    backfire["cooldown"] = map.backfire.cooldown;

    obj["idleEnabled"] = map.idleEnabled;
    obj["idleRPM"] = map.idleRPM;
    obj["idleTiming"] = map.idleTiming;

    JsonObject limiter = obj.createNestedObject("limiter");
    limiter["minRPM"] = map.limiter.minRPM;
    limiter["maxRPM"] = map.limiter.maxRPM;
    limiter["softLimitRPM"] = map.limiter.softLimitRPM;
    limiter["hardLimitRPM"] = map.limiter.hardLimitRPM;
}

static bool jsonToMap(JsonObjectConst obj, CDIMap &map) {
    JsonArrayConst points = obj["points"].as<JsonArrayConst>();
    if (points.isNull() || points.size() != MAP_POINTS) return false;

    size_t i = 0;
    for (JsonObjectConst p : points) {
        if (i >= MAP_POINTS) break;
        map.curve[i].rpm = p["rpm"].as<uint16_t>();
        map.curve[i].timing = p["timing"].as<float>();
        i++;
    }
    if (!validateCurve(map.curve, MAP_POINTS)) return false;

    JsonObjectConst rumble = obj["rumble"];
    map.rumble.enabled   = rumble["enabled"] | false;
    map.rumble.targetRPM = rumble["targetRPM"] | 1500;
    map.rumble.minRPM    = rumble["minRPM"] | 1200;
    map.rumble.maxRPM    = rumble["maxRPM"] | 1800;
    map.rumble.retard    = rumble["retard"] | 5.0f;

    JsonObjectConst backfire = obj["backfire"];
    map.backfire.enabled  = backfire["enabled"] | false;
    map.backfire.minRPM   = backfire["minRPM"] | 3000;
    map.backfire.maxRPM   = backfire["maxRPM"] | 6000;
    map.backfire.retard   = backfire["retard"] | 10.0f;
    map.backfire.duration = backfire["duration"] | 100;
    map.backfire.cooldown = backfire["cooldown"] | 500;

    map.idleEnabled = obj["idleEnabled"] | true;
    map.idleRPM     = obj["idleRPM"] | 1500;
    map.idleTiming  = obj["idleTiming"] | 10.0f;

    JsonObjectConst limiter = obj["limiter"];
    // minRPM/maxRPM default to the full ignition map range so that
    // configs saved before this field existed still load cleanly.
    map.limiter.minRPM       = limiter["minRPM"] | RPM_MIN;
    map.limiter.maxRPM       = limiter["maxRPM"] | RPM_MAX;
    map.limiter.softLimitRPM = limiter["softLimitRPM"] | 9500;
    map.limiter.hardLimitRPM = limiter["hardLimitRPM"] | 10000;

    return true;
}

bool storageInit() {
    return LittleFS.begin();
}

bool saveConfig() {
    DynamicJsonDocument doc(JSON_CAPACITY);

    doc["version"] = config.version;
    doc["activeMap"] = config.activeMap;
    doc["startingAdvance"] = config.startingAdvance;
    doc["startingRPMThreshold"] = config.startingRPMThreshold;

    JsonArray maps = doc.createNestedArray("maps");
    for (uint8_t m = 0; m < MAP_COUNT; m++) {
        JsonObject mapObj = maps.createNestedObject();
        mapToJson(config.maps[m], mapObj);
    }

    File f = LittleFS.open(CONFIG_PATH, "w");
    if (!f) return false;

    size_t written = serializeJson(doc, f);
    f.close();

    return written > 0;
}

bool loadConfig() {
    if (!LittleFS.exists(CONFIG_PATH)) {
        setDefaultConfig();
        return false;
    }

    File f = LittleFS.open(CONFIG_PATH, "r");
    if (!f) {
        setDefaultConfig();
        return false;
    }

    DynamicJsonDocument doc(JSON_CAPACITY);
    DeserializationError err = deserializeJson(doc, f);
    f.close();

    if (err) {
        setDefaultConfig();
        return false;
    }

    CDIConfig loaded{};
    loaded.version = doc["version"] | 0;

    if (loaded.version != CONFIG_VERSION) {
        setDefaultConfig();
        return false;
    }

    loaded.activeMap = doc["activeMap"] | 0;
    loaded.startingAdvance = doc["startingAdvance"] | 5.0f;
    loaded.startingRPMThreshold = doc["startingRPMThreshold"] | 500;

    JsonArrayConst maps = doc["maps"].as<JsonArrayConst>();
    if (maps.isNull() || maps.size() != MAP_COUNT) {
        setDefaultConfig();
        return false;
    }

    uint8_t i = 0;
    bool ok = true;
    for (JsonObjectConst mapObj : maps) {
        if (i >= MAP_COUNT) break;
        if (!jsonToMap(mapObj, loaded.maps[i])) {
            ok = false;
            break;
        }
        i++;
    }

    if (!ok || !validateConfig(loaded)) {
        setDefaultConfig();
        return false;
    }

    config = loaded;
    return true;
}

void resetConfig() {
    setDefaultConfig();
    saveConfig();
}
