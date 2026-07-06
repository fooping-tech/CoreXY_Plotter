from __future__ import annotations

import argparse
from pathlib import Path

import pytest

import serial_send


class FakeSerial:
    def __init__(self, responses: list[str]) -> None:
        self.responses = responses
        self.writes: list[str] = []
        self._buffer = b""

    @property
    def in_waiting(self) -> int:
        return len(self._buffer)

    def write(self, data: bytes) -> None:
        self.writes.append(data.decode("utf-8"))
        response = self.responses.pop(0) if self.responses else ""
        self._buffer = response.encode("utf-8")

    def flush(self) -> None:
        return None

    def read(self, size: int) -> bytes:
        data = self._buffer[:size]
        self._buffer = self._buffer[size:]
        return data


def make_args(
    timeout: float,
    *,
    stream_gcode_motion: bool = False,
    stream_xy_motion: bool = False,
    queue_mode: bool = True,
    motion_timeout_margin: float = 5.0,
    estimate_feed_mm_min: float = 1200.0,
    no_auto_motion_timeout: bool = False,
) -> argparse.Namespace:
    return argparse.Namespace(
        timeout=timeout,
        stream_gcode_motion=stream_gcode_motion,
        stream_xy_motion=stream_xy_motion,
        queue_mode=queue_mode,
        motion_timeout_margin=motion_timeout_margin,
        estimate_feed_mm_min=estimate_feed_mm_min,
        no_auto_motion_timeout=no_auto_motion_timeout,
    )


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


def test_queue_completion_timeout_uses_estimated_motion_plus_margin() -> None:
    row = serial_send.CommandRow(
        line_number=12,
        command="XY 0 100 600",
        delay_ms=250,
        expect="ACK_XY target=",
        comment="long move",
        estimated_motion_s=10.0,
    )

    assert serial_send.queue_completion_timeout_s(
        row,
        make_args(timeout=2.0, motion_timeout_margin=3.0),
    ) == 13.0


def test_annotate_motion_estimates_tracks_csv_xy_absolute_position() -> None:
    rows = [
        serial_send.CommandRow(1, "ZERO", 0, "ZERO", "", source="csv"),
        serial_send.CommandRow(2, "XY 0 100 600", 0, "ACK_XY target=", "", source="csv"),
        serial_send.CommandRow(3, "XY 100 100 1200", 0, "ACK_XY target=", "", source="csv"),
        serial_send.CommandRow(4, "G4 P80", 0, "DWELL", "", source="csv"),
    ]

    annotated = serial_send.annotate_motion_estimates(rows, make_args(timeout=2.0))

    assert [row.estimated_motion_s for row in annotated[:3]] == [0.0, 10.0, 5.0]
    assert annotated[3].estimated_motion_s == pytest.approx(0.08)


def test_annotate_motion_estimates_carries_stream_backlog_to_next_sync_row() -> None:
    rows = [
        serial_send.CommandRow(1, "G90", 0, "distance=ABSOLUTE", "", source="gcode"),
        serial_send.CommandRow(2, "G1 X0 Y60 F600", 0, "ACK_XY target=", "", source="gcode"),
        serial_send.CommandRow(3, "G1 X60 Y60 F600", 0, "ACK_XY target=", "", source="gcode"),
        serial_send.CommandRow(4, "M3", 0, "PEN DOWN", "", source="gcode"),
    ]

    annotated = serial_send.annotate_motion_estimates(
        rows,
        make_args(timeout=2.0, stream_gcode_motion=True),
    )

    assert [row.estimated_motion_s for row in annotated] == [0.0, 6.0, 6.0, 0.0]
    assert [row.preceding_stream_motion_s for row in annotated] == [0.0, 0.0, 0.0, 12.0]


def test_annotate_motion_estimates_tracks_gcode_relative_and_units() -> None:
    rows = [
        serial_send.CommandRow(1, "G20", 0, "units=INCH", "", source="gcode"),
        serial_send.CommandRow(2, "G91", 0, "distance=RELATIVE", "", source="gcode"),
        serial_send.CommandRow(3, "G1 X1 F60", 0, "ACK_XY target=", "", source="gcode"),
    ]

    annotated = serial_send.annotate_motion_estimates(rows, make_args(timeout=2.0))

    assert annotated[2].estimated_motion_s == pytest.approx(25.4)


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


def test_stream_gcode_motion_skips_ack_xy_completion_for_gcode_motion() -> None:
    row = serial_send.CommandRow(
        line_number=7,
        command="G1 X10 Y20 F3000",
        delay_ms=250,
        expect="ACK_XY target=",
        comment="",
        source="gcode",
    )

    assert serial_send.queue_completion_pattern(
        row,
        make_args(timeout=2.0, stream_gcode_motion=True),
    ) == ""


def test_stream_gcode_motion_does_not_change_csv_motion() -> None:
    row = serial_send.CommandRow(
        line_number=7,
        command="G1 X10 Y20 F3000",
        delay_ms=250,
        expect="ACK_XY target=",
        comment="",
        source="csv",
    )

    assert serial_send.queue_completion_pattern(
        row,
        make_args(timeout=2.0, stream_gcode_motion=True),
    ) == "ACK_XY target="


def test_stream_xy_motion_skips_ack_xy_completion_for_csv_xy() -> None:
    row = serial_send.CommandRow(
        line_number=7,
        command="XY 10 20 3000",
        delay_ms=250,
        expect="ACK_XY target=",
        comment="",
        source="csv",
    )

    assert serial_send.queue_completion_pattern(
        row,
        make_args(timeout=2.0, stream_xy_motion=True),
    ) == ""


def test_stream_xy_motion_does_not_change_gcode_motion() -> None:
    row = serial_send.CommandRow(
        line_number=7,
        command="G1 X10 Y20 F3000",
        delay_ms=250,
        expect="ACK_XY target=",
        comment="",
        source="gcode",
    )

    assert serial_send.queue_completion_pattern(
        row,
        make_args(timeout=2.0, stream_xy_motion=True),
    ) == "ACK_XY target="


def test_stream_xy_motion_keeps_non_xy_completion() -> None:
    args = make_args(timeout=2.0, stream_xy_motion=True)
    rows = [
        serial_send.CommandRow(1, "PENDOWN", 250, "PEN DOWN", "", source="csv"),
        serial_send.CommandRow(2, "PENUP", 250, "PEN UP", "", source="csv"),
        serial_send.CommandRow(3, "HOME", 250, "HOME complete", "", source="csv"),
        serial_send.CommandRow(4, "POS", 250, "POS", "", source="csv"),
    ]

    assert [serial_send.queue_completion_pattern(row, args) for row in rows] == [
        "PEN DOWN",
        "PEN UP",
        "HOME complete",
        "POS",
    ]


def test_stream_gcode_motion_keeps_non_motion_completion() -> None:
    args = make_args(timeout=2.0, stream_gcode_motion=True)
    rows = [
        serial_send.CommandRow(1, "M3", 250, "PEN DOWN", "", source="gcode"),
        serial_send.CommandRow(2, "M5", 250, "PEN UP", "", source="gcode"),
        serial_send.CommandRow(3, "G4 P80", 250, "DWELL P=", "", source="gcode"),
        serial_send.CommandRow(4, "G28", 250, "HOME complete", "", source="gcode"),
        serial_send.CommandRow(5, "M114", 250, "POS", "", source="gcode"),
    ]

    assert [serial_send.queue_completion_pattern(row, args) for row in rows] == [
        "PEN DOWN",
        "PEN UP",
        "DWELL P=",
        "HOME complete",
        "POS",
    ]


def test_stream_gcode_motion_requires_gcode_and_queue_mode() -> None:
    with pytest.raises(ValueError, match="requires --gcode"):
        serial_send.validate_args(
            argparse.Namespace(stream_gcode_motion=True, gcode=None, queue_mode=True)
        )

    with pytest.raises(ValueError, match="requires --queue-mode"):
        serial_send.validate_args(
            argparse.Namespace(
                stream_gcode_motion=True,
                gcode=Path("drawing.gcode"),
                queue_mode=False,
            )
        )


def test_stream_xy_motion_requires_queue_mode() -> None:
    with pytest.raises(ValueError, match="requires --queue-mode"):
        serial_send.validate_args(
            argparse.Namespace(stream_xy_motion=True, queue_mode=False)
        )


def test_job_lifecycle_requires_gcode() -> None:
    with pytest.raises(ValueError, match="requires --gcode"):
        serial_send.validate_args(
            argparse.Namespace(job_lifecycle=True, gcode=None)
        )


def test_stream_gcode_motion_retries_command_queue_full() -> None:
    row = serial_send.CommandRow(
        line_number=7,
        command="G1 X10 Y20 F3000",
        delay_ms=0,
        expect="ACK_XY target=",
        comment="",
        source="gcode",
    )
    args = argparse.Namespace(
        timeout=0.1,
        queue_retry_timeout=1.0,
        queue_retry_delay_ms=0,
        echo=False,
        stream_gcode_motion=True,
        stream_xy_motion=False,
    )
    serial_port = FakeSerial(
        [
            "ERROR: CommandQueue full\n",
            "ACK QUEUED G1 X10 Y20 F3000\n",
        ]
    )

    assert serial_send.send_row_queue_mode(serial_port, args, 1, row) == 0
    assert serial_port.writes == [
        "G1 X10 Y20 F3000\n",
        "G1 X10 Y20 F3000\n",
    ]


def test_stream_gcode_motion_returns_on_queue_ack_without_idle_wait(monkeypatch) -> None:
    calls: list[bool] = []

    def fake_read_response_result(*args, **kwargs):
        calls.append(kwargs["return_on_stop"])
        return serial_send.SerialReadResult(
            "ACK QUEUED G1 X10 Y20 F3000\n",
            False,
            kwargs["max_duration_s"],
        )

    monkeypatch.setattr(serial_send, "read_response_result", fake_read_response_result)
    row = serial_send.CommandRow(
        line_number=7,
        command="G1 X10 Y20 F3000",
        delay_ms=0,
        expect="ACK_XY target=",
        comment="",
        source="gcode",
    )
    args = argparse.Namespace(
        timeout=0.1,
        queue_retry_timeout=1.0,
        queue_retry_delay_ms=0,
        echo=False,
        stream_gcode_motion=True,
        stream_xy_motion=False,
    )

    assert serial_send.send_row_queue_mode(FakeSerial([""]), args, 1, row) == 0
    assert calls == [True]


def test_non_stream_motion_keeps_idle_wait_before_queue_ack(monkeypatch) -> None:
    calls: list[bool] = []

    def fake_read_response_result(*args, **kwargs):
        calls.append(kwargs["return_on_stop"])
        return serial_send.SerialReadResult(
            "ACK QUEUED G1 X10 Y20 F3000\nACK_XY target=(10.000,20.000)\n",
            False,
            kwargs["max_duration_s"],
        )

    monkeypatch.setattr(serial_send, "read_response_result", fake_read_response_result)
    row = serial_send.CommandRow(
        line_number=7,
        command="G1 X10 Y20 F3000",
        delay_ms=0,
        expect="ACK_XY target=",
        comment="",
        source="gcode",
    )
    args = argparse.Namespace(
        timeout=0.1,
        queue_retry_timeout=1.0,
        queue_retry_delay_ms=0,
        echo=False,
        stream_gcode_motion=False,
        stream_xy_motion=False,
    )

    assert serial_send.send_row_queue_mode(FakeSerial([""]), args, 1, row) == 0
    assert calls == [False]


def test_stream_xy_motion_returns_on_queue_ack_without_idle_wait(monkeypatch) -> None:
    calls: list[bool] = []

    def fake_read_response_result(*args, **kwargs):
        calls.append(kwargs["return_on_stop"])
        return serial_send.SerialReadResult(
            "ACK QUEUED XY 10 20 3000\n",
            False,
            kwargs["max_duration_s"],
        )

    monkeypatch.setattr(serial_send, "read_response_result", fake_read_response_result)
    row = serial_send.CommandRow(
        line_number=7,
        command="XY 10 20 3000",
        delay_ms=0,
        expect="ACK_XY target=",
        comment="",
        source="csv",
    )
    args = argparse.Namespace(
        timeout=0.1,
        queue_retry_timeout=1.0,
        queue_retry_delay_ms=0,
        echo=False,
        stream_gcode_motion=False,
        stream_xy_motion=True,
    )

    assert serial_send.send_row_queue_mode(FakeSerial([""]), args, 1, row) == 0
    assert calls == [True]


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
    assert [row.source for row in rows] == ["gcode", "gcode", "gcode"]


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


def test_load_input_rows_wraps_gcode_with_job_lifecycle(tmp_path: Path) -> None:
    gcode_path = tmp_path / "drawing.gcode"
    gcode_path.write_text("; drawing\nG21\nG90\n", encoding="utf-8")
    args = argparse.Namespace(
        preamble_csv=None,
        csv=None,
        gcode=gcode_path,
        default_delay_ms=250,
        job_lifecycle=True,
    )

    rows = serial_send.load_input_rows(args)

    assert [row.command for row in rows] == ["JOB_BEGIN", "G21", "G90", "JOB_END"]
    assert rows[0].expect == "JOB_BEGIN OK"
    assert rows[-1].expect == "JOB_END OK"
    assert [row.source for row in rows] == ["job", "gcode", "gcode", "job"]


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
