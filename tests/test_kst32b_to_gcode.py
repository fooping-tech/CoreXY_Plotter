from __future__ import annotations

import sys
from pathlib import Path
from argparse import Namespace


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "tools" / "text_tool"))

import kst32b_to_gcode  # noqa: E402


def test_decode_csf1_x_move_starts_new_pen_up_stroke_at_same_y() -> None:
    glyph = kst32b_to_gcode.decode_csf1(bytes.fromhex("22 a3 dc 2f c3 b0 41 20"), 15)

    assert glyph.strokes == [
        [(1.0, 3.0), (1.0, 28.0)],
        [(13.0, 28.0), (13.0, 3.0)],
        [(13.0, 16.0), (1.0, 16.0)],
    ]


def test_decode_csf1_ascii_l_is_not_diagonal() -> None:
    glyph = kst32b_to_gcode.decode_csf1(bytes.fromhex("23 a3 4b 29 dc 44 20"), 15)

    assert glyph.strokes == [
        [(2.0, 3.0), (11.0, 3.0)],
        [(7.0, 3.0), (7.0, 28.0), (4.0, 28.0)],
    ]


def test_decode_csf1_kanji_top_dot_starts_from_center() -> None:
    glyph = kst32b_to_gcode.decode_csf1(
        bytes.fromhex("21 bb 5e 30 de 29 b2 d8 55 d2 b3 47 24 a2 cf 59 c3 78 c2 56 2b a5 cb 53 c5 a6 49 20"),
        30,
    )

    assert glyph.strokes[0] == [(0.0, 27.0), (28.0, 27.0)]
    assert glyph.strokes[1] == [(14.0, 27.0), (14.0, 30.0)]


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
