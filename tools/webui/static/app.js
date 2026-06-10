const SOFT_LIMIT = { minX: 0, maxX: 60, minY: 0, maxY: 55 };
const API_TIMEOUT_MS = 5000;
const JOG_FEED_MM_MIN = 900;
const PREVIEW_SIZE = { width: 960, height: 640, pad: 36 };
const LAST_PORT_KEY = "corexy.webui.lastPort";
const SAVED_LAYOUT_KEY = "corexy.webui.savedLayout";

const app = {
  page: "dashboard",
  state: null,
  logs: [],
  errors: 0,
  filter: "all",
  jogStep: 1,
  gcodeText: "",
  preview: null,
  layouts: [],
  selectedLayoutId: null,
  nextLayoutId: 1,
  drag: null,
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
    alarmReason: "none",
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

function lastPort() {
  return window.localStorage.getItem(LAST_PORT_KEY) || "";
}

function saveLastPort(port) {
  if (port) window.localStorage.setItem(LAST_PORT_KEY, port);
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
  updateQuickConnectUI();

  $("dashboardState").textContent = stateName;
  $("dashboardStateNote").textContent = stateName === "READY"
    ? "Homed, no alarm"
    : stateName === "ALARM"
      ? `Alarm: ${machine.alarmReason || "unknown"}`
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

  const homeDisabled = !isConnected() || machine.homing || isJobRunning();
  const clearAlarmDisabled = !isConnected() || !machine.alarmed || isJobRunning();
  $("homeBtn").disabled = homeDisabled;
  $("clearAlarmBtn").disabled = clearAlarmDisabled;
  $("dashboardHomeBtn").disabled = homeDisabled;
  $("dashboardClearAlarmBtn").disabled = clearAlarmDisabled;
  $("dashboardSettingsBtn").classList.toggle("hidden", isConnected());
  $("dashboardHomeBtn").classList.toggle("hidden", !isConnected() || machine.homed || machine.alarmed);
  $("dashboardClearAlarmBtn").classList.toggle("hidden", !isConnected() || !machine.alarmed);
  ["jogUp", "jogDown", "jogLeft", "jogRight", "penUpBtn", "penDownBtn"].forEach((id) => {
    $(id).disabled = !canManualMove();
  });
  $("sendJobBtn").disabled = !canSendJob();
  $("abortJobBtn").disabled = !isConnected();
  $("topAbortBtn").disabled = !isConnected();
}

function canSendJob() {
  return jobGateReason() === "";
}

function jobGateReason() {
  const machine = getMachine();
  if (!isConnected()) return "Connect serial port first";
  if (app.layouts.length === 0) return "Add or create G-code first";
  if (isJobRunning()) return "Job is already running";
  if (machine.alarmed) return "Clear alarm before sending";
  if (machine.homing) return "Wait for homing to finish";
  if (!machine.homed) return "Run HOME before sending";
  const warnings = collectLayoutWarnings();
  if (warnings.length > 0) return "Fix preview warnings before sending";
  return "";
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
  const savedPort = lastPort();
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
  ports.sort((a, b) => {
    if (a === savedPort) return -1;
    if (b === savedPort) return 1;
    return a.localeCompare(b);
  });
  if (savedPort && !ports.includes(savedPort)) ports.unshift(savedPort);
  ports.forEach((port) => {
    const option = document.createElement("option");
    option.value = port;
    option.textContent = port === savedPort ? `${port} (last used)` : port;
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

function updateQuickConnectUI() {
  const port = lastPort();
  $("lastPortText").textContent = port || "No saved port";
  $("quickConnectBtn").disabled = !port || isConnected();
  $("quickConnectBtn").textContent = isConnected() ? "Connected" : "Connect";
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
    if (["ack", "error", "firmware"].includes(event.kind)) {
      refreshState().catch(() => {});
    }
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
  sendCommand(`XY ${x.toFixed(3)} ${y.toFixed(3)} ${JOG_FEED_MM_MIN}`).catch(showError);
}

function showError(error) {
  appendLog({ time: Date.now(), kind: "error", message: error.message || String(error) });
}

async function connectToPort(port, baud = 115200, button = null) {
  if (!port) {
    showError(new Error("Enter a manual port or select a serial port first"));
    return;
  }
  const originalText = button ? button.textContent : "";
  if (button) {
    button.disabled = true;
    button.textContent = "Connecting...";
  }
  appendLog({ time: Date.now(), kind: "host", message: `Connecting to ${port} @ ${baud}` });
  try {
    await api("/api/connect", {
      method: "POST",
      body: JSON.stringify({ port, baud }),
      timeoutMs: 3000,
    });
    saveLastPort(port);
    await refreshState().catch((error) => {
      appendLog({ time: Date.now(), kind: "error", message: `State refresh failed: ${error.message || error}` });
    });
    appendLog({ time: Date.now(), kind: "host", message: `Serial target set to ${port} @ ${baud}` });
    updateQuickConnectUI();
  } catch (error) {
    showError(new Error(`Connect failed: ${error.message || error}`));
  } finally {
    if (button) {
      button.disabled = false;
      button.textContent = originalText;
    }
  }
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

function previewGeometry() {
  const { width, height, pad } = PREVIEW_SIZE;
  const workWidth = SOFT_LIMIT.maxX - SOFT_LIMIT.minX;
  const workHeight = SOFT_LIMIT.maxY - SOFT_LIMIT.minY;
  const scale = Math.min((width - pad * 2) / workWidth, (height - pad * 2) / workHeight);
  const ox = (width - workWidth * scale) / 2;
  const oy = (height - workHeight * scale) / 2;
  return {
    width,
    height,
    tx: (x) => ox + x * scale,
    ty: (y) => oy + (SOFT_LIMIT.maxY - y) * scale,
    pxToX: (x) => (x - ox) / scale,
    pxToY: (y) => SOFT_LIMIT.maxY - (y - oy) / scale,
    workWidth,
    workHeight,
    scale,
  };
}

function placedPoint(item, x, y) {
  const b = item.preview.bounds;
  return {
    x: item.x + (x - b.minX) * item.scale,
    y: item.y + (y - b.minY) * item.scale,
  };
}

function placedBounds(item) {
  const b = item.preview.bounds;
  return {
    minX: item.x,
    minY: item.y,
    maxX: item.x + (b.maxX - b.minX) * item.scale,
    maxY: item.y + (b.maxY - b.minY) * item.scale,
  };
}

function isOutOfSoftLimit(bounds) {
  return (
    bounds.minX < SOFT_LIMIT.minX ||
    bounds.maxX > SOFT_LIMIT.maxX ||
    bounds.minY < SOFT_LIMIT.minY ||
    bounds.maxY > SOFT_LIMIT.maxY
  );
}

function collectLayoutWarnings() {
  const warnings = [];
  app.layouts.forEach((item, index) => {
    item.preview.warnings.forEach((warning) => warnings.push(`${item.name}: ${warning}`));
    if (isOutOfSoftLimit(placedBounds(item))) {
      warnings.push(`${index + 1}. ${item.name}: layout leaves soft limit`);
    }
  });
  return warnings;
}

function renderPreview() {
  const geom = previewGeometry();
  const { width, height, tx, ty, workWidth, workHeight } = geom;
  const line = (segment, cls, item) => {
    const p1 = placedPoint(item, segment.x1, segment.y1);
    const p2 = placedPoint(item, segment.x2, segment.y2);
    return `<line class="${cls}" x1="${tx(p1.x)}" y1="${ty(p1.y)}" x2="${tx(p2.x)}" y2="${ty(p2.y)}" />`;
  };

  const parts = [
    `<svg viewBox="0 0 ${width} ${height}" role="img" aria-label="G-code preview">`,
    `<rect class="soft-limit" x="${tx(SOFT_LIMIT.minX)}" y="${ty(SOFT_LIMIT.maxY)}" width="${workWidth * geom.scale}" height="${workHeight * geom.scale}" />`,
  ];
  app.layouts.forEach((item) => {
    const active = item.id === app.selectedLayoutId ? " active" : "";
    const bounds = placedBounds(item);
    parts.push(`<g class="layout-shape${active}" data-id="${item.id}">`);
    item.preview.segments.forEach((segment) => {
      if (segment.type === "draw") parts.push(line(segment, "draw-path", item));
      else if (segment.type === "travel") parts.push(line(segment, "travel-path", item));
      else if (segment.type === "out") parts.push(line(segment, "error-path", item));
    });
    parts.push(
      `<rect class="bounds" x="${tx(bounds.minX)}" y="${ty(bounds.maxY)}" width="${(bounds.maxX - bounds.minX) * geom.scale}" height="${(bounds.maxY - bounds.minY) * geom.scale}" />`,
    );
    const first = item.preview.segments.find((segment) => ["draw", "travel", "out"].includes(segment.type));
    const last = [...item.preview.segments].reverse().find((segment) => ["draw", "travel", "out"].includes(segment.type));
    if (first) {
      const p = placedPoint(item, first.x1, first.y1);
      parts.push(`<circle class="start" cx="${tx(p.x)}" cy="${ty(p.y)}" r="4" />`);
    }
    if (last) {
      const p = placedPoint(item, last.x2, last.y2);
      parts.push(`<circle class="end" cx="${tx(p.x)}" cy="${ty(p.y)}" r="4" />`);
    }
    parts.push("</g>");
  });
  parts.push("</svg>");
  $("preview").innerHTML = parts.join("");
  bindPreviewDrag();
}

function updateJobUI() {
  renderPreview();
  renderLayoutList();
  const totals = app.layouts.reduce(
    (acc, item) => {
      acc.lines += item.preview.lines;
      acc.segments += item.preview.segments.length;
      return acc;
    },
    { lines: 0, segments: 0 },
  );
  const warnings = collectLayoutWarnings();
  const allBounds = app.layouts.map(placedBounds);
  const bounds = allBounds.length
    ? {
        minX: Math.min(...allBounds.map((b) => b.minX)),
        minY: Math.min(...allBounds.map((b) => b.minY)),
        maxX: Math.max(...allBounds.map((b) => b.maxX)),
        maxY: Math.max(...allBounds.map((b) => b.maxY)),
      }
    : null;
  $("fileName").textContent = app.layouts.length ? `${app.layouts.length} file(s)` : "none";
  $("lineCount").textContent = String(totals.lines);
  $("segmentCount").textContent = String(totals.segments);
  $("boundsText").textContent = bounds
    ? `${bounds.minX.toFixed(1)},${bounds.minY.toFixed(1)} - ${bounds.maxX.toFixed(1)},${bounds.maxY.toFixed(1)}`
    : "--";
  $("warningCount").textContent = String(warnings.length);
  $("warnings").textContent = warnings.length ? warnings.join("\n") : "No preview warnings.";
  $("warnings").classList.toggle("has-error", warnings.length > 0);
  const gateReason = jobGateReason();
  $("jobGateReason").textContent = gateReason || "Ready to send.";
  $("jobGateReason").classList.toggle("ready", !gateReason);
  updateLayoutControls();
  updateStateUI();
}

function selectedLayout() {
  return app.layouts.find((item) => item.id === app.selectedLayoutId) || null;
}

function setSelectedLayout(id) {
  app.selectedLayoutId = id;
  updateJobUI();
}

function renderLayoutList() {
  const list = $("layoutItems");
  list.innerHTML = "";
  app.layouts.forEach((item, index) => {
    const row = document.createElement("div");
    row.className = `layout-item${item.id === app.selectedLayoutId ? " active" : ""}`;
    row.dataset.id = String(item.id);

    const name = document.createElement("strong");
    name.textContent = `${index + 1}. ${item.name}`;
    row.appendChild(name);

    const up = document.createElement("button");
    up.type = "button";
    up.textContent = "↑";
    up.disabled = index === 0;
    up.addEventListener("click", (event) => {
      event.stopPropagation();
      moveLayout(index, index - 1);
    });
    row.appendChild(up);

    const down = document.createElement("button");
    down.type = "button";
    down.textContent = "↓";
    down.disabled = index === app.layouts.length - 1;
    down.addEventListener("click", (event) => {
      event.stopPropagation();
      moveLayout(index, index + 1);
    });
    row.appendChild(down);

    row.addEventListener("click", () => setSelectedLayout(item.id));
    list.appendChild(row);
  });
}

function moveLayout(from, to) {
  if (to < 0 || to >= app.layouts.length) return;
  const [item] = app.layouts.splice(from, 1);
  app.layouts.splice(to, 0, item);
  app.selectedLayoutId = item.id;
  updateJobUI();
}

function updateLayoutControls() {
  const item = selectedLayout();
  ["layoutX", "layoutY", "layoutScale"].forEach((id) => {
    $(id).disabled = !item;
  });
  if (!item) {
    $("layoutX").value = "";
    $("layoutY").value = "";
    $("layoutScale").value = "";
    return;
  }
  $("layoutX").value = item.x.toFixed(1);
  $("layoutY").value = item.y.toFixed(1);
  $("layoutScale").value = item.scale.toFixed(2);
}

function updateSelectedLayoutFromInputs() {
  const item = selectedLayout();
  if (!item) return;
  item.x = Number($("layoutX").value || 0);
  item.y = Number($("layoutY").value || 0);
  item.scale = Math.max(0.05, Number($("layoutScale").value || 1));
  updateJobUI();
}

function bindPreviewDrag() {
  const svg = $("preview").querySelector("svg");
  if (!svg) return;
  svg.querySelectorAll(".layout-shape").forEach((shape) => {
    shape.addEventListener("pointerdown", (event) => {
      const item = app.layouts.find((candidate) => candidate.id === Number(shape.dataset.id));
      if (!item) return;
      event.preventDefault();
      app.selectedLayoutId = item.id;
      const point = svgPoint(svg, event);
      app.drag = { id: item.id, startX: point.x, startY: point.y, itemX: item.x, itemY: item.y };
      updateLayoutControls();
      renderLayoutList();
    });
  });
}

function svgPoint(svg, event) {
  const pt = svg.createSVGPoint();
  pt.x = event.clientX;
  pt.y = event.clientY;
  const transformed = pt.matrixTransform(svg.getScreenCTM().inverse());
  const geom = previewGeometry();
  return { x: geom.pxToX(transformed.x), y: geom.pxToY(transformed.y) };
}

function handlePreviewDragMove(event) {
  if (!app.drag) return;
  const svg = $("preview").querySelector("svg");
  const item = app.layouts.find((candidate) => candidate.id === app.drag.id);
  if (!svg || !item) return;
  const point = svgPoint(svg, event);
  item.x = app.drag.itemX + point.x - app.drag.startX;
  item.y = app.drag.itemY + point.y - app.drag.startY;
  updateJobUI();
}

function endPreviewDrag() {
  app.drag = null;
}

async function addGcodeFiles(files) {
  for (const file of files) {
    const text = await file.text();
    const preview = parseGcode(text);
    const item = {
      id: app.nextLayoutId++,
      name: file.name,
      text,
      preview,
      x: 0,
      y: 0,
      scale: 1,
    };
    app.layouts.push(item);
    app.selectedLayoutId = item.id;
  }
  updateJobUI();
}

function addGcodeText(name, text, initialPlacement = {}) {
  const preview = parseGcode(text);
  const item = {
    id: app.nextLayoutId++,
    name,
    text,
    preview,
    x: Number(initialPlacement.x ?? 0),
    y: Number(initialPlacement.y ?? 0),
    scale: 1,
  };
  app.layouts.push(item);
  app.selectedLayoutId = item.id;
  updateJobUI();
}

function qrNumber(id, fallback) {
  const value = Number($(id).value);
  return Number.isFinite(value) ? value : fallback;
}

function qrPayload() {
  return {
    text: $("qrText").value.trim(),
    originX: qrNumber("qrOriginX", 10),
    originY: qrNumber("qrOriginY", 10),
    moduleMm: qrNumber("qrModuleMm", 1.0),
    hatchPitchMm: qrNumber("qrHatchPitchMm", 0.35),
    drawFeed: qrNumber("qrDrawFeed", 600),
    travelFeed: qrNumber("qrTravelFeed", 1800),
    dwellMs: qrNumber("qrDwellMs", 80),
    errorCorrection: $("qrErrorCorrection").value,
  };
}

async function createQrGcode() {
  const button = $("createQrBtn");
  const status = $("qrStatus");
  const payload = qrPayload();
  if (!payload.text) {
    showError(new Error("Enter QR text first"));
    $("qrText").focus();
    return;
  }
  const originalText = button.textContent;
  button.disabled = true;
  button.textContent = "Creating...";
  status.textContent = "Generating QR G-code...";
  try {
    const result = await api("/api/qr/gcode", {
      method: "POST",
      body: JSON.stringify(payload),
      timeoutMs: 15000,
    });
    const safeName = payload.text
      .replace(/[^a-z0-9]+/gi, "_")
      .replace(/^_+|_+$/g, "")
      .slice(0, 32) || "qr";
    addGcodeText(`qr_${safeName}.gcode`, result.gcode, {
      x: payload.originX,
      y: payload.originY,
    });
    status.textContent = result.message || "QR G-code added to layout.";
    appendLog({ time: Date.now(), kind: "host", message: `Added QR G-code for "${payload.text}"` });
  } catch (error) {
    status.textContent = "QR generation failed.";
    showError(error);
  } finally {
    button.disabled = false;
    button.textContent = originalText;
  }
}

function serializeLayout() {
  return {
    version: 1,
    selectedLayoutId: app.selectedLayoutId,
    nextLayoutId: app.nextLayoutId,
    layouts: app.layouts.map((item) => ({
      id: item.id,
      name: item.name,
      text: item.text,
      x: item.x,
      y: item.y,
      scale: item.scale,
    })),
  };
}

function saveLayout() {
  window.localStorage.setItem(SAVED_LAYOUT_KEY, JSON.stringify(serializeLayout()));
  appendLog({ time: Date.now(), kind: "host", message: `Saved layout with ${app.layouts.length} file(s)` });
}

function loadLayout() {
  const raw = window.localStorage.getItem(SAVED_LAYOUT_KEY);
  if (!raw) {
    showError(new Error("No saved layout"));
    return;
  }
  const saved = JSON.parse(raw);
  app.layouts = (saved.layouts || []).map((item) => ({
    id: Number(item.id),
    name: String(item.name || "layout.gcode"),
    text: String(item.text || ""),
    preview: parseGcode(String(item.text || "")),
    x: Number(item.x || 0),
    y: Number(item.y || 0),
    scale: Math.max(0.05, Number(item.scale || 1)),
  }));
  app.nextLayoutId = Math.max(
    Number(saved.nextLayoutId || 1),
    ...app.layouts.map((item) => item.id + 1),
    1,
  );
  app.selectedLayoutId = app.layouts.some((item) => item.id === saved.selectedLayoutId)
    ? saved.selectedLayoutId
    : app.layouts[0]?.id || null;
  updateJobUI();
  appendLog({ time: Date.now(), kind: "host", message: `Loaded layout with ${app.layouts.length} file(s)` });
}

function clearLayout() {
  app.layouts = [];
  app.selectedLayoutId = null;
  app.nextLayoutId = 1;
  updateJobUI();
  appendLog({ time: Date.now(), kind: "host", message: "Cleared layout" });
}

function transformWordsToLine(words, item, motionCode) {
  const point = placedPoint(item, words.x, words.y);
  const feed = words.f === null ? "" : ` F${words.f.toFixed(3)}`;
  return `${motionCode} X${point.x.toFixed(3)} Y${point.y.toFixed(3)}${feed}`;
}

function transformedGcodeForItem(item) {
  let x = 0;
  let y = 0;
  let absolute = true;
  let units = 1;
  const output = [`; ${item.name}`];

  item.text.split(/\r?\n/).forEach((raw) => {
    const trimmed = raw.trim();
    const line = trimmed.toUpperCase();
    if (!line || line.startsWith(";") || line === "%") return;
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
    if (line.startsWith("M3") || line.startsWith("M5") || line.startsWith("G4")) {
      output.push(trimmed);
      return;
    }
    if (line.startsWith("G0") || line.startsWith("G1")) {
      const nextX = words.X === undefined ? x : absolute ? words.X * units : x + words.X * units;
      const nextY = words.Y === undefined ? y : absolute ? words.Y * units : y + words.Y * units;
      const code = line.startsWith("G0") ? "G0" : "G1";
      output.push(transformWordsToLine({ x: nextX, y: nextY, f: words.F ?? null }, item, code));
      x = nextX;
      y = nextY;
    }
  });

  return output.join("\n");
}

function buildCombinedGcode() {
  return [
    "G21",
    "G90",
    ...app.layouts.map(transformedGcodeForItem),
    "M5",
  ].join("\n");
}

async function sendJob() {
  const gateReason = jobGateReason();
  if (gateReason) {
    showError(new Error(`Cannot send job: ${gateReason}`));
    return;
  }
  app.gcodeText = buildCombinedGcode();
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
    await connectToPort(port, baud, button);
  });
  $("quickConnectBtn").addEventListener("click", () => connectToPort(lastPort(), 115200, $("quickConnectBtn")));
  $("disconnectBtn").addEventListener("click", async () => {
    await api("/api/connect", { method: "POST", body: JSON.stringify({ port: "", baud: 115200 }) });
    await refreshState();
    updateQuickConnectUI();
  });
  $("homeBtn").addEventListener("click", () => sendCommand("HOME").catch(showError));
  $("clearAlarmBtn").addEventListener("click", () => sendCommand("ALARM_CLEAR").catch(showError));
  $("dashboardHomeBtn").addEventListener("click", () => sendCommand("HOME").catch(showError));
  $("dashboardClearAlarmBtn").addEventListener("click", () => sendCommand("ALARM_CLEAR").catch(showError));
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
    const files = [...event.target.files];
    if (files.length === 0) return;
    await addGcodeFiles(files);
    event.target.value = "";
  });
  $("createQrBtn").addEventListener("click", () => createQrGcode().catch(showError));
  $("qrText").addEventListener("keydown", (event) => {
    if (event.key !== "Enter") return;
    event.preventDefault();
    createQrGcode().catch(showError);
  });
  $("saveLayoutBtn").addEventListener("click", saveLayout);
  $("loadLayoutBtn").addEventListener("click", loadLayout);
  $("clearLayoutBtn").addEventListener("click", clearLayout);
  ["layoutX", "layoutY", "layoutScale"].forEach((id) => {
    $(id).addEventListener("change", updateSelectedLayoutFromInputs);
  });
  window.addEventListener("pointermove", handlePreviewDragMove);
  window.addEventListener("pointerup", endPreviewDrag);
  window.addEventListener("pointercancel", endPreviewDrag);
  $("sendJobBtn").addEventListener("click", () => sendJob().catch(showError));
  $("abortJobBtn").addEventListener("click", () => api("/api/job/abort", { method: "POST", body: "{}" }).catch(showError));
  $("topAbortBtn").addEventListener("click", () => api("/api/job/abort", { method: "POST", body: "{}" }).catch(showError));
}

async function init() {
  bindUI();
  updateJobUI();
  updateQuickConnectUI();
  await refreshPorts();
  await refreshState();
  startEvents();
  setInterval(() => refreshState().catch(() => {}), 2000);
}

init().catch(showError);
