"""WebUI SVG-to-G-code変換のユニットテスト。"""

from __future__ import annotations

import re
from datetime import datetime

import pytest

from svg_to_gcode import SvgToGcodeOptions, convert_svg_to_gcode


def test_basic_shapes_convert_to_plotter_gcode() -> None:
    svg = """<svg viewBox="0 0 100 100">
      <path d="M10 10 C20 0 30 0 40 10" />
      <polyline points="10,20 20,30 30,20" />
      <line x1="10" y1="40" x2="40" y2="40" />
      <rect x="50" y="10" width="20" height="10" />
      <circle cx="70" cy="50" r="8" />
      <ellipse cx="30" cy="70" rx="10" ry="5" />
    </svg>"""
    result = convert_svg_to_gcode(
        svg,
        SvgToGcodeOptions(width_mm=60, height_mm=55, margin_mm=5, simplify_tolerance_mm=0),
        timestamp=datetime(2026, 6, 13, 12, 34, 56),
    )

    assert result.filename == "generated_svg_20260613_123456.gcode"
    assert result.stroke_count >= 6
    assert result.segment_count > result.stroke_count
    assert result.gcode.startswith("G21\nG90\nM5\n")
    assert re.search(r"\nM5\nG0 X[0-9.]+ Y[0-9.]+ F1200\nM3\nG1 ", result.gcode)
    assert result.gcode.endswith("M5\n")
    assert "G0 X0 Y0" not in result.gcode


def test_translate_scale_transform_and_unsupported_warning() -> None:
    svg = """<svg>
      <g transform="translate(5 10) scale(2)">
        <path d="M0 0 L10 0 A5 5 0 0 1 20 0" fill="red" />
      </g>
    </svg>"""
    result = convert_svg_to_gcode(svg, SvgToGcodeOptions())
    assert result.stroke_count >= 1
    assert any("unsupported path command" in warning for warning in result.warnings)
    assert any("fill" in warning for warning in result.warnings)


def test_empty_svg_raises_clear_error() -> None:
    with pytest.raises(ValueError, match="no drawable strokes"):
        convert_svg_to_gcode("<svg><text>Hello</text></svg>", SvgToGcodeOptions())
