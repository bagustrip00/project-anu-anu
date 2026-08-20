#pragma once
// =============================================================
// webserver.h
// Low-priority / non-real-time layer: Wi-Fi AP, HTTP server, JSON
// REST API, and serving the static web UI from LittleFS.
//
// ESP8266WebServer::handleClient() is non-blocking (it services at
// most one pending request per call), so calling it every loop()
// iteration does not stall RPM/ignition calculation.
// =============================================================

#include <Arduino.h>

void webServerInit();

// Must be called every loop() iteration. Non-blocking.
void webServerHandle();
