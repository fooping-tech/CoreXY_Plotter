const SOFT_LIMIT = { minX: 0, maxX: 55, minY: 0, maxY: 55 };
const API_TIMEOUT_MS = 5000;

const app = {
  page: "dashboard",
  state: null,
  logs: [],
  errors: 0,
  filter: "all",
  jogStep: 1,
  gcodeText: "",
  preview: null,
};

const $ = (id) => document.getElementById(id);

function formatNumber(value) {
  return typeof value === "number" && Number.isFinite(value) ? value.toFixed(2) : "--";
}

function machineStateClass(stateName) {
  if (stateName === "READY") return "ready";
  if (stateName === "ALARM") return "alarm";
  if (stateName === "NEED HOME" || stateName === "HOMING") return "warning";
  return "unknown";
}

function setPage(page) {
  app.page = page;
  document.querySelectorAll(".page").forEach((el) => el.classList.toggle("active", el.id === page));
  document.querySelectorAll(".nav").forEach((el) => el.classList.toggle("active", el.dataset.page === page));
}

function getMachine() {
  return app.state?.machine || {
    state: "UNKNOWN",
    x: null,
    y: null,
    pen: "UNKNOWN",
    homed: false,
    alarmed: false,
    homing: false,
    limits: { x: "UNKNOWN", y: "UNKNOWN" },
    tmc: "UNKNOWN",
  };
}

function isConnected() {
  return Boolean(app.state?.connected && app.state?.port);
}

function isJobRunning() {
  return Boolean(app.state?.jobRunning);
}

function canManualMove() {
  const machine = getMachine();
  return isConnected() && machine.homed && !machine.alarmed && !machine.homing && !isJobRunning();
}

function disabledReason() {
  const machine = getMachine();
  if (!isConnected()) return "Connect serial port first";
  if (isJobRunning()) return "Job running";
  if (machine.alarmed) return "Alarm active";
  if (machine.homing) return "Homing in progress";
  if (!machine.homed) return "Home required";
  return "Manual controls enabled";
}

function updateStateUI() {
  const machine = getMachine();
  const stateName = machine.state || "UNKNOWN";
  const stateClass = machineStateClass(stateName);
  const pill = $("statePill");
  pill.textContent = stateName;
  pill.className = `state-pill ${stateClass}`;

  $("topX").textContent = formatNumber(machine.x);
  $("topY").textContent = formatNumber(machine.y);
  $("topPen").textContent = machine.pen || "UNKNOWN";
  $("connectionText").textContent = isConnected()
    ? `Port ${app.state.port} @ ${app.state.baud}`
    : "Disconnected";
  $("topAbortBtn").classList.toggle("hidden", !isJobRunning());

  $("dashboardState").textContent = stateName;
  $("dashboardStateNote").textContent = stateName === "READY"
    ? "Homed, no alarm"
    : stateName === "ALARM"
      ? "Alarm active"
      : stateName === "NEED HOME"
        ? "Home required before motion"
        : stateName === "HOMING"
          ? "Homing in progress"
          : "No trusted machine state";
  const hero = document.querySelector(".hero-state");
  hero.className = `panel hero-state ${stateClass}`;

  $("dashX").textContent = formatNumber(machine.x);
  $("dashY").textContent = formatNumber(machine.y);
  $("tilePen").textContent = machine.pen || "UNKNOWN";
  $("tileHome").textContent = machine.homed ? "HOMED" : "NOT HOMED";
  $("tileLimits").textContent = `X ${machine.limits?.x || "?"} / Y ${machine.limits?.y || "?"}`;
  $("tileTmc").textContent = machine.tmc || "UNKNOWN";
  $("disabledReason").textContent = disabledReason();

  $("homeBtn").disabled = !isConnected() || machine.homing;
  $("clearAlarmBtn").disabled = !isConnected() || !machine.alarmed;
  ["jogUp", "jogDown", "jogLeft", "jogRight", "penUpBtn", "penDownBtn"].forEach((id) => {
    $(id).disabled = !canManualMove();
  });
  $("sendJobBtn").disabled = !canSendJob();
  $("abortJobBtn").disabled = !isConnected();
  $("topAbortBtn").disabled = !isConnected();
}

function canSendJob() {
  const machine = getMachine();
  return Boolean(
    isConnected() &&
      app.gcodeText.trim() &&
      app.preview &&
      !isJobRunning() &&
      !machine.alarmed &&
      machine.homed &&
      app.preview.warnings.length === 0,
  );
}

async function api(path, options = {}) {
  const { timeoutMs = API_TIMEOUT_MS, ...fetchOptions } = options;
  const controller = new AbortController();
  const timeoutId = window.setTimeout(() => controller.abort(), timeoutMs);
  const response = await fetch(path, {
    headers: { "Content-Type": "application/json", ...(fetchOptions.headers || {}) },
    signal: controller.signal,
    ...fetchOptions,
  }).catch((error) => {
    if (error.name === "AbortError") throw new Error(`${path} timed out`);
    throw error;
  });
  try {
    const payload = await response.json().catch(() => ({}));
    if (!response.ok) throw new Error(payload.error || `${response.status} ${response.statusText}`);
    return payload;
  } finally {
    window.clearTimeout(timeoutId);
  }
}

async function refreshState() {
  app.state = await api("/api/state");
  updateStateUI();
}

async function refreshPorts() {
  const select = $("portSelect");
  select.innerHTML = "";
  let ports = [];
  try {
    appendLog({ time: Date.now(), kind: "host", message: "Refreshing serial ports" });
    ({ ports } = await api("/api/ports", { timeoutMs: 2500 }));
  } catch (error) {
    const option = document.createElement("option");
    option.value = "";
    option.textContent = "Use manual port";
    select.appendChild(option);
    appendLog({
      time: Date.now(),
      kind: "error",
      message: `Port refresh failed: ${error.message || error}. Manual port is still available.`,
    });
    return;
  }
  ports.forEach((port) => {
    const option = document.createElement("option");
    option.value = port;
    option.textContent = port;
    select.appendChild(option);
  });
  if (ports.length === 0) {
    const option = document.createElement("option");
    option.value = "";
    option.textContent = "Use manual port";
    select.appendChild(option);
    appendLog({ time: Date.now(), kind: "host", message: "No serial ports found. Use manual port." });
  } else {
    appendLog({ time: Date.now(), kind: "host", message: `Found ${ports.length} serial port(s)` });
  }
}

function appendLog(event) {
  if (event.kind === "ping") return;
  app.logs.push(event);
  if (app.logs.length > 600) app.logs.shift();
  if (event.kind === "error") app.errors += 1;
  $("errorCount").textContent = String(app.errors);
  renderLogs();
}

function renderLogs() {
  const recent = app.logs.slice(-8);
  renderLogList($("recentLog"), recent, "all");
  renderLogList($("consoleLog"), app.logs, app.filter);
}

function renderLogList(container, logs, filter) {
  container.innerHTML = "";
  logs
    .filter((event) => filter === "all" || event.kind === filter)
    .slice(-260)
    .forEach((event) => {
      const line = document.createElement("div");
      line.className = `log-line ${event.kind}`;
      line.textContent = `${new Date(event.time).toLocaleTimeString()} ${event.message}`;
      container.appendChild(line);
    });
  container.scrollTop = container.scrollHeight;
}

function startEvents() {
  const events = new EventSource("/api/events");
  events.onmessage = (message) => {
    const event = JSON.parse(message.data);
    appendLog(event);
    refreshState().catch(() => {});
  };
  events.onerror = () => {
    appendLog({ time: Date.now(), kind: "error", message: "Event stream disconnected" });
  };
}

async function sendCommand(command) {
  appendLog({ time: Date.now(), kind: "sent", message: `> ${command}` });
  await api("/api/command", {
    method: "POST",
    body: JSON.stringify({ command }),
  });
}

function jog(dx, dy) {
  const machine = getMachine();
  const x = Number(machine.x || 0) + dx * app.jogStep;
  const y = Number(machine.y || 0) + dy * app.jogStep;
  sendCommand(`XY ${x.toFixed(3)} ${y.toFixed(3)}`).catch(showError);
}

function showError(error) {
  appendLog({ time: Date.now(), kind: "error", message: error.message || String(error) });
}

function parseWords(line) {
  const words = {};
  const clean = line.split(";")[0].trim();
  for (const match of clean.matchAll(/([A-Z])\s*(-?\d+(?:\.\d+)?)/gi)) {
    words[match[1].toUpperCase()] = Number(match[2]);
  }
  return words;
}

function parseGcode(text) {
  let x = 0;
  let y = 0;
  let absolute = true;
  let units = 1;
  let penDown = false;
  const segments = [];
  const warnings = [];
  let minX = Infinity;
  let minY = Infinity;
  let maxX = -Infinity;
  let maxY = -Infinity;
  let lines = 0;

  function includePoint(px, py) {
    minX = Math.min(minX, px);
    minY = Math.min(minY, py);
    maxX = Math.max(maxX, px);
    maxY = Math.max(maxY, py);
  }

  text.split(/\r?\n/).forEach((raw, index) => {
    const lineNumber = index + 1;
    const line = raw.trim().toUpperCase();
    if (!line || line.startsWith(";") || line === "%") return;
    lines += 1;
    const words = parseWords(line);

    if (line.startsWith("G20")) {
      units = 25.4;
      return;
    }
    if (line.startsWith("G21")) {
      units = 1;
      return;
    }
    if (line.startsWith("G90")) {
      absolute = true;
      return;
    }
    if (line.startsWith("G91")) {
      absolute = false;
      return;
    }
    if (line.startsWith("M3")) {
      penDown = true;
      return;
    }
    if (line.startsWith("M5")) {
      penDown = false;
      return;
    }
    if (line.startsWith("G28")) {
      segments.push({ type: "home", x1: x, y1: y, x2: x, y2: y, lineNumber });
      return;
    }
    if (line.startsWith("G4")) {
      segments.push({ type: "dwell", x1: x, y1: y, x2: x, y2: y, lineNumber });
      return;
    }
    if (line.startsWith("G0") || line.startsWith("G1")) {
      const nextX = words.X === undefined ? x : absolute ? words.X * units : x + words.X * units;
      const nextY = words.Y === undefined ? y : absolute ? words.Y * units : y + words.Y * units;
      const out =
        nextX < SOFT_LIMIT.minX ||
        nextX > SOFT_LIMIT.maxX ||
        nextY < SOFT_LIMIT.minY ||
        nextY > SOFT_LIMIT.maxY;
      segments.push({
        type: out ? "out" : penDown ? "draw" : "travel",
        x1: x,
        y1: y,
        x2: nextX,
        y2: nextY,
        lineNumber,
      });
      includePoint(x, y);
      includePoint(nextX, nextY);
      if (out) warnings.push(`Line ${lineNumber}: segment leaves soft limit`);
      x = nextX;
      y = nextY;
      return;
    }
    warnings.push(`Line ${lineNumber}: unsupported preview command "${raw.trim()}"`);
  });

  if (!Number.isFinite(minX)) {
    minX = minY = maxX = maxY = 0;
  }
  return { lines, segments, warnings, bounds: { minX, minY, maxX, maxY } };
}

function renderPreview(preview) {
  const width = 720;
  const height = 482;
  const pad = 36;
  const scale = Math.min((width - pad * 2) / 55, (height - pad * 2) / 55);
  const ox = (width - 55 * scale) / 2;
  const oy = (height - 55 * scale) / 2;
  const tx = (x) => ox + x * scale;
  const ty = (y) => oy + (55 - y) * scale;
  const line = (segment, cls) =>
    `<line class="${cls}" x1="${tx(segment.x1)}" y1="${ty(segment.y1)}" x2="${tx(segment.x2)}" y2="${ty(segment.y2)}" />`;

  const parts = [
    `<svg viewBox="0 0 ${width} ${height}" role="img" aria-label="G-code preview">`,
    `<rect class="soft-limit" x="${tx(0)}" y="${ty(55)}" width="${55 * scale}" height="${55 * scale}" />`,
  ];
  preview.segments.forEach((segment) => {
    if (segment.type === "draw") parts.push(line(segment, "draw-path"));
    else if (segment.type === "travel") parts.push(line(segment, "travel-path"));
    else if (segment.type === "out") parts.push(line(segment, "error-path"));
  });
  if (preview.bounds.maxX > preview.bounds.minX || preview.bounds.maxY > preview.bounds.minY) {
    parts.push(
      `<rect class="bounds" x="${tx(preview.bounds.minX)}" y="${ty(preview.bounds.maxY)}" width="${(preview.bounds.maxX - preview.bounds.minX) * scale}" height="${(preview.bounds.maxY - preview.bounds.minY) * scale}" />`,
    );
  }
  const first = preview.segments.find((segment) => ["draw", "travel", "out"].includes(segment.type));
  const last = [...preview.segments].reverse().find((segment) => ["draw", "travel", "out"].includes(segment.type));
  if (first) parts.push(`<circle class="start" cx="${tx(first.x1)}" cy="${ty(first.y1)}" r="4" />`);
  if (last) parts.push(`<circle class="end" cx="${tx(last.x2)}" cy="${ty(last.y2)}" r="4" />`);
  parts.push("</svg>");
  $("preview").innerHTML = parts.join("");
}

function updatePreview(text, fileName = "inline") {
  app.gcodeText = text;
  app.preview = parseGcode(text);
  renderPreview(app.preview);
  $("fileName").textContent = fileName;
  $("lineCount").textContent = String(app.preview.lines);
  $("segmentCount").textContent = String(app.preview.segments.length);
  const b = app.preview.bounds;
  $("boundsText").textContent = `${b.minX.toFixed(1)},${b.minY.toFixed(1)} - ${b.maxX.toFixed(1)},${b.maxY.toFixed(1)}`;
  $("warningCount").textContent = String(app.preview.warnings.length);
  $("warnings").textContent = app.preview.warnings.length ? app.preview.warnings.join("\n") : "No preview warnings.";
  $("warnings").classList.toggle("has-error", app.preview.warnings.length > 0);
  updateStateUI();
}

async function sendJob() {
  await api("/api/job", {
    method: "POST",
    body: JSON.stringify({ gcode: app.gcodeText }),
  });
}

function bindUI() {
  document.querySelectorAll("[data-page]").forEach((el) => {
    el.addEventListener("click", () => setPage(el.dataset.page));
  });
  $("refreshPortsBtn").addEventListener("click", () => refreshPorts().catch(showError));
  $("connectBtn").addEventListener("click", async () => {
    const button = $("connectBtn");
    const port = $("manualPort").value.trim() || $("portSelect").value;
    const baud = Number($("baudInput").value || 115200);
    if (!port) {
      showError(new Error("Enter a manual port or select a serial port first"));
      return;
    }
    button.disabled = true;
    button.textContent = "Connecting...";
    appendLog({ time: Date.now(), kind: "host", message: `Connecting to ${port} @ ${baud}` });
    try {
      await api("/api/connect", {
        method: "POST",
        body: JSON.stringify({ port, baud }),
        timeoutMs: 3000,
      });
      await refreshState().catch((error) => {
        appendLog({ time: Date.now(), kind: "error", message: `State refresh failed: ${error.message || error}` });
      });
      appendLog({ time: Date.now(), kind: "host", message: `Serial target set to ${port} @ ${baud}` });
    } catch (error) {
      showError(new Error(`Connect failed: ${error.message || error}`));
    } finally {
      button.disabled = false;
      button.textContent = "Connect";
    }
  });
  $("disconnectBtn").addEventListener("click", async () => {
    await api("/api/connect", { method: "POST", body: JSON.stringify({ port: "", baud: 115200 }) });
    await refreshState();
  });
  $("homeBtn").addEventListener("click", () => sendCommand("HOME").catch(showError));
  $("clearAlarmBtn").addEventListener("click", () => sendCommand("ALARM_CLEAR").catch(showError));
  $("penUpBtn").addEventListener("click", () => sendCommand("PENUP").catch(showError));
  $("penDownBtn").addEventListener("click", () => sendCommand("PENDOWN").catch(showError));
  $("jogUp").addEventListener("click", () => jog(0, 1));
  $("jogDown").addEventListener("click", () => jog(0, -1));
  $("jogLeft").addEventListener("click", () => jog(-1, 0));
  $("jogRight").addEventListener("click", () => jog(1, 0));
  $("stepControl").addEventListener("click", (event) => {
    const button = event.target.closest("button[data-step]");
    if (!button) return;
    app.jogStep = Number(button.dataset.step);
    $("jogStepLabel").textContent = `${app.jogStep}mm`;
    document.querySelectorAll("#stepControl button").forEach((el) => el.classList.toggle("active", el === button));
  });
  $("logFilter").addEventListener("click", (event) => {
    const button = event.target.closest("button[data-filter]");
    if (!button) return;
    app.filter = button.dataset.filter;
    document.querySelectorAll("#logFilter button").forEach((el) => el.classList.toggle("active", el === button));
    renderLogs();
  });
  $("manualCommandForm").addEventListener("submit", (event) => {
    event.preventDefault();
    const command = $("manualCommand").value.trim();
    if (!command) return;
    $("manualCommand").value = "";
    sendCommand(command).catch(showError);
  });
  $("gcodeFile").addEventListener("change", async (event) => {
    const file = event.target.files[0];
    if (!file) return;
    updatePreview(await file.text(), file.name);
  });
  $("sendJobBtn").addEventListener("click", () => sendJob().catch(showError));
  $("abortJobBtn").addEventListener("click", () => api("/api/job/abort", { method: "POST", body: "{}" }).catch(showError));
  $("topAbortBtn").addEventListener("click", () => api("/api/job/abort", { method: "POST", body: "{}" }).catch(showError));
}

async function init() {
  bindUI();
  renderPreview({ segments: [], warnings: [], bounds: { minX: 0, minY: 0, maxX: 0, maxY: 0 } });
  await refreshPorts();
  await refreshState();
  startEvents();
  setInterval(() => refreshState().catch(() => {}), 2000);
}

init().catch(showError);
