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
