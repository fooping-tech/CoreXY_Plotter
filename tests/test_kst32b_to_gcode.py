from __future__ import annotations

import sys
from pathlib import Path
from argparse import Namespace


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "tools" / "text_tool"))

import kst32b_to_gcode  # noqa: E402


def test_text_to_gcode_skips_duplicate_pen_up_move_between_strokes() -> None:
    glyph = kst32b_to_gcode.Glyph(
        strokes=[
            [(0.0, 0.0), (10.0, 0.0)],
            [(10.0, 0.0), (10.0, 10.0)],
        ],
        advance_units=30,
    )

    lines = kst32b_to_gcode.text_to_gcode(
        glyphs={ord("A"): glyph},
        text="A",
        start_x_mm=0.0,
        start_y_mm=0.0,
        size_mm=32.0,
        char_spacing_mm=0.0,
        line_spacing_mm=0.0,
        feed_mm_min=3000.0,
        rapid_feed_mm_min=8000.0,
        dwell_ms=0,
        flip_y=False,
        missing_glyph="box",
    )

    assert lines.count("G0 X0.000 Y0.000 F8000") == 1
    assert "G0 X10.000 Y0.000 F8000" not in lines
    assert lines.count("M3") == 2


def test_generate_fitting_lines_auto_scales_to_max_x() -> None:
    glyph = kst32b_to_gcode.Glyph(
        strokes=[[(0.0, 0.0), (30.0, 0.0)]],
        advance_units=30,
    )
    args = Namespace(
        x=10.0,
        y=10.0,
        size=20.0,
        char_spacing=3.0,
        line_spacing=6.0,
        feed=3000.0,
        rapid_feed=8000.0,
        dwell_ms=0,
        flip_y=False,
        missing_glyph="box",
        max_x=55.0,
        max_y=None,
        auto_scale_to_fit=True,
    )

    lines, fitted_size = kst32b_to_gcode.generate_fitting_lines(
        {ord("A"): glyph},
        "AAA",
        args,
    )
    bounds = kst32b_to_gcode.gcode_bounds(lines)

    assert fitted_size < 20.0
    assert bounds is not None
    assert bounds[1] <= 55.0


def test_generate_fitting_lines_rejects_out_of_bounds_without_auto_scale() -> None:
    glyph = kst32b_to_gcode.Glyph(
        strokes=[[(0.0, 0.0), (30.0, 0.0)]],
        advance_units=30,
    )
    args = Namespace(
        x=10.0,
        y=10.0,
        size=20.0,
        char_spacing=3.0,
        line_spacing=6.0,
        feed=3000.0,
        rapid_feed=8000.0,
        dwell_ms=0,
        flip_y=False,
        missing_glyph="box",
        max_x=55.0,
        max_y=None,
        auto_scale_to_fit=False,
    )

    try:
        kst32b_to_gcode.generate_fitting_lines({ord("A"): glyph}, "AAA", args)
    except ValueError as exc:
        assert "exceeds configured bounds" in str(exc)
    else:
        raise AssertionError("expected bounds rejection")
