// =============================================================
// app.js
// No external dependencies / no CDN. Talks to the ESP8266 REST
// API only. Polls /api/status every 200ms for live dashboard data.
// =============================================================

const POLL_INTERVAL_MS = 200;

let currentConfig = null;   // cached /api/config response
let currentMapPoints = [];  // cached /api/map points for the active map

// ---------------------------------------------------------------
// Tabs
// ---------------------------------------------------------------
document.querySelectorAll(".tab-btn").forEach(btn => {
  btn.addEventListener("click", () => {
    document.querySelectorAll(".tab-btn").forEach(b => b.classList.remove("active"));
    document.querySelectorAll(".tab-content").forEach(c => c.classList.remove("active"));
    btn.classList.add("active");
    document.getElementById("tab-" + btn.dataset.tab).classList.add("active");
  });
});

// ---------------------------------------------------------------
// Fetch helpers
// ---------------------------------------------------------------
async function apiGet(path) {
  const res = await fetch(path);
  return res.json();
}

async function apiPost(path, body) {
  const res = await fetch(path, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body || {})
  });
  return res.json();
}

// ---------------------------------------------------------------
// Dashboard polling
// ---------------------------------------------------------------
function updateDashboard(s) {
  document.getElementById("dashRpm").textContent = s.rpm + " RPM";
  document.getElementById("dashTiming").textContent = s.timing.toFixed(1) + "\u00B0 BTDC";
  document.getElementById("dashMap").textContent = s.mapName;
  document.getElementById("dashRumble").textContent = s.rumbleIdle ? "ON" : "OFF";
  document.getElementById("dashBackfire").textContent = s.backfire ? "ON" : "OFF";
  document.getElementById("dashLimiter").textContent = s.limiterState;

  document.getElementById("detBase").textContent = s.baseTiming.toFixed(1) + "\u00B0";
  document.getElementById("detFinal").textContent = s.timing.toFixed(1) + "\u00B0";
  document.getElementById("detStarting").textContent = s.starting ? "YES" : "NO";
  document.getElementById("detIdle").textContent = s.idle ? "YES" : "NO";
  document.getElementById("detSource").textContent = s.rpmSource;
  document.getElementById("detSystem").textContent = s.systemState;

  document.getElementById("systemState").textContent = s.systemState;
}

async function pollStatus() {
  try {
    const s = await apiGet("/api/status");
    if (s.success) updateDashboard(s);
  } catch (e) {
    // network hiccup, ignore and retry next tick
  }
  setTimeout(pollStatus, POLL_INTERVAL_MS);
}

// ---------------------------------------------------------------
// Ignition map: graph + table
// ---------------------------------------------------------------
function drawCurve(points) {
  const canvas = document.getElementById("curveCanvas");
  const ctx = canvas.getContext("2d");
  const w = canvas.width, h = canvas.height;
  const padL = 45, padR = 10, padT = 10, padB = 25;

  ctx.clearRect(0, 0, w, h);

  if (!points.length) return;

  const rpmMin = points[0].rpm;
  const rpmMax = points[points.length - 1].rpm;
  let tMin = Math.min(...points.map(p => p.timing));
  let tMax = Math.max(...points.map(p => p.timing));
  if (tMin === tMax) { tMin -= 1; tMax += 1; }
  tMin -= 2; tMax += 2;

  const xForRpm = r => padL + ((r - rpmMin) / (rpmMax - rpmMin)) * (w - padL - padR);
  const yForT = t => (h - padB) - ((t - tMin) / (tMax - tMin)) * (h - padT - padB);

  // Minor grid every 250 RPM, major label every 1000 RPM.
  ctx.strokeStyle = "#1c232c";
  ctx.fillStyle = "#8a95a3";
  ctx.font = "10px sans-serif";
  for (let r = rpmMin; r <= rpmMax; r += 250) {
    const x = xForRpm(r);
    ctx.beginPath();
    ctx.moveTo(x, padT);
    ctx.lineTo(x, h - padB);
    ctx.stroke();
    if (r % 1000 === 0) {
      ctx.fillText(String(r), x - 10, h - 8);
    }
  }

  // Y axis labels
  const steps = 5;
  for (let i = 0; i <= steps; i++) {
    const t = tMin + (i / steps) * (tMax - tMin);
    const y = yForT(t);
    ctx.strokeStyle = "#1c232c";
    ctx.beginPath();
    ctx.moveTo(padL, y);
    ctx.lineTo(w - padR, y);
    ctx.stroke();
    ctx.fillText(t.toFixed(0) + "\u00B0", 4, y + 3);
  }

  // Curve
  ctx.strokeStyle = "#2f6fed";
  ctx.lineWidth = 2;
  ctx.beginPath();
  points.forEach((p, i) => {
    const x = xForRpm(p.rpm), y = yForT(p.timing);
    if (i === 0) ctx.moveTo(x, y); else ctx.lineTo(x, y);
  });
  ctx.stroke();
}

function renderMapTable(points) {
  const tbody = document.querySelector("#mapTable tbody");
  tbody.innerHTML = "";
  points.forEach((p, idx) => {
    const tr = document.createElement("tr");

    const tdRpm = document.createElement("td");
    tdRpm.textContent = p.rpm;
    tr.appendChild(tdRpm);

    const tdTiming = document.createElement("td");
    const input = document.createElement("input");
    input.type = "number";
    input.step = "0.1";
    input.value = p.timing;
    input.dataset.index = idx;
    input.addEventListener("change", () => {
      currentMapPoints[idx].timing = parseFloat(input.value);
      drawCurve(currentMapPoints);
    });
    tdTiming.appendChild(input);
    tr.appendChild(tdTiming);

    tbody.appendChild(tr);
  });
}

async function loadMap() {
  const m = await apiGet("/api/map");
  if (!m.success) return;
  currentMapPoints = m.points;
  document.getElementById("mapActiveLabel").textContent = "MAP " + (m.map + 1);
  drawCurve(currentMapPoints);
  renderMapTable(currentMapPoints);
}

document.getElementById("btnApplyMap").addEventListener("click", async () => {
  const resp = await apiPost("/api/map", { points: currentMapPoints });
  alert(resp.success ? "Map applied" : ("Error: " + resp.error));
});

document.getElementById("btnSaveConfig").addEventListener("click", async () => {
  const resp = await apiPost("/api/config/save", {});
  alert(resp.success ? "Configuration saved" : ("Error: " + resp.error));
});

document.getElementById("btnResetConfig").addEventListener("click", async () => {
  if (!confirm("Reset configuration to defaults? This cannot be undone.")) return;
  const resp = await apiPost("/api/config/reset", {});
  if (resp.success) { await loadMap(); await loadConfigForms(); }
});

// ---------------------------------------------------------------
// Mapping selector
// ---------------------------------------------------------------
document.querySelectorAll(".map-select-btn").forEach(btn => {
  btn.addEventListener("click", async () => {
    const idx = parseInt(btn.dataset.map, 10);
    const resp = await apiPost("/api/map/select", { map: idx });
    if (resp.success) {
      document.querySelectorAll(".map-select-btn").forEach(b => b.classList.remove("active"));
      btn.classList.add("active");
      // loadMap() pulls the freshly-active map's curve from the
      // firmware and updates mapActiveLabel/graph/table together, so
      // the label and the curve can never disagree after a switch.
      await loadMap();
      await loadConfigForms();
    }
  });
});

// ---------------------------------------------------------------
// Config-backed forms (rumble/backfire/limiter/starting/idle)
// ---------------------------------------------------------------
async function loadConfigForms() {
  const c = await apiGet("/api/config");
  if (!c.success) return;
  currentConfig = c;

  const map = c.maps[c.activeMap];

  document.getElementById("rumbleEnabled").checked = map.rumble.enabled;
  document.getElementById("rumbleTarget").value = map.rumble.targetRPM;
  document.getElementById("rumbleMin").value = map.rumble.minRPM;
  document.getElementById("rumbleMax").value = map.rumble.maxRPM;
  document.getElementById("rumbleRetard").value = map.rumble.retard;

  document.getElementById("backfireEnabled").checked = map.backfire.enabled;
  document.getElementById("backfireMin").value = map.backfire.minRPM;
  document.getElementById("backfireMax").value = map.backfire.maxRPM;
  document.getElementById("backfireRetard").value = map.backfire.retard;
  document.getElementById("backfireDuration").value = map.backfire.duration;
  document.getElementById("backfireCooldown").value = map.backfire.cooldown;

  document.getElementById("limiterMin").value = map.limiter.minRPM;
  document.getElementById("limiterMax").value = map.limiter.maxRPM;
  document.getElementById("softLimit").value = map.limiter.softLimitRPM;
  document.getElementById("hardLimit").value = map.limiter.hardLimitRPM;

  document.getElementById("dashLimiterRange").textContent =
    map.limiter.minRPM + "-" + map.limiter.maxRPM + " RPM range";

  document.getElementById("startingAdvance").value = c.startingAdvance;
  document.getElementById("startingThreshold").value = c.startingRPMThreshold;

  document.getElementById("idleEnabled").checked = map.idleEnabled;
  document.getElementById("idleRpm").value = map.idleRPM;
  document.getElementById("idleTiming").value = map.idleTiming;

  document.querySelectorAll(".map-select-btn").forEach(b => {
    b.classList.toggle("active", parseInt(b.dataset.map, 10) === c.activeMap);
  });
  document.getElementById("mapActiveLabel").textContent = "MAP " + (c.activeMap + 1);
}

document.getElementById("btnSaveRumble").addEventListener("click", async () => {
  const resp = await apiPost("/api/rumble", {
    enabled: document.getElementById("rumbleEnabled").checked,
    targetRPM: parseInt(document.getElementById("rumbleTarget").value, 10),
    minRPM: parseInt(document.getElementById("rumbleMin").value, 10),
    maxRPM: parseInt(document.getElementById("rumbleMax").value, 10),
    retard: parseFloat(document.getElementById("rumbleRetard").value)
  });
  alert(resp.success ? "Rumble idle updated" : ("Error: " + resp.error));
});

document.getElementById("btnSaveBackfire").addEventListener("click", async () => {
  const resp = await apiPost("/api/backfire", {
    enabled: document.getElementById("backfireEnabled").checked,
    minRPM: parseInt(document.getElementById("backfireMin").value, 10),
    maxRPM: parseInt(document.getElementById("backfireMax").value, 10),
    retard: parseFloat(document.getElementById("backfireRetard").value),
    duration: parseInt(document.getElementById("backfireDuration").value, 10),
    cooldown: parseInt(document.getElementById("backfireCooldown").value, 10)
  });
  alert(resp.success ? "Backfire updated" : ("Error: " + resp.error));
});

document.getElementById("btnSaveLimiter").addEventListener("click", async () => {
  const resp = await apiPost("/api/revlimit", {
    minRPM: parseInt(document.getElementById("limiterMin").value, 10),
    maxRPM: parseInt(document.getElementById("limiterMax").value, 10),
    softLimitRPM: parseInt(document.getElementById("softLimit").value, 10),
    hardLimitRPM: parseInt(document.getElementById("hardLimit").value, 10)
  });
  if (resp.success) {
    await loadConfigForms();
  }
  alert(resp.success ? "Rev limiter updated" : ("Error: " + resp.error));
});

document.getElementById("btnSaveStarting").addEventListener("click", async () => {
  const resp = await apiPost("/api/settings", {
    startingAdvance: parseFloat(document.getElementById("startingAdvance").value),
    startingRPMThreshold: parseInt(document.getElementById("startingThreshold").value, 10)
  });
  alert(resp.success ? "Starting settings updated" : ("Error: " + resp.error));
});

document.getElementById("btnSaveIdle").addEventListener("click", async () => {
  const resp = await apiPost("/api/settings", {
    idleEnabled: document.getElementById("idleEnabled").checked,
    idleRPM: parseInt(document.getElementById("idleRpm").value, 10),
    idleTiming: parseFloat(document.getElementById("idleTiming").value)
  });
  alert(resp.success ? "Idle settings updated" : ("Error: " + resp.error));
});

// ---------------------------------------------------------------
// RPM simulator
// ---------------------------------------------------------------
document.getElementById("simEnabled").addEventListener("change", async (e) => {
  await apiPost("/api/simulator", { enabled: e.target.checked });
});

document.getElementById("btnSetRpm").addEventListener("click", async () => {
  const rpm = parseInt(document.getElementById("simRpm").value, 10);
  await apiPost("/api/rpm", { rpm });
});

document.getElementById("btnRpmMinus").addEventListener("click", async () => {
  await apiPost("/api/rpm", { rpm: 0, step: -1 });
});

document.getElementById("btnRpmPlus").addEventListener("click", async () => {
  await apiPost("/api/rpm", { rpm: 0, step: 1 });
});

document.getElementById("btnSweepStart").addEventListener("click", async () => {
  await apiPost("/api/simulator", {
    sweep: "start",
    startRpm: parseInt(document.getElementById("sweepStart").value, 10),
    endRpm: parseInt(document.getElementById("sweepEnd").value, 10),
    step: parseInt(document.getElementById("sweepStep").value, 10),
    interval: parseInt(document.getElementById("sweepInterval").value, 10)
  });
});

document.getElementById("btnSweepStop").addEventListener("click", async () => {
  await apiPost("/api/simulator", { sweep: "stop" });
});

// ---------------------------------------------------------------
// Settings tab
// ---------------------------------------------------------------
document.getElementById("btnSave2").addEventListener("click", async () => {
  const resp = await apiPost("/api/config/save", {});
  alert(resp.success ? "Saved" : ("Error: " + resp.error));
});

document.getElementById("btnLoad2").addEventListener("click", async () => {
  await apiPost("/api/config/load", {});
  await loadMap();
  await loadConfigForms();
});

document.getElementById("btnReset2").addEventListener("click", async () => {
  if (!confirm("Reset configuration to defaults? This cannot be undone.")) return;
  await apiPost("/api/config/reset", {});
  await loadMap();
  await loadConfigForms();
});

// ---------------------------------------------------------------
// Boot
// ---------------------------------------------------------------
(async function init() {
  await loadMap();
  await loadConfigForms();
  pollStatus();
})();
