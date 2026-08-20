#pragma once
// =============================================================
// storage.h
// Configuration persistence using LittleFS.
//
// The whole CDIConfig struct is serialized to JSON and written to
// /config.json on LittleFS. Binary struct dumping is avoided so the
// format stays readable/portable across firmware revisions.
// =============================================================

#include <Arduino.h>
#include "config.h"

// Mounts LittleFS. Must be called once from setup() before any
// other storage function. Returns true on success.
bool storageInit();

// Serializes `config` to /config.json. Returns true on success.
bool saveConfig();

// Reads /config.json into `config`. If the file is missing,
// corrupt, or has a mismatched version, falls back to
// setDefaultConfig() and returns false (but never crashes).
bool loadConfig();

// Resets `config` to defaults and persists it immediately.
void resetConfig();
