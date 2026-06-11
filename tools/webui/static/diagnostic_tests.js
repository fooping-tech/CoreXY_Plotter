(() => {
  const DIAGNOSTIC_TESTS = [
    {
      id: "pen_up_border",
      name: "1. Pen-up border travel",
      purpose: "ペンアップ不足、紙ズレ、ベルト滑りの切り分け。描線が出るならペンアップ不足です。",
      build: () => [
        "; Diagnostic: pen-up border travel",
        "G21",
        "G90",
        "M5",
        "G4 P200",
        "G0 X5 Y5 F300",
        "G0 X55 Y5 F300",
        "G0 X55 Y50 F300",
        "G0 X5 Y50 F300",
        "G0 X5 Y5 F300",
        "G0 X55 Y5 F300",
        "G0 X55 Y50 F300",
        "G0 X5 Y50 F300",
        "G0 X5 Y5 F300",
        "M5",
      ].join("\n"),
    },
    {
      id: "slow_square_repeat",
      name: "2. Low-speed square repeat",
      purpose: "低速で同じ四角を3回重ねます。閉じない、重ならない場合は紙ズレ、ペン摩擦、steps/mm、機械ガタを疑います。",
      build: () => {
        const lines = [
          "; Diagnostic: low-speed square repeat",
          "G21",
          "G90",
          "M5",
          "G0 X10 Y10 F300",
          "M3",
          "G4 P200",
        ];
        for (let i = 0; i < 3; i += 1) {
          lines.push(
            `; loop ${i + 1}`,
            "G1 X50 Y10 F200",
            "G1 X50 Y45 F200",
            "G1 X10 Y45 F200",
            "G1 X10 Y10 F200",
          );
        }
        lines.push("M5", "G4 P200");
        return lines.join("\n");
      },
    },
    {
      id: "axis_cross",
      name: "3. Axis cross and diagonal check",
      purpose: "X/Y単独移動と斜め移動の比較。軸ごとのスケール差、CoreXY片側の滑り、ベルトテンション差を見ます。",
      build: () => [
        "; Diagnostic: axis cross and diagonal check",
        "G21",
        "G90",
        "M5",
        "G0 X10 Y10 F300",
        "M3",
        "G4 P200",
        "G1 X50 Y10 F250",
        "M5",
        "G0 X10 Y20 F300",
        "M3",
        "G4 P200",
        "G1 X50 Y20 F250",
        "M5",
        "G0 X10 Y30 F300",
        "M3",
        "G4 P200",
        "G1 X50 Y30 F250",
        "M5",
        "G0 X10 Y40 F300",
        "M3",
        "G4 P200",
        "G1 X50 Y40 F250",
        "M5",
        "G0 X10 Y10 F300",
        "M3",
        "G4 P200",
        "G1 X10 Y45 F250",
        "M5",
        "G0 X30 Y10 F300",
        "M3",
        "G4 P200",
        "G1 X30 Y45 F250",
        "M5",
        "G0 X50 Y10 F300",
        "M3",
        "G4 P200",
        "G1 X50 Y45 F250",
        "M5",
        "G0 X10 Y10 F300",
        "M3",
        "G4 P200",
        "G1 X50 Y45 F250",
        "M5",
        "G0 X50 Y10 F300",
        "M3",
        "G4 P200",
        "G1 X10 Y45 F250",
        "M5",
      ].join("\n"),
    },
    {
      id: "discrete_maze_low",
      name: "4. Discrete maze, low speed",
      purpose: "迷路をペンアップ移動とペンダウン描画に分けて低速実行。これでズレるならjunctionより紙/ペン/機械が本命です。",
      build: () => {
        const segments = [
          [8, 8, 52, 8], [52, 8, 52, 48], [52, 48, 8, 48], [8, 48, 8, 8],
          [14, 8, 14, 22], [14, 28, 14, 42], [20, 14, 38, 14], [44, 14, 52, 14],
          [20, 20, 20, 36], [20, 36, 34, 36], [34, 22, 34, 42], [26, 22, 46, 22],
          [44, 22, 44, 36], [26, 30, 38, 30], [38, 30, 38, 48], [8, 42, 28, 42],
          [28, 42, 28, 48], [8, 26, 20, 26], [32, 8, 32, 14], [46, 36, 52, 36],
        ];
        const lines = ["; Diagnostic: discrete maze low speed", "G21", "G90", "M5"];
        segments.forEach(([x1, y1, x2, y2], index) => {
          lines.push(
            `; segment ${index + 1}`,
            `G0 X${x1.toFixed(3)} Y${y1.toFixed(3)} F300`,
            "M3",
            "G4 P150",
            `G1 X${x2.toFixed(3)} Y${y2.toFixed(3)} F200`,
            "M5",
            "G4 P100",
          );
        });
        return lines.join("\n");
      },
    },
    {
      id: "continuous_corner_low",
      name: "5. Continuous corner path, low speed",
      purpose: "同じ連続直角パスを低速で実行。これがOKで高速版だけ崩れるならjunction/look-ahead/加速度が濃厚です。",
      build: () => continuousCornerPath(250),
    },
    {
      id: "continuous_corner_fast",
      name: "6. Continuous corner path, fast",
      purpose: "junction/look-aheadのストレステスト。低速版と比較し、角でズレ始めるなら加速度、junction、ペン摩擦が原因です。",
      build: () => continuousCornerPath(700),
    },
    {
      id: "short_backlash",
      name: "7. Short back-and-forth backlash",
      purpose: "短い往復を繰り返します。線が太る、位置が戻らない、片方向だけズレる場合はバックラッシュ、プーリ固定、ベルト張力を疑います。",
      build: () => {
        const lines = [
          "; Diagnostic: short back-and-forth backlash",
          "G21",
          "G90",
          "M5",
          "G0 X12 Y12 F300",
          "M3",
          "G4 P200",
        ];
        for (let y = 12; y <= 42; y += 5) {
          lines.push(`G0 X12 Y${y.toFixed(3)} F300`, "M3", "G4 P100");
          for (let i = 0; i < 5; i += 1) {
            lines.push("G1 X28 Y" + y.toFixed(3) + " F250", "G1 X12 Y" + y.toFixed(3) + " F250");
          }
          lines.push("M5");
        }
        lines.push("M5");
        return lines.join("\n");
      },
    },
  ];

  function continuousCornerPath(feed) {
    const points = [
      [8, 8], [52, 8], [52, 14], [14, 14], [14, 20], [46, 20], [46, 26],
      [20, 26], [20, 32], [52, 32], [52, 38], [8, 38], [8, 44], [52, 44],
    ];
    const lines = [
      `; Diagnostic: continuous corner path F${feed}`,
      "G21",
      "G90",
      "M5",
      `G0 X${points[0][0]} Y${points[0][1]} F300`,
      "M3",
      "G4 P200",
    ];
    points.slice(1).forEach(([x, y]) => {
      lines.push(`G1 X${x.toFixed(3)} Y${y.toFixed(3)} F${feed}`);
    });
    lines.push("M5", "G4 P200");
    return lines.join("\n");
  }

  function escapeHtml(text) {
    return text.replace(/[&<>"]/g, (ch) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[ch]));
  }

  async function api(path, options = {}) {
    const response = await fetch(path, {
      headers: { "Content-Type": "application/json", ...(options.headers || {}) },
      ...options,
    });
    const payload = await response.json().catch(() => ({}));
    if (!response.ok) {
      throw new Error(payload.error || `${response.status} ${response.statusText}`);
    }
    return payload;
  }

  function selectedTest() {
    const select = document.getElementById("diagnosticTestSelect");
    return DIAGNOSTIC_TESTS.find((test) => test.id === select.value) || DIAGNOSTIC_TESTS[0];
  }

  function updatePreview() {
    const test = selectedTest();
    const purpose = document.getElementById("diagnosticPurpose");
    const preview = document.getElementById("diagnosticGcodePreview");
    purpose.textContent = test.purpose;
    preview.textContent = test.build();
  }

  async function runSelectedDiagnostic() {
    const button = document.getElementById("runDiagnosticBtn");
    const status = document.getElementById("diagnosticStatus");
    const test = selectedTest();
    const originalText = button.textContent;

    button.disabled = true;
    button.textContent = "Starting...";
    status.textContent = "Checking machine state...";

    try {
      const state = await api("/api/state");
      const machine = state.machine || {};
      if (!state.connected || !state.port) throw new Error("Serial port is not connected");
      if (state.jobRunning) throw new Error("Another job is already running");
      if (machine.alarmed) throw new Error(`Alarm is active: ${machine.alarmReason || "unknown"}`);
      if (!machine.homed) throw new Error("Run HOME before diagnostic tests");

      const gcode = test.build();
      status.textContent = `Sending ${test.name}...`;
      await api("/api/job", {
        method: "POST",
        body: JSON.stringify({ gcode }),
      });
      status.textContent = `Started: ${test.name}. Watch the log and plotted result.`;
    } catch (error) {
      status.textContent = `Diagnostic failed: ${error.message || error}`;
    } finally {
      button.disabled = false;
      button.textContent = originalText;
    }
  }

  async function copyGcode() {
    const status = document.getElementById("diagnosticStatus");
    try {
      await navigator.clipboard.writeText(selectedTest().build());
      status.textContent = "Diagnostic G-code copied to clipboard.";
    } catch (error) {
      status.textContent = `Copy failed: ${error.message || error}`;
    }
  }

  function injectPanel() {
    const jobSection = document.getElementById("job");
    if (!jobSection || document.getElementById("diagnosticPanel")) return;

    const panel = document.createElement("article");
    panel.id = "diagnosticPanel";
    panel.className = "panel diagnostic-panel";
    panel.innerHTML = `
      <div class="panel-header">
        <div>
          <div class="label">Diagnostics</div>
          <h2>Cause Isolation Test Programs</h2>
        </div>
        <button id="runDiagnosticBtn" class="warning" type="button">RUN SELECTED TEST</button>
      </div>
      <div class="form-grid">
        <label>Test program
          <select id="diagnosticTestSelect"></select>
        </label>
      </div>
      <div id="diagnosticPurpose" class="muted"></div>
      <div class="button-row">
        <button id="copyDiagnosticGcodeBtn" class="secondary" type="button">COPY G-CODE</button>
      </div>
      <pre id="diagnosticGcodePreview" class="diagnostic-gcode"></pre>
      <div id="diagnosticStatus" class="muted">Run tests one by one. Fix the paper firmly before starting.</div>
    `;

    const warningsPanel = document.getElementById("warnings")?.closest("article");
    if (warningsPanel) {
      warningsPanel.insertAdjacentElement("afterend", panel);
    } else {
      jobSection.appendChild(panel);
    }

    const select = document.getElementById("diagnosticTestSelect");
    DIAGNOSTIC_TESTS.forEach((test) => {
      const option = document.createElement("option");
      option.value = test.id;
      option.textContent = test.name;
      select.appendChild(option);
    });
    select.addEventListener("change", updatePreview);
    document.getElementById("runDiagnosticBtn").addEventListener("click", runSelectedDiagnostic);
    document.getElementById("copyDiagnosticGcodeBtn").addEventListener("click", copyGcode);
    updatePreview();
  }

  function injectStyle() {
    if (document.getElementById("diagnosticStyle")) return;
    const style = document.createElement("style");
    style.id = "diagnosticStyle";
    style.textContent = `
      .diagnostic-panel { margin-top: 1rem; }
      .diagnostic-gcode {
        max-height: 18rem;
        overflow: auto;
        padding: 0.75rem;
        border: 1px solid rgba(255, 255, 255, 0.12);
        border-radius: 0.75rem;
        white-space: pre-wrap;
        font-size: 0.82rem;
      }
    `;
    document.head.appendChild(style);
  }

  function initDiagnostics() {
    injectStyle();
    injectPanel();
  }

  if (document.readyState === "loading") {
    document.addEventListener("DOMContentLoaded", initDiagnostics);
  } else {
    initDiagnostics();
  }
})();
