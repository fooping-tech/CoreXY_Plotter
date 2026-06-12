#!/usr/bin/env python3
"""Host WebUI bridge for the CoreXY plotter.

This server intentionally delegates serial command delivery to
tools/serial_tool/serial_send.py so ACK handling, queue retry, and job lifecycle
behavior stay in one place.
"""

from __future__ import annotations

import argparse
import csv
import json
import os
import queue
import re
import subprocess
import sys
import tempfile
import threading
import time
from http import HTTPStatus
from http.server import ThreadingHTTPServer, SimpleHTTPRequestHandler
from pathlib import Path
from urllib.parse import parse_qs, urlparse


REPO_ROOT = Path(__file__).resolve().parents[2]
WEB_ROOT = Path(__file__).resolve().parent / "static"
SERIAL_SEND = REPO_ROOT / "tools" / "serial_tool" / "serial_send.py"
QR_TOOL = REPO_ROOT / "tools" / "qr_tool" / "qr_to_plot_csv.py"
TEXT_TOOL = REPO_ROOT / "tools" / "text_tool" / "kst32b_to_gcode.py"
TEXT_FONT = REPO_ROOT / "tools" / "text_tool" / "fonts" / "KST32B.TXT"
DEFAULT_BAUD = 115200
MAX_GCODE_BYTES = 2 * 1024 * 1024
MAX_QR_TEXT_CHARS = 512
MAX_TEXT_GCODE_CHARS = 512
DEFAULT_SEND_SETTINGS: dict[str, object] = {
    "commandTimeoutS": 5.0,
    "jobTimeoutS": 30.0,
    "motionTimeoutMarginS": 5.0,
    "autoMotionTimeout": True,
    "streamGcodeMotion": True,
    "jobLifecycle": True,
    "queueRetryDelayMs": 250,
    "queueRetryTimeoutS": 10.0,
}


log_queue: "queue.Queue[dict[str, object]]" = queue.Queue()
state_lock = threading.Lock()
state: dict[str, object] = {
    "connected": False,
    "port": "",
    "baud": DEFAULT_BAUD,
    "sendSettings": dict(DEFAULT_SEND_SETTINGS),
    "jobRunning": False,
    "lastExitCode": None,
    "machine": {
        "state": "UNKNOWN",
        "x": None,
        "y": None,
        "pen": "UNKNOWN",
        "homed": False,
        "alarmed": False,
        "alarmReason": "none",
        "homing": False,
        "limits": {"x": "UNKNOWN", "y": "UNKNOWN"},
        "tmc": "UNKNOWN",
    },
}
job_process: subprocess.Popen[str] | None = None
job_process_lock = threading.Lock()


POS_RE = re.compile(
    r"X=?(?P<x>-?\d+(?:\.\d+)?)\s+Y=?(?P<y>-?\d+(?:\.\d+)?)|"
    r"pos:\s*X\s*(?P<x2>-?\d+(?:\.\d+)?)\s*Y\s*(?P<y2>-?\d+(?:\.\d+)?)",
    re.IGNORECASE,
)
XY_TARGET_RE = re.compile(
    r"\b(?:ACK_XY|XY batch=).*target=\((?P<x>-?\d+(?:\.\d+)?),(?P<y>-?\d+(?:\.\d+)?)\)",
    re.IGNORECASE,
)
HOMED_RE = re.compile(r"HOMED=(YES|NO)|home:\s*(YES|NO)", re.IGNORECASE)
ALARM_RE = re.compile(r"ALARM=(YES|NO)|safety:\s*(ALARM|READY)", re.IGNORECASE)
ALARM_REASON_RE = re.compile(r'ALARM_REASON="([^"]*)"|ALARM_REASON=([^\s]+)', re.IGNORECASE)
PEN_RE = re.compile(r"PEN=(UP|DOWN)|pen:\s*(UP|DOWN)|PEN\s+(UP|DOWN)", re.IGNORECASE)
LIMIT_RE = re.compile(r"LIMIT_X=(OPEN|ACTIVE|ON|OFF).*LIMIT_Y=(OPEN|ACTIVE|ON|OFF)", re.IGNORECASE)
TMC_RE = re.compile(r"TMC[:=]\s*(READY|NOT READY|OFF)", re.IGNORECASE)
GCODE_WORD_RE = re.compile(r"([A-Z])\s*(-?\d+(?:\.\d+)?)", re.IGNORECASE)


def now_ms() -> int:
    return int(time.time() * 1000)


def emit(kind: str, message: str, **extra: object) -> None:
    event = {"time": now_ms(), "kind": kind, "message": message, **extra}
    log_queue.put(event)
    update_state_from_log(message)


def classify_line(line: str) -> str:
    if "NACK" in line or "REJECT:" in line or "ERROR:" in line or "ALARM=YES" in line:
        return "error"
    if "ACK" in line or " OK" in line or "complete" in line:
        return "ack"
    if line.startswith(">") or "TIMING START" in line:
        return "sent"
    return "firmware"


def machine_state_from_flags(machine: dict[str, object]) -> str:
    if machine.get("alarmed"):
        return "ALARM"
    if machine.get("homing"):
        return "HOMING"
    if not machine.get("homed"):
        return "NEED HOME"
    return "READY"


def update_state_from_log(line: str) -> None:
    with state_lock:
        machine = dict(state["machine"])  # shallow copy
        limits = dict(machine.get("limits", {}))

        pos_match = POS_RE.search(line)
        if pos_match:
            x = pos_match.group("x") or pos_match.group("x2")
            y = pos_match.group("y") or pos_match.group("y2")
            machine["x"] = float(x)
            machine["y"] = float(y)

        xy_target_match = XY_TARGET_RE.search(line)
        if xy_target_match:
            machine["x"] = float(xy_target_match.group("x"))
            machine["y"] = float(xy_target_match.group("y"))

        homed_match = HOMED_RE.search(line)
        if homed_match:
            value = (homed_match.group(1) or homed_match.group(2) or "").upper()
            machine["homed"] = value == "YES"

        alarm_match = ALARM_RE.search(line)
        if alarm_match:
            value = (alarm_match.group(1) or alarm_match.group(2) or "").upper()
            machine["alarmed"] = value in {"YES", "ALARM"}
            if value == "NO":
                machine["alarmReason"] = "none"

        alarm_reason_match = ALARM_REASON_RE.search(line)
        if alarm_reason_match:
            machine["alarmReason"] = alarm_reason_match.group(1) or alarm_reason_match.group(2)

        pen_match = PEN_RE.search(line)
        if pen_match:
            machine["pen"] = next(group for group in pen_match.groups() if group).upper()

        limit_match = LIMIT_RE.search(line)
        if limit_match:
            limits["x"] = limit_match.group(1).upper()
            limits["y"] = limit_match.group(2).upper()
            machine["limits"] = limits

        tmc_match = TMC_RE.search(line)
        if tmc_match:
            machine["tmc"] = tmc_match.group(1).upper()

        if "HOME complete" in line:
            machine["homed"] = True
            machine["homing"] = False
        elif "HOME" in line and ("ACK QUEUED" in line or "AUTO_HOME start" in line):
            machine["homing"] = True
        elif "ALARM_CLEAR complete" in line:
            machine["alarmed"] = False
            machine["alarmReason"] = "none"
        elif "ABORT complete" in line or "JOB_ABORT complete" in line:
            machine["alarmed"] = True
            machine["alarmReason"] = "abort requested"
            machine["homed"] = False
            machine["homing"] = False

        machine["state"] = machine_state_from_flags(machine)
        state["machine"] = machine


def list_ports() -> list[str]:
    ports: set[str] = set()
    for entry in os.scandir("/dev"):
        name = entry.name
        if name.startswith(("cu.", "ttyUSB", "ttyACM")):
            ports.add(str(Path("/dev") / name))
    serial_by_id = Path("/dev/serial/by-id")
    if serial_by_id.is_dir():
        for entry in os.scandir(serial_by_id):
            ports.add(str(serial_by_id / entry.name))
    return sorted(set(ports))


def run_serial_send(args: list[str], *, label: str) -> int:
    cmd = [sys.executable, str(SERIAL_SEND), *args]
    emit("host", f"Starting {label}: {' '.join(cmd)}")
    process = subprocess.Popen(
        cmd,
        cwd=REPO_ROOT,
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        bufsize=1,
    )
    assert process.stdout is not None
    for raw_line in process.stdout:
        line = raw_line.rstrip("\n")
        emit(classify_line(line), line)
    exit_code = process.wait()
    emit("host", f"{label} exited with code {exit_code}", exitCode=exit_code)
    with state_lock:
        state["lastExitCode"] = exit_code
    return exit_code


def write_command_csv(command: str) -> Path:
    temp = tempfile.NamedTemporaryFile("w", newline="", suffix=".csv", delete=False)
    with temp:
        writer = csv.DictWriter(temp, fieldnames=["command", "delay_ms", "expect", "comment"])
        writer.writeheader()
        writer.writerow({"command": command, "delay_ms": "0", "expect": "", "comment": "webui"})
    return Path(temp.name)


def clamp_float(value: object, *, name: str, minimum: float, maximum: float) -> float:
    try:
        parsed = float(value)
    except (TypeError, ValueError) as exc:
        raise ValueError(f"{name} must be a number") from exc
    if not minimum <= parsed <= maximum:
        raise ValueError(f"{name} must be between {minimum:g} and {maximum:g}")
    return parsed


def clamp_int(value: object, *, name: str, minimum: int, maximum: int) -> int:
    try:
        parsed = int(value)
    except (TypeError, ValueError) as exc:
        raise ValueError(f"{name} must be an integer") from exc
    if not minimum <= parsed <= maximum:
        raise ValueError(f"{name} must be between {minimum} and {maximum}")
    return parsed


def bool_setting(value: object, *, name: str) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        normalized = value.strip().lower()
        if normalized in {"1", "true", "yes", "on"}:
            return True
        if normalized in {"0", "false", "no", "off"}:
            return False
    raise ValueError(f"{name} must be true or false")


def normalize_send_settings(raw: dict[str, object]) -> dict[str, object]:
    settings = dict(DEFAULT_SEND_SETTINGS)
    settings.update(raw)
    return {
        "commandTimeoutS": clamp_float(settings["commandTimeoutS"], name="commandTimeoutS", minimum=0.5, maximum=600.0),
        "jobTimeoutS": clamp_float(settings["jobTimeoutS"], name="jobTimeoutS", minimum=1.0, maximum=3600.0),
        "motionTimeoutMarginS": clamp_float(
            settings["motionTimeoutMarginS"],
            name="motionTimeoutMarginS",
            minimum=0.0,
            maximum=600.0,
        ),
        "autoMotionTimeout": bool_setting(settings["autoMotionTimeout"], name="autoMotionTimeout"),
        "streamGcodeMotion": bool_setting(settings["streamGcodeMotion"], name="streamGcodeMotion"),
        "jobLifecycle": bool_setting(settings["jobLifecycle"], name="jobLifecycle"),
        "queueRetryDelayMs": clamp_int(settings["queueRetryDelayMs"], name="queueRetryDelayMs", minimum=0, maximum=60000),
        "queueRetryTimeoutS": clamp_float(
            settings["queueRetryTimeoutS"],
            name="queueRetryTimeoutS",
            minimum=0.5,
            maximum=600.0,
        ),
    }


def command_args(port: str, baud: int, csv_path: Path, settings: dict[str, object]) -> list[str]:
    return [
        "--csv",
        str(csv_path),
        "--port",
        port,
        "--baud",
        str(baud),
        "--startup-delay",
        "0",
        "--startup-drain",
        "0.1",
        "--timeout",
        str(settings["commandTimeoutS"]),
        "--queue-mode",
        "--echo",
    ]


def job_args(port: str, baud: int, gcode_path: Path, settings: dict[str, object]) -> list[str]:
    args = [
        "--gcode",
        str(gcode_path),
        "--port",
        port,
        "--baud",
        str(baud),
        "--queue-mode",
        "--queue-retry-delay-ms",
        str(settings["queueRetryDelayMs"]),
        "--queue-retry-timeout",
        str(settings["queueRetryTimeoutS"]),
        "--timeout",
        str(settings["jobTimeoutS"]),
        "--motion-timeout-margin",
        str(settings["motionTimeoutMarginS"]),
        "--startup-delay",
        "0",
        "--startup-drain",
        "0.2",
        "--echo",
    ]
    if settings["streamGcodeMotion"]:
        args.append("--stream-gcode-motion")
    if settings["jobLifecycle"]:
        args.append("--job-lifecycle")
    if not settings["autoMotionTimeout"]:
        args.append("--no-auto-motion-timeout")
    return args


def read_json(handler: SimpleHTTPRequestHandler) -> dict[str, object]:
    length = int(handler.headers.get("Content-Length", "0"))
    if length <= 0:
        return {}
    return json.loads(handler.rfile.read(length).decode("utf-8"))


def send_json(handler: SimpleHTTPRequestHandler, payload: object, status: HTTPStatus = HTTPStatus.OK) -> None:
    data = json.dumps(payload).encode("utf-8")
    handler.send_response(status)
    handler.send_header("Content-Type", "application/json")
    handler.send_header("Content-Length", str(len(data)))
    handler.end_headers()
    handler.wfile.write(data)


def snapshot_state() -> dict[str, object]:
    reap_finished_job_process()
    with state_lock:
        return {
            "connected": state["connected"],
            "port": state["port"],
            "baud": state["baud"],
            "sendSettings": json.loads(json.dumps(state["sendSettings"])),
            "jobRunning": state["jobRunning"],
            "lastExitCode": state["lastExitCode"],
            "machine": json.loads(json.dumps(state["machine"])),
        }


def save_gcode(text: str) -> Path:
    encoded = text.encode("utf-8")
    if len(encoded) > MAX_GCODE_BYTES:
        raise ValueError("G-code file is too large for the MVP WebUI")
    temp = tempfile.NamedTemporaryFile("w", suffix=".gcode", delete=False, encoding="utf-8")
    with temp:
        temp.write(text)
    return Path(temp.name)


def gcode_words(line: str) -> dict[str, float]:
    return {match.group(1).upper(): float(match.group(2)) for match in GCODE_WORD_RE.finditer(line.split(";")[0])}


def replace_motion_xy(line: str, x_mm: float, y_mm: float) -> str:
    words = gcode_words(line)
    feed = f" F{words['F']:g}" if "F" in words else ""
    command = "G0" if line.strip().upper().startswith(("G0", "G00")) else "G1"
    return f"{command} X{x_mm:.3f} Y{y_mm:.3f}{feed}"


def point_outside_bounds(point: tuple[float, float], bounds: tuple[float, float, float, float]) -> bool:
    min_x, min_y, max_x, max_y = bounds
    margin = max(2.0, max(max_x - min_x, max_y - min_y) * 0.25)
    x_mm, y_mm = point
    return x_mm < min_x - margin or x_mm > max_x + margin or y_mm < min_y - margin or y_mm > max_y + margin


def normalize_generated_gcode_start(gcode: str) -> str:
    """Remove a stray initial draw start without changing normal generated paths.

    The WebUI generators should start with M5, then a pen-up G0 to the first
    drawable point, then M3. If an older generator emits a stale G0 X0 Y0 before
    M3, the first G1 would draw from that isolated point. Detect that case from
    the drawable bounding box and retarget the stale travel to the first body
    point.
    """

    lines = gcode.splitlines()
    x_mm: float | None = None
    y_mm: float | None = None
    absolute = True
    units = 1.0
    pen_down = False
    last_motion_before_first_m3: int | None = None
    first_m3_seen = False
    first_draw_end: tuple[float, float] | None = None
    draw_endpoints: list[tuple[float, float]] = []

    for index, raw_line in enumerate(lines):
        line = raw_line.strip().upper()
        if not line or line.startswith(";") or line == "%":
            continue
        words = gcode_words(line)
        if line.startswith("G20"):
            units = 25.4
            continue
        if line.startswith("G21"):
            units = 1.0
            continue
        if line.startswith("G90"):
            absolute = True
            continue
        if line.startswith("G91"):
            absolute = False
            continue
        if line.startswith("M3"):
            pen_down = True
            first_m3_seen = True
            continue
        if line.startswith("M5"):
            pen_down = False
            continue
        if not line.startswith(("G0", "G00", "G1", "G01")):
            continue

        current_x = x_mm if x_mm is not None else 0.0
        current_y = y_mm if y_mm is not None else 0.0
        next_x = current_x if "X" not in words else (words["X"] * units if absolute else current_x + words["X"] * units)
        next_y = current_y if "Y" not in words else (words["Y"] * units if absolute else current_y + words["Y"] * units)
        if not first_m3_seen:
            last_motion_before_first_m3 = index
        if pen_down and line.startswith(("G1", "G01")):
            if first_draw_end is None:
                first_draw_end = (next_x, next_y)
            draw_endpoints.append((next_x, next_y))
        x_mm = next_x
        y_mm = next_y

    if last_motion_before_first_m3 is None or first_draw_end is None or len(draw_endpoints) < 2:
        return gcode

    words = gcode_words(lines[last_motion_before_first_m3])
    if "X" not in words or "Y" not in words:
        return gcode
    initial_point = (words["X"], words["Y"])
    min_x = min(point[0] for point in draw_endpoints)
    min_y = min(point[1] for point in draw_endpoints)
    max_x = max(point[0] for point in draw_endpoints)
    max_y = max(point[1] for point in draw_endpoints)
    if not point_outside_bounds(initial_point, (min_x, min_y, max_x, max_y)):
        return gcode

    lines[last_motion_before_first_m3] = replace_motion_xy(lines[last_motion_before_first_m3], *first_draw_end)
    return "\n".join(lines) + "\n"


def reap_finished_job_process() -> None:
    global job_process
    with job_process_lock:
        process = job_process
        if process is None:
            return
        exit_code = process.poll()
        if exit_code is None:
            return
        job_process = None
    with state_lock:
        state["jobRunning"] = False
        state["lastExitCode"] = exit_code


def stop_host_job_process() -> None:
    global job_process
    with job_process_lock:
        process = job_process
    if process is None or process.poll() is not None:
        reap_finished_job_process()
        return

    emit("host", "Stopping host job sender before JOB_ABORT")
    process.terminate()
    try:
        exit_code = process.wait(timeout=2.0)
    except subprocess.TimeoutExpired:
        emit("host", "Host job sender did not stop; killing it")
        process.kill()
        exit_code = process.wait(timeout=2.0)

    with job_process_lock:
        if job_process is process:
            job_process = None
    with state_lock:
        state["jobRunning"] = False
        state["lastExitCode"] = exit_code
    emit("host", f"host job sender stopped with code {exit_code}", exitCode=exit_code)


def abort_job_thread(port: str, baud: int, settings: dict[str, object]) -> None:
    stop_host_job_process()
    csv_path = write_command_csv("JOB_ABORT")
    run_serial_send(command_args(port, baud, csv_path, settings), label="JOB_ABORT")


class WebUIHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args: object, **kwargs: object) -> None:
        super().__init__(*args, directory=str(WEB_ROOT), **kwargs)

    def log_message(self, format: str, *args: object) -> None:
        # Keep routine HTTP access logs out of the UI event stream. Logging
        # every /api/state poll can create a feedback loop and slow controls.
        return

    def do_GET(self) -> None:
        parsed = urlparse(self.path)
        if parsed.path == "/api/ports":
            send_json(self, {"ports": list_ports()})
            return
        if parsed.path == "/api/state":
            send_json(self, snapshot_state())
            return
        if parsed.path == "/api/events":
            self.handle_events()
            return
        if parsed.path == "/":
            self.path = "/index.html"
        super().do_GET()

    def do_POST(self) -> None:
        parsed = urlparse(self.path)
        try:
            if parsed.path == "/api/connect":
                self.handle_connect()
            elif parsed.path == "/api/command":
                self.handle_command()
            elif parsed.path == "/api/job":
                self.handle_job()
            elif parsed.path == "/api/job/abort":
                self.handle_abort()
            elif parsed.path == "/api/qr/gcode":
                self.handle_qr_gcode()
            elif parsed.path == "/api/text/gcode":
                self.handle_text_gcode()
            elif parsed.path == "/api/settings":
                self.handle_settings()
            else:
                send_json(self, {"error": "not found"}, HTTPStatus.NOT_FOUND)
        except Exception as exc:  # Keep UI failures visible without crashing the server.
            emit("error", f"Host error: {exc}")
            send_json(self, {"error": str(exc)}, HTTPStatus.BAD_REQUEST)

    def handle_connect(self) -> None:
        body = read_json(self)
        port = str(body.get("port", ""))
        baud = int(body.get("baud", DEFAULT_BAUD))
        if port and port not in list_ports():
            emit("host", f"Using manually entered port: {port}")
        with state_lock:
            state["connected"] = bool(port)
            state["port"] = port
            state["baud"] = baud
        emit("host", f"Serial target set to {port or 'disconnected'} @ {baud}")
        send_json(self, {"ok": True})

    def handle_command(self) -> None:
        body = read_json(self)
        command = str(body.get("command", "")).strip()
        if not command:
            raise ValueError("command is required")
        with state_lock:
            port = str(state.get("port", ""))
            baud = int(state.get("baud", DEFAULT_BAUD))
            settings = dict(state.get("sendSettings", DEFAULT_SEND_SETTINGS))
        if not port:
            raise ValueError("serial port is not configured")
        csv_path = write_command_csv(command)
        threading.Thread(
            target=run_serial_send,
            args=(command_args(port, baud, csv_path, settings),),
            kwargs={"label": f"command {command}"},
            daemon=True,
        ).start()
        send_json(self, {"ok": True})

    def handle_job(self) -> None:
        global job_process
        body = read_json(self)
        text = str(body.get("gcode", ""))
        if not text.strip():
            raise ValueError("gcode is required")
        with state_lock:
            port = str(state.get("port", ""))
            baud = int(state.get("baud", DEFAULT_BAUD))
            settings = dict(state.get("sendSettings", DEFAULT_SEND_SETTINGS))
        if not port:
            raise ValueError("serial port is not configured")
        reap_finished_job_process()
        with job_process_lock:
            if job_process is not None and job_process.poll() is None:
                raise ValueError("job is already running")
        gcode_path = save_gcode(text)
        thread = threading.Thread(
            target=self.run_job_thread,
            args=(job_args(port, baud, gcode_path, settings),),
            daemon=True,
        )
        thread.start()
        send_json(self, {"ok": True})

    def handle_settings(self) -> None:
        body = read_json(self)
        settings = normalize_send_settings(body)
        with state_lock:
            state["sendSettings"] = settings
        emit("host", "Updated serial_send.py WebUI defaults")
        send_json(self, {"ok": True, "sendSettings": settings})

    def handle_qr_gcode(self) -> None:
        body = read_json(self)
        text = str(body.get("text", "")).strip()
        if not text:
            raise ValueError("QR text is required")
        if len(text) > MAX_QR_TEXT_CHARS:
            raise ValueError(f"QR text must be {MAX_QR_TEXT_CHARS} characters or fewer")
        if not QR_TOOL.exists():
            raise ValueError(f"QR tool not found: {QR_TOOL}")

        origin_x = float(body.get("originX", 10))
        origin_y = float(body.get("originY", 10))
        module_mm = float(body.get("moduleMm", 1.0))
        hatch_pitch_mm = float(body.get("hatchPitchMm", 0.35))
        draw_feed = float(body.get("drawFeed", 600))
        travel_feed = float(body.get("travelFeed", 1800))
        dwell_ms = int(body.get("dwellMs", 80))
        error_correction = str(body.get("errorCorrection", "M")).upper()
        version = body.get("version", "")

        temp = tempfile.NamedTemporaryFile("w", suffix=".gcode", delete=False, encoding="utf-8")
        temp_path = Path(temp.name)
        temp.close()

        cmd = [
            sys.executable,
            str(QR_TOOL),
            "--text",
            text,
            "--gcode-output",
            str(temp_path),
            "--origin-x",
            str(origin_x),
            "--origin-y",
            str(origin_y),
            "--module-mm",
            str(module_mm),
            "--hatch-pitch-mm",
            str(hatch_pitch_mm),
            "--draw-feed",
            str(draw_feed),
            "--travel-feed",
            str(travel_feed),
            "--dwell-ms",
            str(dwell_ms),
            "--error-correction",
            error_correction,
        ]
        if str(version).strip():
            cmd.extend(["--version", str(int(version))])

        result = subprocess.run(
            cmd,
            cwd=REPO_ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
        )
        output = result.stdout.strip()
        if result.returncode != 0:
            raise ValueError(output or f"QR tool exited with code {result.returncode}")
        gcode = normalize_generated_gcode_start(temp_path.read_text(encoding="utf-8"))
        temp_path.unlink(missing_ok=True)
        if len(gcode.encode("utf-8")) > MAX_GCODE_BYTES:
            raise ValueError("Generated QR G-code is too large")
        send_json(
            self,
            {
                "ok": True,
                "name": "qr_generated.gcode",
                "gcode": gcode,
                "message": output,
            },
        )

    def handle_text_gcode(self) -> None:
        body = read_json(self)
        text = str(body.get("text", "")).strip()
        if not text:
            raise ValueError("text is required")
        if len(text) > MAX_TEXT_GCODE_CHARS:
            raise ValueError(f"text must be {MAX_TEXT_GCODE_CHARS} characters or fewer")
        if not TEXT_TOOL.exists():
            raise ValueError(f"text tool not found: {TEXT_TOOL}")
        if not TEXT_FONT.exists():
            raise ValueError(f"text font not found: {TEXT_FONT}")

        origin_x = float(body.get("originX", 10))
        origin_y = float(body.get("originY", 10))
        size_mm = float(body.get("sizeMm", 20))
        char_spacing_mm = float(body.get("charSpacingMm", 3))
        line_spacing_mm = float(body.get("lineSpacingMm", 6))
        draw_feed = float(body.get("drawFeed", 3000))
        travel_feed = float(body.get("travelFeed", 8000))
        dwell_ms = int(body.get("dwellMs", 80))
        flip_y = bool_setting(body.get("flipY", False), name="flipY")
        auto_scale = bool_setting(body.get("autoScaleToFit", True), name="autoScaleToFit")
        if size_mm <= 0:
            raise ValueError("sizeMm must be > 0")
        if char_spacing_mm < 0 or line_spacing_mm < 0:
            raise ValueError("text spacing must be >= 0")
        if draw_feed <= 0 or travel_feed <= 0:
            raise ValueError("text feed values must be > 0")
        if dwell_ms < 0:
            raise ValueError("dwellMs must be >= 0")

        temp = tempfile.NamedTemporaryFile("w", suffix=".gcode", delete=False, encoding="utf-8")
        temp_path = Path(temp.name)
        temp.close()

        cmd = [
            sys.executable,
            str(TEXT_TOOL),
            "--font",
            str(TEXT_FONT),
            "--text",
            text,
            "--x",
            str(origin_x),
            "--y",
            str(origin_y),
            "--size",
            str(size_mm),
            "--char-spacing",
            str(char_spacing_mm),
            "--line-spacing",
            str(line_spacing_mm),
            "--feed",
            str(draw_feed),
            "--rapid-feed",
            str(travel_feed),
            "--dwell-ms",
            str(dwell_ms),
            "--max-x",
            "60",
            "--max-y",
            "55",
            "-o",
            str(temp_path),
        ]
        if flip_y:
            cmd.append("--flip-y")
        if auto_scale:
            cmd.append("--auto-scale-to-fit")

        result = subprocess.run(
            cmd,
            cwd=REPO_ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            check=False,
        )
        output = result.stdout.strip()
        if result.returncode != 0:
            temp_path.unlink(missing_ok=True)
            raise ValueError(output or f"text tool exited with code {result.returncode}")
        gcode = normalize_generated_gcode_start(temp_path.read_text(encoding="utf-8"))
        temp_path.unlink(missing_ok=True)
        if len(gcode.encode("utf-8")) > MAX_GCODE_BYTES:
            raise ValueError("Generated text G-code is too large")
        send_json(
            self,
            {
                "ok": True,
                "name": "text_generated.gcode",
                "gcode": gcode,
                "message": output,
            },
        )

    def run_job_thread(self, args: list[str]) -> None:
        global job_process
        with state_lock:
            state["jobRunning"] = True
        cmd = [sys.executable, str(SERIAL_SEND), *args]
        emit("host", f"Starting job: {' '.join(cmd)}")
        process = subprocess.Popen(
            cmd,
            cwd=REPO_ROOT,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,
        )
        with job_process_lock:
            job_process = process
        assert process.stdout is not None
        for raw_line in process.stdout:
            line = raw_line.rstrip("\n")
            emit(classify_line(line), line)
        exit_code = process.wait()
        emit("host", f"job exited with code {exit_code}", exitCode=exit_code)
        with job_process_lock:
            job_process = None
        with state_lock:
            state["jobRunning"] = False
            state["lastExitCode"] = exit_code

    def handle_abort(self) -> None:
        with state_lock:
            port = str(state.get("port", ""))
            baud = int(state.get("baud", DEFAULT_BAUD))
            settings = dict(state.get("sendSettings", DEFAULT_SEND_SETTINGS))
        if not port:
            raise ValueError("serial port is not configured")
        threading.Thread(
            target=abort_job_thread,
            args=(port, baud, settings),
            daemon=True,
        ).start()
        send_json(self, {"ok": True})

    def handle_events(self) -> None:
        self.send_response(HTTPStatus.OK)
        self.send_header("Content-Type", "text/event-stream")
        self.send_header("Cache-Control", "no-cache")
        self.send_header("Connection", "keep-alive")
        self.end_headers()
        emit("host", "Event stream connected")
        while True:
            try:
                event = log_queue.get(timeout=20)
            except queue.Empty:
                event = {"time": now_ms(), "kind": "ping", "message": "ping"}
            try:
                self.wfile.write(f"data: {json.dumps(event)}\n\n".encode("utf-8"))
                self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError):
                break


class ReusableThreadingHTTPServer(ThreadingHTTPServer):
    allow_reuse_address = True
    daemon_threads = True


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Run the CoreXY Host WebUI.")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--port", type=int, default=8787)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    if not SERIAL_SEND.exists():
        print(f"serial_send.py not found: {SERIAL_SEND}", file=sys.stderr)
        return 1
    server = ReusableThreadingHTTPServer((args.host, args.port), WebUIHandler)
    print(f"CoreXY Host WebUI: http://{args.host}:{args.port}", flush=True)
    print("Press Ctrl-C to stop.", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nStopping.")
    finally:
        server.server_close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
