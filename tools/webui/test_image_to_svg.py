#!/usr/bin/env python3
"""Unit checks for WebUI raster image tracing."""

from __future__ import annotations

import sys
import unittest
import re
from io import BytesIO
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from image_to_svg import Image, RasterTraceOptions, trace_raster_image_to_svg


@unittest.skipIf(Image is None, "Pillow is not installed")
class ImageToSvgTest(unittest.TestCase):
    def png_bytes(self) -> bytes:
        image = Image.new("L", (16, 16), 255)
        for index in range(3, 13):
            image.putpixel((index, index), 0)
            image.putpixel((index, 15 - index), 0)
        buffer = BytesIO()
        image.save(buffer, format="PNG")
        return buffer.getvalue()

    def filled_rect_png_bytes(self) -> bytes:
        image = Image.new("L", (20, 20), 255)
        for y in range(5, 15):
            for x in range(4, 16):
                image.putpixel((x, y), 0)
        buffer = BytesIO()
        image.save(buffer, format="PNG")
        return buffer.getvalue()

    def wide_rect_png_bytes(self) -> bytes:
        image = Image.new("L", (40, 20), 255)
        for y in range(5, 15):
            for x in range(4, 34):
                image.putpixel((x, y), 0)
        buffer = BytesIO()
        image.save(buffer, format="PNG")
        return buffer.getvalue()

    def test_line_art_generates_simple_svg(self) -> None:
        result = trace_raster_image_to_svg(
            self.png_bytes(),
            RasterTraceOptions(trace_mode="line_art", threshold_mode="manual", threshold_value=200, max_segments=200),
        )
        self.assertIn("<svg", result.svg)
        self.assertIn("<polyline", result.svg)
        self.assertGreater(result.stroke_count, 0)
        self.assertGreater(result.segment_count, 0)

    def test_outline_trace_generates_simple_svg(self) -> None:
        result = trace_raster_image_to_svg(
            self.png_bytes(),
            RasterTraceOptions(trace_mode="outline", threshold_mode="manual", threshold_value=200, max_segments=300),
        )
        self.assertIn('fill="none"', result.svg)
        self.assertGreater(result.segment_count, 0)

    def test_outline_trace_follows_filled_shape_boundary(self) -> None:
        result = trace_raster_image_to_svg(
            self.filled_rect_png_bytes(),
            RasterTraceOptions(trace_mode="outline", threshold_mode="manual", threshold_value=200, max_segments=300),
        )
        points = [
            tuple(map(float, pair.split(",")))
            for pair in re.search(r'points="([^"]+)"', result.svg).group(1).split()
        ]
        self.assertGreater(len(points), 8)
        self.assertAlmostEqual(points[0][0], points[-1][0], delta=0.01)
        self.assertAlmostEqual(points[0][1], points[-1][1], delta=0.01)
        self.assertLess(max(abs(points[i][0] - points[i - 1][0]) for i in range(1, len(points))), 25)
        self.assertLess(max(abs(points[i][1] - points[i - 1][1]) for i in range(1, len(points))), 25)

    def test_intermediate_svg_preserves_aspect_ratio(self) -> None:
        result = trace_raster_image_to_svg(
            self.wide_rect_png_bytes(),
            RasterTraceOptions(trace_mode="outline", threshold_mode="manual", threshold_value=200),
        )
        match = re.search(r'viewBox="0 0 ([0-9.]+) ([0-9.]+)"', result.svg)
        self.assertIsNotNone(match)
        width = float(match.group(1))
        height = float(match.group(2))
        self.assertGreater(width / height, 2.0)

    def test_hatch_dark_areas_adds_strokes(self) -> None:
        without_hatch = trace_raster_image_to_svg(
            self.filled_rect_png_bytes(),
            RasterTraceOptions(trace_mode="outline", threshold_mode="manual", threshold_value=200),
        )
        with_hatch = trace_raster_image_to_svg(
            self.filled_rect_png_bytes(),
            RasterTraceOptions(
                trace_mode="outline",
                threshold_mode="manual",
                threshold_value=200,
                hatch_enabled=True,
                hatch_threshold=200,
                hatch_pitch_px=3,
            ),
        )
        self.assertGreater(with_hatch.stroke_count, without_hatch.stroke_count)
        self.assertGreater(with_hatch.segment_count, without_hatch.segment_count)

    def test_max_segments_is_enforced(self) -> None:
        with self.assertRaisesRegex(ValueError, "exceeds max_segments"):
            trace_raster_image_to_svg(
                self.png_bytes(),
                RasterTraceOptions(trace_mode="outline", threshold_mode="manual", threshold_value=200, max_segments=1),
            )


if __name__ == "__main__":
    unittest.main()
