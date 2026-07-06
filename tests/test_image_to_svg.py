"""WebUIラスタ画像トレースのユニットテスト。"""

from __future__ import annotations

import re
from io import BytesIO

import pytest

from image_to_svg import Image, RasterTraceOptions, trace_raster_image_to_svg

pytestmark = pytest.mark.skipif(Image is None, reason="Pillow is not installed")


def png_bytes() -> bytes:
    image = Image.new("L", (16, 16), 255)
    for index in range(3, 13):
        image.putpixel((index, index), 0)
        image.putpixel((index, 15 - index), 0)
    buffer = BytesIO()
    image.save(buffer, format="PNG")
    return buffer.getvalue()


def filled_rect_png_bytes() -> bytes:
    image = Image.new("L", (20, 20), 255)
    for y in range(5, 15):
        for x in range(4, 16):
            image.putpixel((x, y), 0)
    buffer = BytesIO()
    image.save(buffer, format="PNG")
    return buffer.getvalue()


def wide_rect_png_bytes() -> bytes:
    image = Image.new("L", (40, 20), 255)
    for y in range(5, 15):
        for x in range(4, 34):
            image.putpixel((x, y), 0)
    buffer = BytesIO()
    image.save(buffer, format="PNG")
    return buffer.getvalue()


def test_line_art_generates_simple_svg() -> None:
    result = trace_raster_image_to_svg(
        png_bytes(),
        RasterTraceOptions(trace_mode="line_art", threshold_mode="manual", threshold_value=200, max_segments=200),
    )
    assert "<svg" in result.svg
    assert "<polyline" in result.svg
    assert result.stroke_count > 0
    assert result.segment_count > 0


def test_outline_trace_generates_simple_svg() -> None:
    result = trace_raster_image_to_svg(
        png_bytes(),
        RasterTraceOptions(trace_mode="outline", threshold_mode="manual", threshold_value=200, max_segments=300),
    )
    assert 'fill="none"' in result.svg
    assert result.segment_count > 0


def test_outline_trace_follows_filled_shape_boundary() -> None:
    result = trace_raster_image_to_svg(
        filled_rect_png_bytes(),
        RasterTraceOptions(trace_mode="outline", threshold_mode="manual", threshold_value=200, max_segments=300),
    )
    points = [
        tuple(map(float, pair.split(",")))
        for pair in re.search(r'points="([^"]+)"', result.svg).group(1).split()
    ]
    assert len(points) > 8
    assert abs(points[0][0] - points[-1][0]) < 0.01
    assert abs(points[0][1] - points[-1][1]) < 0.01
    assert max(abs(points[i][0] - points[i - 1][0]) for i in range(1, len(points))) < 25
    assert max(abs(points[i][1] - points[i - 1][1]) for i in range(1, len(points))) < 25


def test_intermediate_svg_preserves_aspect_ratio() -> None:
    result = trace_raster_image_to_svg(
        wide_rect_png_bytes(),
        RasterTraceOptions(trace_mode="outline", threshold_mode="manual", threshold_value=200),
    )
    match = re.search(r'viewBox="0 0 ([0-9.]+) ([0-9.]+)"', result.svg)
    assert match is not None
    width = float(match.group(1))
    height = float(match.group(2))
    assert width / height > 2.0


def test_hatch_dark_areas_adds_strokes() -> None:
    without_hatch = trace_raster_image_to_svg(
        filled_rect_png_bytes(),
        RasterTraceOptions(trace_mode="outline", threshold_mode="manual", threshold_value=200),
    )
    with_hatch = trace_raster_image_to_svg(
        filled_rect_png_bytes(),
        RasterTraceOptions(
            trace_mode="outline",
            threshold_mode="manual",
            threshold_value=200,
            hatch_enabled=True,
            hatch_threshold=200,
            hatch_pitch_px=3,
        ),
    )
    assert with_hatch.stroke_count > without_hatch.stroke_count
    assert with_hatch.segment_count > without_hatch.segment_count


def test_max_segments_is_enforced() -> None:
    with pytest.raises(ValueError, match="exceeds max_segments"):
        trace_raster_image_to_svg(
            png_bytes(),
            RasterTraceOptions(trace_mode="outline", threshold_mode="manual", threshold_value=200, max_segments=1),
        )
