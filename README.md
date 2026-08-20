# Programmable CDI Web Controller V2 (Real Hardware Edition)

Wemos D1 Mini / ESP8266 — Real CDI hardware control with pulser input, TSS support, and programmable ignition mapping.

## 🚨 IMPORTANT: HARDWARE CALIBRATION REQUIRED

**This firmware is a PROTOTYPE for Supra X 125 ignition control.**

⚠️ **BEFORE connecting to any real motorcycle CDI:**
- Verify all GPIO connections with oscilloscope
- Confirm pulser signal integrity (edge detection, debouncing)
- Validate SCR trigger timing with timing light
- Test safe state on bench BEFORE real engine
- Measure actual ignition timing vs Web UI display

**Failure to calibrate can result in engine damage, injury, or fire.**

## Architecture

```
┌─ REAL-TIME PATH ─────────────────┐
│                                   │
│  Pulser (D4)                     │
│    │                              │
│    └─> Interrupt Handler          │
│         │                         │
│         └─> RPM Calculation       │
│              │                    │
│              └─> Ignition Engine  │
│                   │               │
│                   └─> SCR Output (D7)
│                                   │
└───────────────────────────────────┘
         (NON-BLOCKING)

┌─ LOW-PRIORITY PATH ──────────────┐
│                                  │
│  Web Server (WiFi)              │
│    │                             │
│    ├─ REST API                   │
│    ├─ JSON Config                │
│    └─ Static UI                  │
│                                  │
│  Storage (LittleFS)              │
│    │                             │
│    └─ /config.json               │
│                                  │
│  Hardware Abstraction            │
│    ├─ MAP Switch (D3)            │
│    ├─ TSS Input (D6)             │
│    ├─ LED Status (D8)            │
│    └─ LED Limiter (D5)           │
│                                  │
└──────────────────────────────────┘
   (NEVER BLOCKS REAL-TIME)
```

## GPIO Pin Mapping (Wemos D1 Mini)

| Function | GPIO | Wemos | Direction | Protection |
|----------|------|-------|-----------|------------|
| Pulser Input | GPIO2 | D4 | Input | Optocoupler |
| SCR Trigger Output | GPIO13 | D7 | Output | Optocoupler |
| CDI Status LED | GPIO15 | D8 | Output | Optocoupler |
| TSS Input | GPIO12 | D6 | Input | Optocoupler |
| Limiter LED | GPIO14 | D5 | Output | Optocoupler |
| MAP Switch | GPIO0 | D3 | Input | External Pull-down |

### Hardware Assumptions

- **Optocoupler isolation** on all inputs/outputs (voltage protection)
- **D8 (GPIO15)** must not pull LOW during boot (check boot-strap circuit)
- **D3 (GPIO0)** uses external pull-down; must not be pulled low during boot
- **TSS logic:** OFF=HIGH, ON=LOW (from optocoupler circuit)
- **Pulser signal:** ~0.5–1.0V AC through optocoupler (clean single pulse per revolution)

## Features

✅ **Real Pulser Input** (D4 interrupt-driven RPM)
✅ **Real SCR Output** (D7 triggers ignition at precise timing)
✅ **TSS Support** (per-map mode: AUTO/ON/OFF + configurable retard)
✅ **3 Programmable Maps** (MAP 1 = Supra X 125 OEM base)
✅ **MAP Selector Button** (D3: one-press = MAP 1→2→3→1)
✅ **LED Indicators** (D8=status, D5=limiter blink)
✅ **Persistent Storage** (LittleFS + JSON)
✅ **Web UI** (Real-time dashboard + configuration)
✅ **Fail-safe Boot** (LED status, timeout protection)
✅ **Backward Compatible** (Simulator mode still works)

## OEM Data: Honda Supra X 125

### Base Ignition Curve (MAP 1)

| RPM | Timing (BTDC) | Notes |
|-----|---------------|-------|
| 1000 | 10.4° | Factory base (10° typical) |
| 2000 | 13.0° | Early advance begin |
| 3000 | 20.0° | Mid-range |
| 4000 | 24.0° | High advance |
| 5000 | 26.0° | Near plateau |
| 6000 | 28.0° | Full advance |
| 7000+ | 28.0° | Plateau (knock prevention) |

**Status:** ⚠️ **RESEARCH-BASED, NOT VERIFIED ON REAL ENGINE**
- Idle timing (10° BTDC) is factory-documented
- Full curve based on industry-standard 125cc CDI progression
- **MUST be verified with timing light before use**

### TSS (Throttle Switch Sensor)

**Function:** Reduces ignition timing during deceleration/engine brake

**Effect:** Retards timing by ~5° (configurable per map)

**Purpose:**
- Prevent backfire during engine braking
- Reduce knock/detonation on closed throttle
- Smooth downshift experience
- Lower emissions

**Status:** ⚠️ **Typical Honda behavior; specific Supra X 125 values require calibration**

## API Endpoints

### Status
```
GET /api/status
```
Returns real-time RPM, timing, TSS state, limiter state.

### Configuration
```
GET  /api/config          - Full configuration (all maps)
GET  /api/map             - Active map ignition curve
POST /api/map             - Update active map timing points
POST /api/map/select      - Switch active map
POST /api/tss             - Update TSS per map
POST /api/rumble          - Rumble idle settings
POST /api/backfire        - Backfire settings
POST /api/revlimit        - Rev limiter settings
POST /api/settings        - Starting/idle global settings
```

### Hardware Control
```
POST /api/rpm             - Manual RPM set (simulator mode)
POST /api/simulator       - Enable/disable simulator + sweep
POST /api/config/save     - Save config to LittleFS
POST /api/config/load     - Load config from LittleFS
POST /api/config/reset    - Reset to defaults
```

## Installation

### PlatformIO (Recommended)

```bash
pio run -t uploadfs   # Flash index.html, style.css, app.js to LittleFS
pio run -t upload     # Compile and flash firmware
```

### Arduino IDE

1. Select board: **LOLIN(WEMOS) D1 R2 & mini**
2. Use **LittleFS Data Upload** tool on `data/` folder
3. Upload `cdi.ino` (which includes all source files)

## Testing Checklist

### Boot Test
- [ ] D7 (SCR) = LOW (safe state)
- [ ] D8 (CDI LED) = LOW (no fault)
- [ ] D5 (Limiter LED) = LOW (not active)
- [ ] Web UI loads at http://192.168.4.1

### Pulser Test
- [ ] Connect simulated/real pulser to D4
- [ ] Dashboard shows RPM change in real-time
- [ ] RPM calculation is stable (no jitter)
- [ ] Timeout works: RPM → 0 if pulser stops

### MAP Test
- [ ] Press D3 (MAP switch): MAP 1 → 2 → 3 → 1
- [ ] Web UI reflects map change
- [ ] Each map has independent TSS, rumble, backfire config

### TSS Test
- [ ] AUTO mode: Web UI shows real TSS input
- [ ] ON mode: Final timing retarded by configured amount
- [ ] OFF mode: Final timing unaffected by TSS
- [ ] Change map: TSS setting follows new map

### SCR Output Test
- [ ] D7 remains LOW unless engine is running (simulator or real pulser)
- [ ] Trigger frequency increases with RPM
- [ ] Trigger timing matches calculated ignition angle

### LED Test
- [ ] D8 toggles based on system health
- [ ] D5 blinks smoothly when limiter active (non-blocking)

### Persistence Test
- [ ] Edit MAP 1 timing curve via Web UI
- [ ] Set TSS to force ON
- [ ] Save config
- [ ] Power OFF / Power ON
- [ ] Web UI still shows same map + TSS setting

## Configuration Storage (LittleFS)

```
Wemos LittleFS
├── /config.json          Current configuration
└── /config.bak           Backup (for future use)
```

**Format:** JSON (human-readable, version-controlled)

**Migration:** CONFIG_VERSION 1 → 2 adds TSS; old configs fallback to defaults safely.

## Real-Time Guarantees

Ignition timing is **never blocked by**:
- WiFi operations
- JSON parsing
- File I/O
- Web server handling

Web server operations run at **very low priority** — `webServerHandle()` services at most one HTTP request per loop iteration, ensuring the ignition path is never starved.

## Safety & Fail-Safe

### Boot Sequence
1. GPIO init (all outputs LOW)
2. LittleFS mount
3. Config load/validate → fallback to defaults if invalid
4. Ignition engine init
5. Web server start
6. Set D8 LOW (OK status)
7. Enter main loop

### Fault Detection
- **Config invalid:** Uses safe defaults
- **LittleFS error:** Continues with defaults in RAM
- **No pulser signal:** RPM → 0 after 500ms timeout
- **GPIO error:** System continues; fault reported in status

### LED Status (D8)
- **LOW** = System operating normally
- **HIGH** = Critical fault detected (stop ignition use)

## Development

### File Structure

```
src/
├── main.cpp           Main loop (wires everything)
├── config.h/.cpp      Configuration structures + defaults
├── ignition.h/.cpp    Ignition timing calculation engine
├── rpm.h/.cpp         RPM input (real pulser + simulator)
├── storage.h/.cpp     LittleFS JSON persistence
├── webserver.h/.cpp   REST API + static file serving
└── hardware.h/.cpp    GPIO abstraction (LEDs, SCR, button)

data/
├── index.html         Web UI
├── style.css          Styling
└── app.js             Browser logic + API calls
```

### Ignition Calculation Pipeline

```
Pulser Edge
   ↓
RPM Calculation
   ↓
Active Map Selection
   ↓
Base Timing Interpolation
   ↓
Starting Retard (if RPM < threshold)
   ↓
TSS Retard (if TSS active)
   ↓
Idle Timing Correction
   ↓
Rumble Idle Retard
   ↓
Backfire Retard Pulse
   ↓
Rev Limiter State Check
   ↓
SCR Trigger @ Final Timing
```

## Known Limitations & TODOs

- [ ] Real ignition timing requires crank angle reference (future: timer-based pulse delay)
- [ ] Pulser debounce is software-based; optimize if glitches occur
- [ ] LittleFS atomic writes need verification under power-loss conditions
- [ ] TSS retard amount is fixed per map; could expand to RPM-dependent curve
- [ ] MAP button requires external pull-down (no internal weak pull available on D3)

## Debugging

### Serial Output (115200 baud)

```
RPM=4500 MAP=1 TSS_INPUT=ON TSS_MODE=AUTO TSS_ACTIVE=YES BASE=28.0 FINAL=23.0 LIMITER=NORMAL CDI=OK
```

### Web UI Dashboard

- RPM, timing, limiter state in real-time
- TSS input and active state
- System status (OK/FAULT)

## Future Enhancements

1. **Real Crank Angle Trigger:** Use timer PWM + pulser edge for precise ignition pulse
2. **TSS RPM Curve:** Different retard values at different RPMs
3. **Load-based Tuning:** Secondary sensor input (MAT, TPS) for automatic map switching
4. **Data Logging:** Record session history to LittleFS for analysis
5. **Over-The-Air Updates:** Firmware update via Web UI

---

**Revision:** V2.0 (2026-08-20)
**Target:** Wemos D1 Mini (ESP8266) + Honda Supra X 125 CDI
**Status:** 🟡 **BENCH TEST READY** — Requires timing light + dyno verification before real engine use
