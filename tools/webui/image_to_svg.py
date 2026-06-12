#!/usr/bin/env python3
"""Trace raster images into simple plotter-friendly SVG polylines.

This module intentionally depends only on Pillow. The WebUI should still start
when NumPy/OpenCV/scikit-image are absent or broken; those libraries can replace
the internals later without changing the server/UI boundary.
"""

from __future__ import annotations

import html
import math
from collections import deque
from dataclasses import dataclass
from io import BytesIO

try:
    from PIL import Image
except ImportError as exc:  # pragma: no cover - exercised on hosts without Pillow.
    Image = None  # type: ignore[assignment]
    PIL_IMPORT_ERROR = exc
else:
    PIL_IMPORT_ERROR = None


Point = tuple[float, float]
Stroke = list[Point]
GrayImage = list[list[int]]
BoolImage = list[list[bool]]


@dataclass(frozen=True)
class RasterTraceOptions:
    trace_mode: str = "line_art"
    threshold_mode: str = "auto"
    threshold_value: int = 128
    invert: bool = False
    skeletonize: bool = True
    max_segments: int = 12000
    min_stroke_length_px: float = 2.0


@dataclass
class RasterTraceResult:
    svg: str
    stroke_count: int
    segment_count: int
    warnings: list[str]


def image_size(image: BoolImage | GrayImage) -> tuple[int, int]:
    return (len(image[0]) if image else 0, len(image))


def otsu_threshold(gray: GrayImage) -> int:
    hist = [0] * 256
    total = 0
    sum_total = 0
    for row in gray:
        for value in row:
            hist[value] += 1
            total += 1
            sum_total += value
    if total == 0:
        return 128

    sum_background = 0.0
    weight_background = 0.0
    max_variance = -1.0
    threshold = 128
    for value, count in enumerate(hist):
        weight_background += count
        if weight_background == 0:
            continue
        weight_foreground = total - weight_background
        if weight_foreground == 0:
            break
        sum_background += value * count
        mean_background = sum_background / weight_background
        mean_foreground = (sum_total - sum_background) / weight_foreground
        variance = weight_background * weight_foreground * (mean_background - mean_foreground) ** 2
        if variance > max_variance:
            max_variance = variance
            threshold = value
    return threshold


def load_grayscale(data: bytes) -> GrayImage:
    if Image is None:
        raise ValueError(f"PNG/JPEG reading requires Pillow: {PIL_IMPORT_ERROR}")
    try:
        with Image.open(BytesIO(data)) as image:
            gray = image.convert("L")
            width, height = gray.size
            pixels = list(gray.getdata())
    except Exception as exc:
        raise ValueError(f"PNG/JPEG読み込み失敗: {exc}") from exc
    return [pixels[y * width : (y + 1) * width] for y in range(height)]


def threshold_image(gray: GrayImage, options: RasterTraceOptions, warnings: list[str]) -> BoolImage:
    if options.threshold_mode == "auto":
        threshold = otsu_threshold(gray)
        warnings.append(f"auto threshold={threshold}")
    elif options.threshold_mode == "manual":
        threshold = max(0, min(255, int(options.threshold_value)))
    else:
        raise ValueError("threshold_mode must be auto or manual")
    return [[(value >= threshold) if options.invert else (value < threshold) for value in row] for row in gray]


def has_foreground(image: BoolImage) -> bool:
    return any(any(row) for row in image)


def transition_count(values: list[bool]) -> int:
    circular = values + [values[0]]
    return sum(1 for i in range(len(values)) if not circular[i] and circular[i + 1])


def copy_bool_image(image: BoolImage) -> BoolImage:
    return [row[:] for row in image]


def zhang_suen_skeletonize(image: BoolImage) -> BoolImage:
    """Small dependency-free thinning pass for line-art inputs."""

    width, height = image_size(image)
    work = copy_bool_image(image)
    if width < 3 or height < 3:
        return work
    changed = True
    while changed:
        changed = False
        for phase in (0, 1):
            remove: list[tuple[int, int]] = []
            for y in range(1, height - 1):
                for x in range(1, width - 1):
                    if not work[y][x]:
                        continue
                    p2 = work[y - 1][x]
                    p3 = work[y - 1][x + 1]
                    p4 = work[y][x + 1]
                    p5 = work[y + 1][x + 1]
                    p6 = work[y + 1][x]
                    p7 = work[y + 1][x - 1]
                    p8 = work[y][x - 1]
                    p9 = work[y - 1][x - 1]
                    neighbors = [p2, p3, p4, p5, p6, p7, p8, p9]
                    count = sum(1 for value in neighbors if value)
                    if count < 2 or count > 6:
                        continue
                    if transition_count(neighbors) != 1:
                        continue
                    if phase == 0:
                        if p2 and p4 and p6:
                            continue
                        if p4 and p6 and p8:
                            continue
                    else:
                        if p2 and p4 and p8:
                            continue
                        if p2 and p6 and p8:
                            continue
                    remove.append((y, x))
            if remove:
                changed = True
                for y, x in remove:
                    work[y][x] = False
    return work


def boundary_image(image: BoolImage) -> BoolImage:
    width, height = image_size(image)
    boundary = [[False for _ in range(width)] for _ in range(height)]
    for y in range(height):
        for x in range(width):
            if not image[y][x]:
                continue
            is_boundary = False
            for dy in (-1, 0, 1):
                for dx in (-1, 0, 1):
                    ny, nx = y + dy, x + dx
                    if ny < 0 or ny >= height or nx < 0 or nx >= width or not image[ny][nx]:
                        is_boundary = True
            boundary[y][x] = is_boundary
    return boundary


def neighbors8(point: tuple[int, int], image: BoolImage) -> list[tuple[int, int]]:
    y, x = point
    width, height = image_size(image)
    out: list[tuple[int, int]] = []
    for dy in (-1, 0, 1):
        for dx in (-1, 0, 1):
            if dy == 0 and dx == 0:
                continue
            ny, nx = y + dy, x + dx
            if 0 <= ny < height and 0 <= nx < width and image[ny][nx]:
                out.append((ny, nx))
    return out


def connected_components(image: BoolImage) -> list[list[tuple[int, int]]]:
    width, height = image_size(image)
    visited = [[False for _ in range(width)] for _ in range(height)]
    components: list[list[tuple[int, int]]] = []
    for y in range(height):
        for x in range(width):
            if visited[y][x] or not image[y][x]:
                continue
            queue: deque[tuple[int, int]] = deque([(y, x)])
            visited[y][x] = True
            component: list[tuple[int, int]] = []
            while queue:
                point = queue.popleft()
                component.append(point)
                for next_point in neighbors8(point, image):
                    ny, nx = next_point
                    if not visited[ny][nx]:
                        visited[ny][nx] = True
                        queue.append(next_point)
            components.append(component)
    return components


def nearest_unvisited(current: tuple[int, int], candidates: set[tuple[int, int]]) -> tuple[int, int]:
    return min(candidates, key=lambda point: (point[0] - current[0]) ** 2 + (point[1] - current[1]) ** 2)


def component_to_stroke(component: list[tuple[int, int]], image: BoolImage) -> Stroke:
    component_set = set(component)
    endpoints = [point for point in component if sum(1 for n in neighbors8(point, image) if n in component_set) <= 1]
    current = min(endpoints or component, key=lambda point: (point[1], point[0]))
    unvisited = set(component)
    stroke: Stroke = []
    while unvisited:
        if current not in unvisited:
            current = nearest_unvisited(current, unvisited)
        unvisited.remove(current)
        y, x = current
        stroke.append((float(x), float(y)))
        next_points = [point for point in neighbors8(current, image) if point in unvisited]
        if next_points:
            current = min(next_points, key=lambda point: (point[0] - y) ** 2 + (point[1] - x) ** 2)
    return stroke


def simplify_stroke(stroke: Stroke, step: int = 2) -> Stroke:
    if len(stroke) <= 2:
        return stroke
    out = [stroke[0]]
    out.extend(stroke[index] for index in range(step, len(stroke) - 1, step))
    out.append(stroke[-1])
    return out


def stroke_length(stroke: Stroke) -> float:
    return sum(math.dist(stroke[i - 1], stroke[i]) for i in range(1, len(stroke)))


def image_to_strokes(image: BoolImage, options: RasterTraceOptions) -> list[Stroke]:
    strokes: list[Stroke] = []
    for component in connected_components(image):
        if len(component) < 2:
            continue
        stroke = simplify_stroke(component_to_stroke(component, image))
        if len(stroke) < 2 or stroke_length(stroke) < options.min_stroke_length_px:
            continue
        strokes.append(stroke)
    return strokes


def normalize_strokes(strokes: list[Stroke]) -> list[Stroke]:
    points = [point for stroke in strokes for point in stroke]
    if not points:
        return []
    min_x = min(point[0] for point in points)
    min_y = min(point[1] for point in points)
    max_x = max(point[0] for point in points)
    max_y = max(point[1] for point in points)
    width = max(1.0, max_x - min_x)
    height = max(1.0, max_y - min_y)
    return [[((x - min_x) / width * 100.0, (y - min_y) / height * 100.0) for x, y in stroke] for stroke in strokes]


def svg_polyline(stroke: Stroke) -> str:
    points = " ".join(f"{x:.3f},{y:.3f}" for x, y in stroke)
    return f'<polyline points="{html.escape(points)}" />'


def strokes_to_svg(strokes: list[Stroke]) -> str:
    body = "\n  ".join(svg_polyline(stroke) for stroke in strokes)
    return (
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" '
        'fill="none" stroke="black" stroke-width="1" stroke-linecap="round" stroke-linejoin="round">\n'
        f"  {body}\n"
        "</svg>\n"
    )


def validate_options(options: RasterTraceOptions) -> None:
    if options.trace_mode not in {"line_art", "outline"}:
        raise ValueError("trace_mode must be line_art or outline")
    if options.threshold_mode not in {"auto", "manual"}:
        raise ValueError("threshold_mode must be auto or manual")
    if not 0 <= options.threshold_value <= 255:
        raise ValueError("threshold_value must be between 0 and 255")
    if options.max_segments < 1:
        raise ValueError("max_segments must be >= 1")


def trace_raster_image_to_svg(data: bytes, options: RasterTraceOptions) -> RasterTraceResult:
    validate_options(options)
    warnings: list[str] = []
    gray = load_grayscale(data)
    foreground = threshold_image(gray, options, warnings)
    if not has_foreground(foreground):
        raise ValueError("画像trace結果が空です")
    if options.trace_mode == "line_art":
        traced = zhang_suen_skeletonize(foreground) if options.skeletonize else foreground
    else:
        traced = boundary_image(foreground)
    strokes = normalize_strokes(image_to_strokes(traced, options))
    if not strokes:
        raise ValueError("画像trace結果が空です")
    segment_count = sum(len(stroke) - 1 for stroke in strokes)
    if segment_count > options.max_segments:
        raise ValueError(f"segment count {segment_count} exceeds max_segments {options.max_segments}")
    return RasterTraceResult(
        svg=strokes_to_svg(strokes),
        stroke_count=len(strokes),
        segment_count=segment_count,
        warnings=warnings,
    )


def trace_mode_from_value(value: object) -> str:
    normalized = str(value or "line_art").strip().lower().replace("-", "_").replace(" ", "_")
    if normalized in {"line_art", "lineart"}:
        return "line_art"
    if normalized in {"outline", "outline_trace"}:
        return "outline"
    raise ValueError("trace_mode must be Line Art or Outline Trace")
