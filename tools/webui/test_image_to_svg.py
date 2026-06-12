#!/usr/bin/env python3
"""Unit checks for WebUI raster image tracing."""

from __future__ import annotations

import sys
import unittest
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

    def test_max_segments_is_enforced(self) -> None:
        with self.assertRaisesRegex(ValueError, "exceeds max_segments"):
            trace_raster_image_to_svg(
                self.png_bytes(),
                RasterTraceOptions(trace_mode="outline", threshold_mode="manual", threshold_value=200, max_segments=1),
            )


if __name__ == "__main__":
    unittest.main()
