from __future__ import annotations

import argparse
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "tools" / "serial_tool"))

import serial_send  # noqa: E402


def make_args(timeout: float) -> argparse.Namespace:
    return argparse.Namespace(timeout=timeout)


def test_home_queue_completion_timeout_has_homing_floor() -> None:
    row = serial_send.CommandRow(
        line_number=10,
        command="HOME",
        delay_ms=0,
        expect="",
        comment="home before drawing",
    )

    assert serial_send.queue_completion_pattern(row) == "HOME complete"
    assert serial_send.queue_completion_timeout_s(row, make_args(timeout=2.0)) == 30.0


def test_non_homing_queue_completion_timeout_uses_timeout_or_delay() -> None:
    row = serial_send.CommandRow(
        line_number=12,
        command="PENUP",
        delay_ms=250,
        expect="PENUP",
        comment="raise pen",
    )

    assert serial_send.queue_completion_timeout_s(row, make_args(timeout=2.0)) == 2.0


def test_g28_queue_completion_uses_homing_floor() -> None:
    row = serial_send.CommandRow(
        line_number=4,
        command="G28",
        delay_ms=250,
        expect="",
        comment="home through gcode",
    )

    assert serial_send.queue_completion_pattern(row) == "HOME complete"
    assert serial_send.queue_completion_timeout_s(row, make_args(timeout=2.0)) == 30.0


def test_load_gcode_rows_skips_comments_and_blank_lines(tmp_path: Path) -> None:
    gcode_path = tmp_path / "sample.gcode"
    gcode_path.write_text(
        "\n"
        "; text: test\n"
        "G21\n"
        "G90\n"
        "%\n"
        "G0 X1 Y2 F3000 ; inline comment is firmware-safe\n",
        encoding="utf-8",
    )

    rows = serial_send.load_gcode_rows(gcode_path, default_delay_ms=123)

    assert [(row.line_number, row.command, row.delay_ms, row.expect) for row in rows] == [
        (3, "G21", 123, "units=MM"),
        (4, "G90", 123, "distance=ABSOLUTE"),
        (6, "G0 X1 Y2 F3000 ; inline comment is firmware-safe", 123, "ACK_XY target="),
    ]


def test_load_input_rows_prepends_preamble_csv_to_gcode(tmp_path: Path) -> None:
    preamble_path = tmp_path / "preamble.csv"
    preamble_path.write_text(
        "command,delay_ms,expect,comment\n"
        "ZERO,500,ZERO,reset stale position\n"
        "G28,500,HOME complete,home before drawing\n",
        encoding="utf-8",
    )
    gcode_path = tmp_path / "drawing.gcode"
    gcode_path.write_text("; drawing\nG21\nG90\n", encoding="utf-8")
    args = argparse.Namespace(
        preamble_csv=preamble_path,
        csv=None,
        gcode=gcode_path,
        default_delay_ms=250,
    )

    rows = serial_send.load_input_rows(args)

    assert [row.command for row in rows] == ["ZERO", "G28", "G21", "G90"]
    assert rows[0].expect == "ZERO"
    assert rows[1].expect == "HOME complete"
    assert rows[2].expect == "units=MM"


def test_default_gcode_expect_for_drawing_commands() -> None:
    assert serial_send.default_gcode_expect("G1 X10 Y20 F3000") == "ACK_XY target="
    assert serial_send.default_gcode_expect("G4 P80") == "DWELL P="
    assert serial_send.default_gcode_expect("M3") == "PEN DOWN"
    assert serial_send.default_gcode_expect("M5") == "PEN UP"


def test_firmware_failure_line_detects_reject_and_nack() -> None:
    assert serial_send.firmware_failure_line("REJECT: machine is alarmed reason=hard limit")
    assert serial_send.firmware_failure_line("NACK_XY target=(1,2) reason=rejected")
    assert serial_send.firmware_failure_line("POS ALARM=YES")
    assert not serial_send.firmware_failure_line("ACK_XY target=(1,2)")
