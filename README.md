# Programmable CDI Web Controller V1

Wemos D1 Mini / ESP8266 — Web UI ignition-map tuner **prototype**.

**V1 does not drive any ignition coil.** `hardware.cpp::scheduleIgnition()`
only stores the calculated value for the dashboard/debug output. There is no
GPIO ignition output, no coil driver, and no high-voltage circuitry anywhere
in this firmware.

## A. Architecture

```
Browser
  |
Wi-Fi AP (192.168.4.1, SSID/PW in config.h)
  |
Web Server (webserver.cpp) -- REST API + static files from LittleFS
  |
Configuration (config.h/.cpp, storage.h/.cpp) -- CDIConfig, JSON persistence
  |
Ignition Engine (ignition.h/.cpp) -- interpolation, starting/idle/rumble/
  |                                    backfire/limiter, pure computation
RPM Simulator (rpm.h/.cpp) -- readRPM() abstraction, manual + sweep modes
  |
Hardware Abstraction (hardware.h/.cpp) -- placeholder only, no coil output
```

`main.cpp` wires these together in a non-blocking `loop()`: no `delay()` is
used anywhere in the control path. `webServerHandle()` (HTTP) and the RPM/
ignition read-and-calculate step both run every iteration, so the web
server can never stall ignition math.

## B. Dependencies

| Library | Purpose | Version | Install |
|---|---|---|---|
| ESP8266 Arduino Core | Wi-Fi, `ESP8266WebServer`, `LittleFS` | latest stable (3.x) | Arduino IDE Boards Manager: add `http://arduino.esp8266.com/stable/package_esp8266com_index.json`, then install "esp8266" boards package. In PlatformIO it's pulled automatically via `platform = espressif8266`. |
| ArduinoJson (bblanchon) | JSON parsing/serialization for the REST API and config file | ^6.21.3 | Arduino IDE: Library Manager -> search "ArduinoJson" by Benoit Blanchon -> Install. PlatformIO: already listed in `platformio.ini` under `lib_deps`. |

No other third-party libraries are used.

## C. Folder structure

```
cdi/
  platformio.ini
  src/
    main.cpp
    config.h / config.cpp
    ignition.h / ignition.cpp
    rpm.h / rpm.cpp
    storage.h / storage.cpp
    webserver.h / webserver.cpp
    hardware.h / hardware.cpp
  data/                <- uploaded to LittleFS, served by the web server
    index.html
    style.css
    app.js
```

### Using Arduino IDE instead of PlatformIO

Arduino IDE does not support a `src/` subfolder directly. Two options:

1. **Recommended:** use PlatformIO (VS Code extension) with this folder
   as-is — no changes needed.
2. **Arduino IDE:** create a sketch folder named `cdi` (matching this
   project name) and copy all files from `src/` directly into that sketch
   folder (flat, no subfolder) alongside a `cdi.ino` that just contains:
   ```cpp
   // cdi.ino intentionally left with no code — main.cpp provides
   // setup()/loop(). Arduino IDE compiles all .cpp/.h files in the
   // sketch folder together, so this file can stay empty.
   ```
   Then use the **ESP8266 LittleFS Data Upload** tool (Arduino IDE plugin)
   pointed at the `data/` folder to flash the web UI files. The program
   logic itself stays fully modular across the separate `.h`/`.cpp` files;
   only the folder layout changes.

## D–S. Source code

See the files listed in section C above — every file is complete,
compilable source, no pseudocode, no omitted sections.

## T. Install steps

1. Install [PlatformIO](https://platformio.org/) (VS Code extension) **or**
   Arduino IDE + ESP8266 board package (see table in section B).
2. Install the ArduinoJson library (PlatformIO does this automatically from
   `platformio.ini`; Arduino IDE users install it via Library Manager).
3. Open this `cdi/` folder as a PlatformIO project (or copy files per the
   Arduino IDE instructions above).

## U. Upload steps

**PlatformIO:**
```
pio run -t uploadfs   # uploads data/ (index.html, style.css, app.js) to LittleFS
pio run -t upload     # compiles and uploads the firmware
```
Run `uploadfs` first — the web server serves `index.html`/`style.css`/
`app.js` straight from LittleFS, so the UI won't load without it.

**Arduino IDE:**
1. Select board "LOLIN(WEMOS) D1 R2 & mini".
2. Use the LittleFS Data Upload tool to upload the `data/` folder contents.
3. Click Upload to compile and flash `cdi.ino` (which pulls in main.cpp
   and all modules from the same folder).

## V. Test steps

1. On your phone, connect to Wi-Fi network **CDI_TUNER** (password
   `12345678`).
2. Open `http://192.168.4.1` in a browser.
3. Go to **RPM Simulator** tab, confirm "Simulator ON" is checked.
4. Set a manual RPM (e.g. 3250) and tap **SET RPM**; confirm the Dashboard
   updates within ~200ms.
5. Go to **Mapping** tab, switch between MAP 1/2/3; confirm the Ignition
   Map tab's graph and table change accordingly.
6. Go to **Ignition Map** tab, edit a timing value, tap **APPLY**; confirm
   `GET /api/map` (or the graph) reflects the change.
7. Set RPM to a value between two map points (e.g. 3125 with 3000=20.0 and
   3250=21.0 defaults) and confirm the dashboard shows ~20.5° (interpolation
   test — see Test Case 2 below).
8. Go to **Rumble Idle**, enable it with an RPM band that contains your
   current test RPM, apply, and confirm `FINAL` timing on the dashboard
   drops by the configured retard.
9. Go to **Backfire**, enable it with an RPM band containing your test RPM,
   apply, and confirm the dashboard's BACKFIRE indicator toggles ON/OFF in
   pulses (duration/cooldown-driven).
10. Set RPM to/above the Soft Limit, then Hard Limit values on the **Rev
    Limiter** tab; confirm REV LIMIT shows `SOFT_LIMIT` / `HARD_LIMIT`.
11. Go to **Settings**, tap **SAVE CONFIG**.
12. Power-cycle (reboot) the Wemos.
13. Reconnect and reload the page; confirm your edited map/rumble/backfire/
    limiter values are still present (i.e. they survived the reboot).

## Test cases (from the spec)

| # | RPM | Expected |
|---|---|---|
| 1 | 3000 | 20.0° (default MAP 1) |
| 2 | 3125 | 20.5° (interpolated between 3000=20.0° and 3250=21.0°) |
| 3 | 9500 | `SOFT_LIMIT` (default soft limit) |
| 4 | 10000 | `HARD_LIMIT` (default hard limit) |
| 5 | select MAP 2 | `activeMap` becomes `1` |
| 6 | Rumble ON, RPM in band | final timing = base - retard |
| 7 | Backfire ON, RPM in band | timing modifier pulses per duration/cooldown |
| 8 | Save, reboot | configuration persists via LittleFS |

## Module reference (what handles what)

1. **RPM** — `rpm.h`/`rpm.cpp` (`readRPM()`, simulator, sweep).
2. **Interpolation** — `ignition.cpp::interpolateTiming()` /
   `calculateBaseTiming()`.
3. **Active map** — `config.activeMap`, read/written via
   `webserver.cpp` (`/api/map/select`) and used throughout `ignition.cpp`.
4. **Rumble Idle** — `ignition.cpp::applyRumbleIdle()`,
   `config.h::RumbleConfig`, `/api/rumble`.
5. **Backfire** — `ignition.cpp::applyBackfire()` (non-blocking pulse via
   `millis()`), `config.h::BackfireConfig`, `/api/backfire`.
6. **Rev Limiter** — `ignition.cpp::applyRevLimiter()`,
   `config.h::LimiterConfig`, `/api/revlimit`.
7. **Configuration storage** — `storage.h`/`storage.cpp` (LittleFS +
   ArduinoJson, `/config.json`).
8. **Web UI** — `webserver.cpp` (REST + static file serving) and
   `data/index.html` / `style.css` / `app.js`.
9. **Future pickup sensor** — replace the body of `rpm.cpp::readRPM()`
   (and add interrupt/ISR bookkeeping in `rpmUpdate()`); no other module
   needs to change because everything else only calls `readRPM()`.
10. **Future ignition hardware** — replace the body of
    `hardware.cpp::scheduleIgnition()` with a real timer/interrupt-driven
    driver once a trigger-angle reference is available; `ignition.cpp`
    already calls `scheduleIgnition()` with the final computed timing, so
    no calling code needs to change.

## Key architecture decisions

- **No `delay()` anywhere in the control path** — `loop()` only calls
  `rpmUpdate()`, `readRPM()`, `calculateIgnitionTiming()`,
  `scheduleIgnition()`, and `webServerHandle()`, all non-blocking, so the
  Wi-Fi/HTTP stack is never starved.
- **JSON-based config storage** instead of raw struct dumps, so the file
  stays human-readable and tolerant of future field additions (version
  check + validation falls back to defaults instead of crashing on a
  corrupt/old file).
- **`MAP_POINTS` is computed from `RPM_MIN`/`RPM_MAX`/`RPM_STEP`**, never
  hard-coded, so changing the resolution later is a one-line edit in
  `config.h`.
- **Ignition math is fully decoupled from Wi-Fi/HTTP/storage** — `ignition.h`/
  `.cpp` only include `config.h` and `Arduino.h`, making it trivial to unit
  test or later move onto a hardware timer/ISR without touching the web
  layer.
- **No high-voltage output anywhere** — `hardware.cpp` only stores a float;
  this is a deliberate, permanent boundary for V1, documented in the header
  comments so it isn't accidentally crossed during future edits.
