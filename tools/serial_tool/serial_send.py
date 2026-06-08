#!/usr/bin/env python3
"""Send firmware commands from CSV or G-code files over USB serial."""

from __future__ import annotations

import argparse
import csv
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable


DEFAULT_BAUD = 115200
DEFAULT_TIMEOUT_S = 2.0
DEFAULT_DELAY_MS = 250
DEFAULT_STARTUP_DELAY_S = 4.0
DEFAULT_STARTUP_DRAIN_S = 0.5
DEFAULT_OPEN_RETRIES = 3
DEFAULT_CLOSE_DELAY_S = 0.3
DEFAULT_QUEUE_RETRY_DELAY_MS = 100
DEFAULT_QUEUE_RETRY_TIMEOUT_S = 30.0
READ_DRAIN_S = 0.2
READ_IDLE_S = 0.15
INTERRUPT_ABORT_TIMEOUT_S = 1.0
INTERRUPT_ABORT_MIN_READ_S = 0.2
QUEUE_ACK_PATTERNS = ("ACK QUEUED", "ACK ABORT requested")
QUEUE_FULL_PATTERN = "ERROR: CommandQueue full"
ERROR_PATTERN = "ERROR:"
FIRMWARE_FAILURE_PATTERNS = (
    "NACK",
    "REJECT:",
    "ALARM=YES",
    "machine is alarmed",
)
SYNC_COMPLETION_BY_COMMAND = {
    "HOME": "HOME complete",
    "HOME_X": "HOME_X set zero",
    "HOME_Y": "HOME_Y set zero",
    "G28": "HOME complete",
}
SYNC_COMPLETION_TIMEOUT_S_BY_COMMAND = {
    "HOME": 30.0,
    "HOME_X": 30.0,
    "HOME_Y": 30.0,
    "G28": 30.0,
    "JOB_BEGIN": 60.0,
    "JOB_END": 30.0,
}


@dataclass(frozen=True)
class CommandRow:
    line_number: int
    command: str
    delay_ms: int
    expect: str
    comment: str
    source: str = "csv"


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Send CoreXY plotter firmware commands from CSV or G-code files."
    )
    input_group = parser.add_mutually_exclusive_group(required=True)
    input_group.add_argument(
        "--csv",
        type=Path,
        help="CSV file with command, delay_ms, expect, and comment columns.",
    )
    input_group.add_argument(
        "--gcode",
        type=Path,
        help="G-code file to send one command line at a time.",
    )
    parser.add_argument(
        "--preamble-csv",
        type=Path,
        help=(
            "Optional CSV file to send before the main --csv or --gcode input. "
            "Use this for safety checks, alarm clear, and homing."
        ),
    )
    parser.add_argument(
        "--port",
        help="Serial port path, for example /dev/cu.usbserial-0001.",
    )
    parser.add_argument(
        "--baud",
        type=int,
        default=DEFAULT_BAUD,
        help=f"Serial baud rate. Default: {DEFAULT_BAUD}.",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=DEFAULT_TIMEOUT_S,
        help=f"Maximum response wait in seconds per command. Default: {DEFAULT_TIMEOUT_S}.",
    )
    parser.add_argument(
        "--startup-delay",
        type=float,
        default=DEFAULT_STARTUP_DELAY_S,
        help=(
            "Seconds to wait after opening the serial port before sending commands. "
            f"Default: {DEFAULT_STARTUP_DELAY_S}."
        ),
    )
    parser.add_argument(
        "--startup-drain",
        type=float,
        default=DEFAULT_STARTUP_DRAIN_S,
        help=(
            "Maximum seconds to drain startup serial logs after startup-delay. "
            f"Default: {DEFAULT_STARTUP_DRAIN_S}. This is independent of --timeout."
        ),
    )
    parser.add_argument(
        "--open-retries",
        type=int,
        default=DEFAULT_OPEN_RETRIES,
        help=f"Serial open retry count for transient adapter errors. Default: {DEFAULT_OPEN_RETRIES}.",
    )
    parser.add_argument(
        "--close-delay",
        type=float,
        default=DEFAULT_CLOSE_DELAY_S,
        help=(
            "Seconds to wait after closing the serial port before exiting. "
            f"Default: {DEFAULT_CLOSE_DELAY_S}."
        ),
    )
    parser.add_argument(
        "--default-delay-ms",
        type=int,
        default=DEFAULT_DELAY_MS,
        help=f"Delay after each command when delay_ms is empty. Default: {DEFAULT_DELAY_MS}.",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Print commands without opening the serial port.",
    )
    parser.add_argument(
        "--echo",
        action="store_true",
        help="Print each command before sending it.",
    )
    parser.add_argument(
        "--queue-mode",
        action="store_true",
        help=(
            "Send each row after it is accepted by the firmware command queue. "
            "Retries rows that hit CommandQueue full; HOME commands still wait for completion."
        ),
    )
    parser.add_argument(
        "--stream-gcode-motion",
        action="store_true",
        help=(
            "With --gcode and --queue-mode, advance G0/G1 after ACK QUEUED instead "
            "of waiting for ACK_XY completion. Modal, pen, dwell, homing, and POS "
            "commands still wait for their completion logs."
        ),
    )
    parser.add_argument(
        "--job-lifecycle",
        action="store_true",
        help=(
            "With --gcode, wrap the file with JOB_BEGIN and JOB_END. "
            "If a G-code row fails, send JOB_ABORT before stopping."
        ),
    )
    parser.add_argument(
        "--stream-xy-motion",
        action="store_true",
        help=(
            "With --queue-mode, advance CSV XY commands after ACK QUEUED instead "
            "of waiting for ACK_XY completion. Non-XY commands still wait for "
            "their configured expect/completion logs."
        ),
    )
    parser.add_argument(
        "--queue-retry-delay-ms",
        type=int,
        default=DEFAULT_QUEUE_RETRY_DELAY_MS,
        help=(
            "Delay before retrying a row after CommandQueue full in --queue-mode. "
            f"Default: {DEFAULT_QUEUE_RETRY_DELAY_MS}."
        ),
    )
    parser.add_argument(
        "--queue-retry-timeout",
        type=float,
        default=DEFAULT_QUEUE_RETRY_TIMEOUT_S,
        help=(
            "Maximum seconds to keep retrying one row after CommandQueue full in --queue-mode. "
            f"Default: {DEFAULT_QUEUE_RETRY_TIMEOUT_S}."
        ),
    )
    dtr_group = parser.add_mutually_exclusive_group()
    dtr_group.add_argument(
        "--dtr",
        action="store_true",
        default=None,
        help="Assert DTR after opening the port. Default: leave unchanged.",
    )
    dtr_group.add_argument(
        "--no-dtr",
        dest="dtr",
        action="store_false",
        help="Deassert DTR after opening the port. Default: leave unchanged.",
    )
    rts_group = parser.add_mutually_exclusive_group()
    rts_group.add_argument(
        "--rts",
        action="store_true",
        default=None,
        help="Assert RTS after opening the port. Default: leave unchanged.",
    )
    rts_group.add_argument(
        "--no-rts",
        dest="rts",
        action="store_false",
        help="Deassert RTS after opening the port. Default: leave unchanged.",
    )
    parser.add_argument(
        "--continue-on-error",
        action="store_true",
        help="Continue after an expect mismatch. Default: stop at the first error.",
    )
    return parser.parse_args()


def parse_delay_ms(raw_value: str, default_delay_ms: int, line_number: int) -> int:
    value = raw_value.strip()
    if not value:
        return default_delay_ms
    try:
        delay_ms = int(value)
    except ValueError as exc:
        raise ValueError(f"line {line_number}: delay_ms must be an integer") from exc
    if delay_ms < 0:
        raise ValueError(f"line {line_number}: delay_ms must be >= 0")
    return delay_ms


def load_command_rows(csv_path: Path, default_delay_ms: int) -> list[CommandRow]:
    if default_delay_ms < 0:
        raise ValueError("--default-delay-ms must be >= 0")
    if not csv_path.exists():
        raise FileNotFoundError(f"CSV file not found: {csv_path}")

    rows: list[CommandRow] = []
    with csv_path.open(newline="", encoding="utf-8") as csv_file:
        reader = csv.DictReader(csv_file)
        if reader.fieldnames is None:
            raise ValueError("CSV file must include a header row")
        if "command" not in reader.fieldnames:
            raise ValueError("CSV file must include a command column")

        for line_number, raw_row in enumerate(reader, start=2):
            command = (raw_row.get("command") or "").strip()
            if not command:
                continue
            rows.append(
                CommandRow(
                    line_number=line_number,
                    command=command,
                    delay_ms=parse_delay_ms(
                        raw_row.get("delay_ms") or "",
                        default_delay_ms,
                        line_number,
                    ),
                    expect=(raw_row.get("expect") or "").strip(),
                    comment=(raw_row.get("comment") or "").strip(),
                    source="csv",
                )
            )

    if not rows:
        raise ValueError("CSV file does not contain any commands")
    return rows


def strip_gcode_line(raw_line: str) -> str:
    line = raw_line.strip()
    if line.startswith("\ufeff"):
        line = line.removeprefix("\ufeff").strip()
    if not line or line.startswith(";") or line == "%":
        return ""
    return line


def load_gcode_rows(gcode_path: Path, default_delay_ms: int) -> list[CommandRow]:
    if default_delay_ms < 0:
        raise ValueError("--default-delay-ms must be >= 0")
    if not gcode_path.exists():
        raise FileNotFoundError(f"G-code file not found: {gcode_path}")

    rows: list[CommandRow] = []
    with gcode_path.open(encoding="utf-8") as gcode_file:
        for line_number, raw_line in enumerate(gcode_file, start=1):
            command = strip_gcode_line(raw_line)
            if not command:
                continue
            rows.append(
                CommandRow(
                    line_number=line_number,
                    command=command,
                    delay_ms=default_delay_ms,
                    expect=default_gcode_expect(command),
                    comment="",
                    source="gcode",
                )
            )

    if not rows:
        raise ValueError("G-code file does not contain any commands")
    return rows


def load_input_rows(args: argparse.Namespace) -> list[CommandRow]:
    rows: list[CommandRow] = []
    if args.preamble_csv is not None:
        rows.extend(load_command_rows(args.preamble_csv, args.default_delay_ms))
    if args.csv is not None:
        rows.extend(load_command_rows(args.csv, args.default_delay_ms))
    else:
        if getattr(args, "job_lifecycle", False):
            rows.append(
                CommandRow(
                    line_number=0,
                    command="JOB_BEGIN",
                    delay_ms=args.default_delay_ms,
                    expect="JOB_BEGIN OK",
                    comment="begin formal G-code job",
                    source="job",
                )
            )
        rows.extend(load_gcode_rows(args.gcode, args.default_delay_ms))
        if getattr(args, "job_lifecycle", False):
            rows.append(
                CommandRow(
                    line_number=0,
                    command="JOB_END",
                    delay_ms=args.default_delay_ms,
                    expect="JOB_END OK",
                    comment="end formal G-code job",
                    source="job",
                )
            )
    return rows


def validate_args(args: argparse.Namespace) -> None:
    if getattr(args, "job_lifecycle", False) and getattr(args, "gcode", None) is None:
        raise ValueError("--job-lifecycle requires --gcode")
    if getattr(args, "stream_gcode_motion", False) and getattr(args, "gcode", None) is None:
        raise ValueError("--stream-gcode-motion requires --gcode")
    if getattr(args, "stream_gcode_motion", False) and not getattr(args, "queue_mode", False):
        raise ValueError("--stream-gcode-motion requires --queue-mode")
    if getattr(args, "stream_xy_motion", False) and not getattr(args, "queue_mode", False):
        raise ValueError("--stream-xy-motion requires --queue-mode")


def print_plan(rows: Iterable[CommandRow]) -> None:
    for index, row in enumerate(rows, start=1):
        suffix = f" # {row.comment}" if row.comment else ""
        print(
            f"{index:03d} line={row.line_number} delay_ms={row.delay_ms} "
            f"expect={row.expect!r} command={row.command!r}{suffix}"
        )


def command_name(command: str) -> str:
    return command.split(maxsplit=1)[0].upper() if command.strip() else ""


def default_gcode_expect(command: str) -> str:
    name = command_name(command)
    if name in ("G0", "G1"):
        return "ACK_XY target="
    if name == "G4":
        return "DWELL P="
    if name == "G21":
        return "units=MM"
    if name == "G20":
        return "units=INCH"
    if name == "G90":
        return "distance=ABSOLUTE"
    if name == "G91":
        return "distance=RELATIVE"
    if name == "G28":
        return "HOME complete"
    if name == "M3":
        return "PEN DOWN"
    if name == "M5":
        return "PEN UP"
    if name == "M114":
        return "POS"
    return ""


def is_gcode_motion_command(row: CommandRow) -> bool:
    return row.source == "gcode" and command_name(row.command) in ("G0", "G1")


def stream_gcode_motion_enabled(args: argparse.Namespace, row: CommandRow) -> bool:
    return bool(getattr(args, "stream_gcode_motion", False)) and is_gcode_motion_command(row)


def is_csv_xy_command(row: CommandRow) -> bool:
    return row.source == "csv" and command_name(row.command) == "XY"


def stream_xy_motion_enabled(args: argparse.Namespace, row: CommandRow) -> bool:
    return bool(getattr(args, "stream_xy_motion", False)) and is_csv_xy_command(row)


def stream_motion_enabled(args: argparse.Namespace, row: CommandRow) -> bool:
    return stream_gcode_motion_enabled(args, row) or stream_xy_motion_enabled(args, row)


def firmware_failure_line(response: str) -> str:
    for line in response.splitlines():
        if ERROR_PATTERN in line and QUEUE_FULL_PATTERN not in line:
            return line
        if any(pattern in line for pattern in FIRMWARE_FAILURE_PATTERNS):
            return line
    return ""


def stop_patterns_with_failures(patterns: str | tuple[str, ...]) -> tuple[str, ...]:
    if isinstance(patterns, str):
        base = (patterns,) if patterns else ()
    else:
        base = patterns
    return base + (ERROR_PATTERN,) + FIRMWARE_FAILURE_PATTERNS


def contains_any(text: str, patterns: str | tuple[str, ...]) -> bool:
    if isinstance(patterns, str):
        return bool(patterns) and patterns in text
    return any(pattern and pattern in text for pattern in patterns)


def has_stop_patterns(patterns: str | tuple[str, ...]) -> bool:
    if isinstance(patterns, str):
        return bool(patterns)
    return any(bool(pattern) for pattern in patterns)


def import_serial_module():
    try:
        import serial  # type: ignore[import-not-found]
    except ImportError as exc:
        raise RuntimeError(
            "pyserial is not installed. Activate the venv and run: "
            "python -m pip install -r tools/serial_tool/requirements.txt"
        ) from exc
    return serial


def read_available_text(serial_port, timeout_s: float) -> str:
    deadline = time.monotonic() + timeout_s
    chunks: list[bytes] = []
    while time.monotonic() < deadline:
        waiting = getattr(serial_port, "in_waiting", 0)
        if waiting:
            chunks.append(serial_port.read(waiting))
            deadline = time.monotonic() + READ_DRAIN_S
        else:
            time.sleep(0.02)
    return b"".join(chunks).decode("utf-8", errors="replace")


def read_response_text(
    serial_port,
    min_duration_s: float,
    max_duration_s: float,
    stop_on: str | tuple[str, ...],
    return_on_stop: bool = False,
) -> str:
    started_at = time.monotonic()
    deadline = started_at + max(max_duration_s, min_duration_s)
    last_rx_at: float | None = None
    chunks: list[bytes] = []
    while True:
        now = time.monotonic()
        elapsed_s = now - started_at
        text = b"".join(chunks).decode("utf-8", errors="replace")
        idle_s = None if last_rx_at is None else now - last_rx_at
        if elapsed_s >= min_duration_s:
            if return_on_stop and contains_any(text, stop_on):
                break
            if contains_any(text, stop_on) and idle_s is not None and idle_s >= READ_IDLE_S:
                break
            if not has_stop_patterns(stop_on) and idle_s is not None and idle_s >= READ_IDLE_S:
                break
        if now >= deadline:
            break

        waiting = getattr(serial_port, "in_waiting", 0)
        if waiting:
            chunks.append(serial_port.read(waiting))
            last_rx_at = time.monotonic()
        else:
            time.sleep(0.02)
    return b"".join(chunks).decode("utf-8", errors="replace")


def open_serial_port(serial, port: str, baud: int, timeout: float, retries: int):
    attempts = max(1, retries)
    last_error: Exception | None = None
    for attempt in range(1, attempts + 1):
        try:
            return serial.Serial(port, baud, timeout=timeout)
        except Exception as exc:  # pyserial can surface transient termios errors.
            last_error = exc
            if attempt == attempts:
                break
            print(
                f"WARNING: serial open failed on attempt {attempt}/{attempts}: {exc}",
                file=sys.stderr,
                flush=True,
            )
            time.sleep(0.5)
    detail = f"could not open serial port {port}: {last_error}"
    if last_error is not None and "Invalid argument" in str(last_error):
        detail += (
            "\nmacOS/USB-serial driver refused termios setup before commands were sent. "
            "This is not a CSV/expect error. Confirm no serial monitor is open, then "
            "unplug/replug the CP2104 USB serial adapter or reset the USB connection."
        )
    raise RuntimeError(detail)


def send_abort_on_interrupt(serial_port) -> None:
    if not serial_port.is_open:
        return
    print("Interrupted: sending ABORT before closing serial port.", file=sys.stderr, flush=True)
    try:
        serial_port.write(b"ABORT\n")
        serial_port.flush()
        response = read_response_text(
            serial_port,
            min_duration_s=INTERRUPT_ABORT_MIN_READ_S,
            max_duration_s=INTERRUPT_ABORT_TIMEOUT_S,
            stop_on="ABORT",
        )
    except Exception as exc:
        print(f"WARNING: failed to send ABORT: {exc}", file=sys.stderr, flush=True)
        return
    if response:
        print(response, end="" if response.endswith("\n") else "\n", flush=True)


def send_job_abort_on_failure(serial_port, args: argparse.Namespace) -> None:
    if not getattr(args, "job_lifecycle", False) or not serial_port.is_open:
        return
    print("G-code job failed: sending JOB_ABORT.", file=sys.stderr, flush=True)
    try:
        serial_port.write(b"JOB_ABORT\n")
        serial_port.flush()
        response = read_response_text(
            serial_port,
            min_duration_s=0.2,
            max_duration_s=max(args.timeout, 1.0),
            stop_on=stop_patterns_with_failures(("JOB_ABORT complete", "JOB_ABORT requested")),
        )
    except Exception as exc:
        print(f"WARNING: failed to send JOB_ABORT: {exc}", file=sys.stderr, flush=True)
        return
    print_response(response)


def should_send_job_abort_on_failure(args: argparse.Namespace, row: CommandRow) -> bool:
    if not getattr(args, "job_lifecycle", False):
        return False
    name = command_name(row.command)
    if name in ("JOB_ABORT", "JOB_END"):
        return False
    return row.source in ("gcode", "job")


def print_response(response: str) -> None:
    if response:
        print(response, end="" if response.endswith("\n") else "\n", flush=True)


def elapsed_s(origin_s: float) -> float:
    return time.monotonic() - origin_s


def print_command_timing(
    event: str,
    run_started_at_s: float,
    index: int,
    row: CommandRow,
    command_started_at_s: float | None = None,
    status: str | None = None,
) -> None:
    parts = [
        f"TIMING {event}",
        f"t={elapsed_s(run_started_at_s):.3f}s",
        f"index={index:03d}",
        f"line={row.line_number}",
    ]
    if command_started_at_s is not None:
        parts.append(f"dt={time.monotonic() - command_started_at_s:.3f}s")
    if status is not None:
        parts.append(f"status={status}")
    parts.append(f"command={row.command!r}")
    print(" ".join(parts), flush=True)


def queue_completion_pattern(row: CommandRow, args: argparse.Namespace | None = None) -> str:
    if args is not None and stream_motion_enabled(args, row):
        return ""
    if row.expect:
        return row.expect
    return SYNC_COMPLETION_BY_COMMAND.get(command_name(row.command), "")


def queue_completion_timeout_s(row: CommandRow, args: argparse.Namespace) -> float:
    return max(
        args.timeout,
        row.delay_ms / 1000.0,
        SYNC_COMPLETION_TIMEOUT_S_BY_COMMAND.get(command_name(row.command), 0.0),
    )


def send_row_queue_mode(serial_port, args: argparse.Namespace, index: int, row: CommandRow) -> int:
    retry_deadline = time.monotonic() + args.queue_retry_timeout
    stop_on = QUEUE_ACK_PATTERNS + (QUEUE_FULL_PATTERN,) + stop_patterns_with_failures("")
    attempt = 0
    response = ""
    stream_motion = stream_motion_enabled(args, row)

    while True:
        attempt += 1
        if args.echo and not stream_motion:
            suffix = f" retry={attempt}" if attempt > 1 else ""
            print(f">>> {index:03d}: {row.command}{suffix}", flush=True)
        serial_port.write((row.command + "\n").encode("utf-8"))
        serial_port.flush()
        response = read_response_text(
            serial_port,
            min_duration_s=0.0,
            max_duration_s=args.timeout,
            stop_on=stop_on,
            return_on_stop=stream_motion,
        )
        if not stream_motion or QUEUE_FULL_PATTERN in response or firmware_failure_line(response):
            print_response(response)

        if QUEUE_FULL_PATTERN in response:
            if time.monotonic() >= retry_deadline:
                print(
                    f"ERROR: line {row.line_number}: CommandQueue stayed full "
                    f"for {args.queue_retry_timeout:.1f}s",
                    file=sys.stderr,
                    flush=True,
                )
                return 1
            time.sleep(args.queue_retry_delay_ms / 1000.0)
            continue

        failure = firmware_failure_line(response)
        if failure:
            print(
                f"ERROR: line {row.line_number}: firmware failure: {failure}",
                file=sys.stderr,
                flush=True,
            )
            return 1

        if contains_any(response, QUEUE_ACK_PATTERNS):
            break

        if ERROR_PATTERN in response:
            if stream_motion:
                print_response(response)
            print(
                f"ERROR: line {row.line_number}: firmware rejected command before queue ACK",
                file=sys.stderr,
                flush=True,
            )
            return 1

        print(
            f"ERROR: line {row.line_number}: queue ACK was not found in serial response",
            file=sys.stderr,
            flush=True,
        )
        return 1

    completion = queue_completion_pattern(row, args)
    if completion and completion not in response:
        delay_s = row.delay_ms / 1000.0
        completion_response = read_response_text(
            serial_port,
            min_duration_s=delay_s,
            max_duration_s=queue_completion_timeout_s(row, args),
            stop_on=stop_patterns_with_failures(completion),
        )
        print_response(completion_response)
        response += completion_response

    failure = firmware_failure_line(response)
    if failure:
        print(
            f"ERROR: line {row.line_number}: firmware failure: {failure}",
            file=sys.stderr,
            flush=True,
        )
        return 1

    if completion and completion not in response:
        print(
            f"ERROR: line {row.line_number}: expected {completion!r} "
            "was not found in serial response",
            file=sys.stderr,
            flush=True,
        )
        return 1

    return 0


def send_row_standard_mode(serial_port, args: argparse.Namespace, index: int, row: CommandRow) -> int:
    if args.echo:
        print(f">>> {index:03d}: {row.command}", flush=True)
    serial_port.write((row.command + "\n").encode("utf-8"))
    serial_port.flush()
    delay_s = row.delay_ms / 1000.0
    response = read_response_text(
        serial_port,
        min_duration_s=delay_s,
        max_duration_s=max(args.timeout, delay_s),
        stop_on=stop_patterns_with_failures(row.expect),
    )
    print_response(response)
    failure = firmware_failure_line(response)
    if failure:
        print(
            f"ERROR: line {row.line_number}: firmware failure: {failure}",
            file=sys.stderr,
            flush=True,
        )
        return 1
    if row.expect and row.expect not in response:
        print(
            f"ERROR: line {row.line_number}: expected {row.expect!r} "
            "was not found in serial response",
            file=sys.stderr,
            flush=True,
        )
        return 1
    return 0


def send_rows(args: argparse.Namespace, rows: list[CommandRow]) -> int:
    if not args.port:
        raise ValueError("--port is required unless --dry-run is used")
    if args.timeout < 0:
        raise ValueError("--timeout must be >= 0")
    if args.startup_delay < 0:
        raise ValueError("--startup-delay must be >= 0")
    if args.startup_drain < 0:
        raise ValueError("--startup-drain must be >= 0")
    if args.open_retries < 1:
        raise ValueError("--open-retries must be >= 1")
    if args.close_delay < 0:
        raise ValueError("--close-delay must be >= 0")
    if args.queue_retry_delay_ms < 0:
        raise ValueError("--queue-retry-delay-ms must be >= 0")
    if args.queue_retry_timeout < 0:
        raise ValueError("--queue-retry-timeout must be >= 0")
    validate_args(args)

    serial = import_serial_module()
    failures = 0
    serial_port = open_serial_port(
        serial, args.port, args.baud, args.timeout, args.open_retries
    )
    try:
        if args.dtr is not None:
            serial_port.dtr = args.dtr
        if args.rts is not None:
            serial_port.rts = args.rts
        if args.dtr is not None or args.rts is not None:
            time.sleep(0.1)
        serial_port.reset_input_buffer()
        time.sleep(args.startup_delay)
        startup_text = read_available_text(serial_port, args.startup_drain)
        if startup_text:
            print(startup_text, end="" if startup_text.endswith("\n") else "\n", flush=True)

        run_started_at_s = time.monotonic()
        for index, row in enumerate(rows, start=1):
            command_started_at_s = time.monotonic()
            show_timing = not stream_motion_enabled(args, row)
            if show_timing:
                print_command_timing(
                    "START", run_started_at_s, index, row, command_started_at_s
                )
            if args.queue_mode:
                result = send_row_queue_mode(serial_port, args, index, row)
            else:
                result = send_row_standard_mode(serial_port, args, index, row)
            if show_timing or result:
                print_command_timing(
                    "END",
                    run_started_at_s,
                    index,
                    row,
                    command_started_at_s,
                    "OK" if result == 0 else "ERROR",
                )
            if result:
                failures += 1
                if not args.continue_on_error:
                    if should_send_job_abort_on_failure(args, row):
                        send_job_abort_on_failure(serial_port, args)
                    return 1
        return 1 if failures else 0
    except KeyboardInterrupt:
        send_abort_on_interrupt(serial_port)
        raise
    finally:
        if serial_port.is_open:
            serial_port.flush()
            serial_port.close()
        if args.close_delay > 0:
            time.sleep(args.close_delay)


def main() -> int:
    args = parse_args()
    try:
        validate_args(args)
        rows = load_input_rows(args)
        if args.dry_run:
            print_plan(rows)
            return 0
        return send_rows(args, rows)
    except KeyboardInterrupt:
        print("Interrupted.", file=sys.stderr, flush=True)
        return 130
    except Exception as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())
