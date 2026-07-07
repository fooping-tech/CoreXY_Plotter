"""WebUIのG-code変換・後処理。

HTTP層(server.py)から分離した変換パイプライン。
SVG/ラスタ画像→G-code変換、生成G-codeの正規化、変換オプションの検証を担当する。
"""

from __future__ import annotations

import sys
from datetime import datetime
from pathlib import Path
from typing import Callable

if str(Path(__file__).resolve().parent) not in sys.path:
    sys.path.insert(0, str(Path(__file__).resolve().parent))
if str(Path(__file__).resolve().parents[1]) not in sys.path:
    sys.path.insert(0, str(Path(__file__).resolve().parents[1]))

from common.plotter_gcode import gcode_words  # noqa: E402
from svg_to_gcode import SvgToGcodeOptions, convert_svg_to_gcode  # noqa: E402
from image_to_svg import (  # noqa: E402
    RasterTraceOptions,
    trace_mode_from_value,
    trace_raster_image_to_svg,
)
from webui_settings import bool_setting, clamp_float, clamp_int  # noqa: E402

MAX_GCODE_BYTES = 2 * 1024 * 1024
MAX_SVG_BYTES = 2 * 1024 * 1024
MAX_IMAGE_BYTES = 8 * 1024 * 1024

ProgressCallback = Callable[[str, str, str], None]


def svg_options_from_mapping(data: dict[str, object]) -> SvgToGcodeOptions:
    return SvgToGcodeOptions(
        width_mm=clamp_float(data.get("width_mm", 50), name="width_mm", minimum=1.0, maximum=1000.0),
        height_mm=clamp_float(data.get("height_mm", 50), name="height_mm", minimum=1.0, maximum=1000.0),
        margin_mm=clamp_float(data.get("margin_mm", 5), name="margin_mm", minimum=0.0, maximum=500.0),
        feed_mm_min=clamp_float(
            data.get("feed_mm_min", SvgToGcodeOptions().feed_mm_min),
            name="feed_mm_min", minimum=1.0, maximum=50000.0),
        travel_feed_mm_min=clamp_float(
            data.get("travel_feed_mm_min", SvgToGcodeOptions().travel_feed_mm_min),
            name="travel_feed_mm_min",
            minimum=1.0,
            maximum=50000.0,
        ),
        simplify_tolerance_mm=clamp_float(
            data.get("simplify_tolerance_mm", 0.2),
            name="simplify_tolerance_mm",
            minimum=0.0,
            maximum=10.0,
        ),
        min_stroke_length_mm=clamp_float(
            data.get("min_stroke_length_mm", 0.5),
            name="min_stroke_length_mm",
            minimum=0.0,
            maximum=100.0,
        ),
        optimize_stroke_order=bool_setting(
            data.get("optimize_stroke_order", True),
            name="optimize_stroke_order",
        ),
    )


def raster_options_from_mapping(data: dict[str, object]) -> RasterTraceOptions:
    return RasterTraceOptions(
        trace_mode=trace_mode_from_value(data.get("trace_mode", "outline")),
        trace_detail=str(data.get("trace_detail", "high")).strip().lower(),
        threshold_mode=str(data.get("threshold_mode", "auto")).strip().lower(),
        threshold_value=clamp_int(data.get("threshold_value", 128), name="threshold_value", minimum=0, maximum=255),
        invert=bool_setting(data.get("invert", False), name="invert"),
        skeletonize=bool_setting(data.get("skeletonize", True), name="skeletonize"),
        max_segments=clamp_int(data.get("max_segments", 12000), name="max_segments", minimum=1, maximum=200000),
        hatch_enabled=bool_setting(data.get("hatch_enabled", False), name="hatch_enabled"),
        hatch_threshold=clamp_int(data.get("hatch_threshold", 96), name="hatch_threshold", minimum=0, maximum=255),
        hatch_pitch_px=clamp_int(data.get("hatch_pitch_px", 8), name="hatch_pitch_px", minimum=1, maximum=200),
    )


def replace_motion_xy(line: str, x_mm: float, y_mm: float) -> str:
    words = gcode_words(line)
    feed = f" F{words['F']:g}" if "F" in words else ""
    command = "G0" if line.strip().upper().startswith(("G0", "G00")) else "G1"
    return f"{command} X{x_mm:.3f} Y{y_mm:.3f}{feed}"


def point_outside_bounds(point: tuple[float, float], bounds: tuple[float, float, float, float]) -> bool:
    min_x, min_y, max_x, max_y = bounds
    margin = max(2.0, max(max_x - min_x, max_y - min_y) * 0.25)
    x_mm, y_mm = point
    return x_mm < min_x - margin or x_mm > max_x + margin or y_mm < min_y - margin or y_mm > max_y + margin


def normalize_generated_gcode_start(gcode: str) -> str:
    """Remove a stray initial draw start without changing normal generated paths.

    The WebUI generators should start with M5, then a pen-up G0 to the first
    drawable point, then M3. If an older generator emits a stale G0 X0 Y0 before
    M3, the first G1 would draw from that isolated point. Detect that case from
    the drawable bounding box and retarget the stale travel to the first body
    point.
    """

    lines = gcode.splitlines()
    x_mm: float | None = None
    y_mm: float | None = None
    absolute = True
    units = 1.0
    pen_down = False
    last_motion_before_first_m3: int | None = None
    first_m3_seen = False
    first_draw_end: tuple[float, float] | None = None
    draw_endpoints: list[tuple[float, float]] = []

    for index, raw_line in enumerate(lines):
        line = raw_line.strip().upper()
        if not line or line.startswith(";") or line == "%":
            continue
        words = gcode_words(line)
        if line.startswith("G20"):
            units = 25.4
            continue
        if line.startswith("G21"):
            units = 1.0
            continue
        if line.startswith("G90"):
            absolute = True
            continue
        if line.startswith("G91"):
            absolute = False
            continue
        if line.startswith("M3"):
            pen_down = True
            first_m3_seen = True
            continue
        if line.startswith("M5"):
            pen_down = False
            continue
        if not line.startswith(("G0", "G00", "G1", "G01")):
            continue

        current_x = x_mm if x_mm is not None else 0.0
        current_y = y_mm if y_mm is not None else 0.0
        next_x = current_x if "X" not in words else (words["X"] * units if absolute else current_x + words["X"] * units)
        next_y = current_y if "Y" not in words else (words["Y"] * units if absolute else current_y + words["Y"] * units)
        if not first_m3_seen:
            last_motion_before_first_m3 = index
        if pen_down and line.startswith(("G1", "G01")):
            if first_draw_end is None:
                first_draw_end = (next_x, next_y)
            draw_endpoints.append((next_x, next_y))
        x_mm = next_x
        y_mm = next_y

    if last_motion_before_first_m3 is None or first_draw_end is None or len(draw_endpoints) < 2:
        return gcode

    words = gcode_words(lines[last_motion_before_first_m3])
    if "X" not in words or "Y" not in words:
        return gcode
    initial_point = (words["X"], words["Y"])
    min_x = min(point[0] for point in draw_endpoints)
    min_y = min(point[1] for point in draw_endpoints)
    max_x = max(point[0] for point in draw_endpoints)
    max_y = max(point[1] for point in draw_endpoints)
    if not point_outside_bounds(initial_point, (min_x, min_y, max_x, max_y)):
        return gcode

    lines[last_motion_before_first_m3] = replace_motion_xy(lines[last_motion_before_first_m3], *first_draw_end)
    return "\n".join(lines) + "\n"


def svg_to_gcode(svg: str, options: SvgToGcodeOptions) -> dict[str, object]:
    result = convert_svg_to_gcode(svg, options)
    gcode = normalize_generated_gcode_start(result.gcode)
    if len(gcode.encode("utf-8")) > MAX_GCODE_BYTES:
        raise ValueError("Generated SVG G-code is too large")
    return {
        "filename": result.filename,
        "gcode": gcode,
        "intermediate_svg": svg,
        "stroke_count": result.stroke_count,
        "segment_count": result.segment_count,
        "warnings": result.warnings,
    }


def input_extension(filename: str) -> str:
    return Path(filename or "").suffix.lower()


def convert_image_to_gcode(
    *,
    filename: str,
    content: bytes,
    gcode_options: SvgToGcodeOptions,
    raster_options: RasterTraceOptions,
    progress: ProgressCallback | None = None,
) -> dict[str, object]:
    extension = input_extension(filename)
    if extension == ".svg":
        if len(content) > MAX_SVG_BYTES:
            raise ValueError("SVG input is too large")
        if progress:
            progress("read", "done", f"Loaded SVG file: {filename}")
            progress("trace", "done", "Skipped raster trace for SVG input")
            progress("gcode", "active", "Parsing SVG strokes and generating G-code")
        svg = content.decode("utf-8")
        payload = svg_to_gcode(svg, gcode_options)
        payload["input_type"] = "svg"
        if progress:
            progress("gcode", "done", f"SVG to G-code complete: {payload['stroke_count']} strokes, {payload['segment_count']} segments")
    elif extension in {".png", ".jpg", ".jpeg"}:
        if len(content) > MAX_IMAGE_BYTES:
            raise ValueError("Image input is too large")
        if progress:
            progress("read", "done", f"Loaded raster file: {filename}")
            progress("trace", "active", "Tracing raster image to plotter-friendly SVG")
        trace_result = trace_raster_image_to_svg(content, raster_options)
        if progress:
            progress("trace", "done", f"Trace complete: {trace_result.stroke_count} strokes, {trace_result.segment_count} segments")
            progress("gcode", "active", "Converting intermediate SVG to G-code")
        payload = svg_to_gcode(trace_result.svg, gcode_options)
        payload["input_type"] = "raster"
        payload["intermediate_svg"] = trace_result.svg
        payload["warnings"] = [*trace_result.warnings, *payload.get("warnings", [])]
        if progress:
            progress("gcode", "done", f"SVG to G-code complete: {payload['stroke_count']} strokes, {payload['segment_count']} segments")
    else:
        raise ValueError("未対応ファイル形式です。SVG, PNG, JPG, JPEGを選択してください")
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    payload["filename"] = f"generated_image_{stamp}.gcode"
    return payload
