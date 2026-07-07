"""server.pyから分割したWebUIモジュール(gcode_processing/webui_settings)のテスト。"""

from __future__ import annotations

from pathlib import Path

import pytest

from gcode_processing import (
    convert_image_to_gcode,
    input_extension,
    normalize_generated_gcode_start,
    point_outside_bounds,
    raster_options_from_mapping,
    replace_motion_xy,
    svg_options_from_mapping,
    svg_to_gcode,
)
from webui_settings import (
    DEFAULT_SEND_SETTINGS,
    bool_setting,
    clamp_float,
    clamp_int,
    command_args,
    job_args,
    normalize_send_settings,
)


def test_clamp_helpers_validate_ranges() -> None:
    assert clamp_float("2.5", name="v", minimum=0.0, maximum=10.0) == 2.5
    assert clamp_int(7, name="v", minimum=0, maximum=10) == 7
    assert bool_setting("yes", name="v") is True
    assert bool_setting("off", name="v") is False
    with pytest.raises(ValueError, match="v must be between"):
        clamp_float(11, name="v", minimum=0.0, maximum=10.0)
    with pytest.raises(ValueError, match="v must be an integer"):
        clamp_int("x", name="v", minimum=0, maximum=10)
    with pytest.raises(ValueError, match="v must be true or false"):
        bool_setting("maybe", name="v")


def test_normalize_send_settings_applies_defaults_and_clamps() -> None:
    settings = normalize_send_settings({})
    assert settings == {
        "commandTimeoutS": 5.0,
        "jobTimeoutS": 30.0,
        "motionTimeoutMarginS": 5.0,
        "autoMotionTimeout": True,
        "streamGcodeMotion": True,
        "jobLifecycle": True,
        "queueRetryDelayMs": 250,
        "queueRetryTimeoutS": 10.0,
    }
    assert settings.keys() == DEFAULT_SEND_SETTINGS.keys()
    with pytest.raises(ValueError):
        normalize_send_settings({"jobTimeoutS": 0})


def test_job_args_reflect_settings_flags() -> None:
    settings = normalize_send_settings({
        "streamGcodeMotion": False,
        "jobLifecycle": True,
        "autoMotionTimeout": False,
    })
    args = job_args("COM3", 115200, Path("job.gcode"), settings)
    assert "--job-lifecycle" in args
    assert "--stream-gcode-motion" not in args
    assert "--no-auto-motion-timeout" in args
    assert args[args.index("--port") + 1] == "COM3"


def test_command_args_use_command_timeout() -> None:
    settings = normalize_send_settings({"commandTimeoutS": 7.5})
    args = command_args("COM4", 115200, Path("cmd.csv"), settings)
    assert args[args.index("--timeout") + 1] == "7.5"
    assert "--queue-mode" in args


def test_replace_motion_xy_keeps_command_and_feed() -> None:
    assert replace_motion_xy("G0 X0 Y0 F1200", 3.0, 4.0) == "G0 X3.000 Y4.000 F1200"
    assert replace_motion_xy("G1 X9 Y9", 1.0, 2.0) == "G1 X1.000 Y2.000"


def test_point_outside_bounds_uses_margin() -> None:
    bounds = (10.0, 10.0, 20.0, 20.0)
    assert not point_outside_bounds((10.0, 10.0), bounds)
    assert not point_outside_bounds((8.5, 10.0), bounds)  # margin内
    assert point_outside_bounds((0.0, 0.0), bounds)


def test_normalize_generated_gcode_start_retargets_stale_origin_travel() -> None:
    gcode = (
        "G21\nG90\nM5\n"
        "G0 X0 Y0 F1200\n"
        "M3\n"
        "G1 X30 Y30 F800\n"
        "G1 X31 Y30 F800\n"
        "M5\n"
    )
    normalized = normalize_generated_gcode_start(gcode)
    assert "G0 X30.000 Y30.000 F1200" in normalized
    assert "G0 X0 Y0" not in normalized


def test_normalize_generated_gcode_start_keeps_normal_path() -> None:
    gcode = (
        "G21\nG90\nM5\n"
        "G0 X29 Y29 F1200\n"
        "M3\n"
        "G1 X30 Y30 F800\n"
        "G1 X31 Y30 F800\n"
        "M5\n"
    )
    assert normalize_generated_gcode_start(gcode) == gcode


def test_svg_options_mapping_defaults_follow_common_feeds() -> None:
    options = svg_options_from_mapping({})
    assert options.feed_mm_min == 800.0
    assert options.travel_feed_mm_min == 1200.0
    raster = raster_options_from_mapping({})
    assert raster.max_segments == 12000


def test_svg_to_gcode_payload_shape() -> None:
    payload = svg_to_gcode(
        '<svg viewBox="0 0 10 10"><line x1="1" y1="1" x2="9" y2="9"/></svg>',
        svg_options_from_mapping({}),
    )
    assert payload["gcode"].startswith("G21\nG90\nM5\n")
    assert payload["stroke_count"] == 1
    assert payload["warnings"] == []


def test_convert_image_to_gcode_dispatches_svg() -> None:
    events: list[tuple[str, str]] = []
    payload = convert_image_to_gcode(
        filename="input.svg",
        content=b'<svg viewBox="0 0 10 10"><line x1="1" y1="1" x2="9" y2="9"/></svg>',
        gcode_options=svg_options_from_mapping({}),
        raster_options=raster_options_from_mapping({}),
        progress=lambda step, status, text: events.append((step, status)),
    )
    assert payload["input_type"] == "svg"
    assert payload["filename"].startswith("generated_image_")
    assert ("gcode", "done") in events
    assert input_extension("photo.JPG") == ".jpg"


def test_convert_image_to_gcode_rejects_unknown_extension() -> None:
    with pytest.raises(ValueError, match="SVG, PNG, JPG, JPEG"):
        convert_image_to_gcode(
            filename="input.bmp",
            content=b"",
            gcode_options=svg_options_from_mapping({}),
            raster_options=raster_options_from_mapping({}),
        )
