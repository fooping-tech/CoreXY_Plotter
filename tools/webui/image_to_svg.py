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
    trace_mode: str = "outline"
    trace_detail: str = "high"
    threshold_mode: str = "auto"
    threshold_value: int = 128
    invert: bool = False
    skeletonize: bool = True
    max_segments: int = 12000
    min_stroke_length_px: float = 2.0
    hatch_enabled: bool = False
    hatch_threshold: int = 96
    hatch_pitch_px: int = 8


@dataclass
class RasterTraceResult:
    svg: str
    stroke_count: int
    segment_count: int
    warnings: list[str]


@dataclass(frozen=True)
class NormalizedStrokes:
    strokes: list[Stroke]
    width: float
    height: float


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


def point_line_distance(point: Point, start: Point, end: Point) -> float:
    line_len = math.dist(start, end)
    if line_len == 0:
        return math.dist(point, start)
    return abs((end[0] - start[0]) * (start[1] - point[1]) - (start[0] - point[0]) * (end[1] - start[1])) / line_len


def simplify_stroke(stroke: Stroke, tolerance: float) -> Stroke:
    if tolerance <= 0 or len(stroke) <= 2:
        return stroke
    max_distance = -1.0
    index = 0
    for i in range(1, len(stroke) - 1):
        distance = point_line_distance(stroke[i], stroke[0], stroke[-1])
        if distance > max_distance:
            max_distance = distance
            index = i
    if max_distance > tolerance:
        left = simplify_stroke(stroke[: index + 1], tolerance)
        right = simplify_stroke(stroke[index:], tolerance)
        return left[:-1] + right
    return [stroke[0], stroke[-1]]


def detail_tolerance_px(options: RasterTraceOptions) -> float:
    if options.trace_detail == "high":
        return 0.25
    if options.trace_detail == "simple":
        return 1.6
    return 0.75


def stroke_length(stroke: Stroke) -> float:
    return sum(math.dist(stroke[i - 1], stroke[i]) for i in range(1, len(stroke)))


def image_to_strokes(image: BoolImage, options: RasterTraceOptions) -> list[Stroke]:
    strokes: list[Stroke] = []
    for component in connected_components(image):
        if len(component) < 2:
            continue
        stroke = simplify_stroke(component_to_stroke(component, image), detail_tolerance_px(options))
        if len(stroke) < 2 or stroke_length(stroke) < options.min_stroke_length_px:
            continue
        strokes.append(stroke)
    return strokes


def padded_bool_image(image: BoolImage) -> BoolImage:
    width, _ = image_size(image)
    padding = [False for _ in range(width + 2)]
    return [padding[:], *[[False, *row, False] for row in image], padding[:]]


def marching_square_segments(image: BoolImage) -> list[tuple[tuple[int, int], tuple[int, int]]]:
    """Return contour segments as doubled integer coordinates.

    Endpoints are stored at 2x scale so half-pixel marching-square positions
    remain exact integers. This avoids diagonal jumps across filled areas.
    """

    padded = padded_bool_image(image)
    width, height = image_size(padded)
    segments: list[tuple[tuple[int, int], tuple[int, int]]] = []

    def p_top(x: int, y: int) -> tuple[int, int]:
        return (2 * x + 1, 2 * y)

    def p_right(x: int, y: int) -> tuple[int, int]:
        return (2 * x + 2, 2 * y + 1)

    def p_bottom(x: int, y: int) -> tuple[int, int]:
        return (2 * x + 1, 2 * y + 2)

    def p_left(x: int, y: int) -> tuple[int, int]:
        return (2 * x, 2 * y + 1)

    for y in range(height - 1):
        for x in range(width - 1):
            tl = padded[y][x]
            tr = padded[y][x + 1]
            br = padded[y + 1][x + 1]
            bl = padded[y + 1][x]
            case = (8 if tl else 0) | (4 if tr else 0) | (2 if br else 0) | (1 if bl else 0)
            top = p_top(x, y)
            right = p_right(x, y)
            bottom = p_bottom(x, y)
            left = p_left(x, y)
            if case in {0, 15}:
                continue
            if case == 1:
                segments.append((left, bottom))
            elif case == 2:
                segments.append((bottom, right))
            elif case == 3:
                segments.append((left, right))
            elif case == 4:
                segments.append((top, right))
            elif case == 5:
                segments.extend([(top, left), (bottom, right)])
            elif case == 6:
                segments.append((top, bottom))
            elif case == 7:
                segments.append((top, left))
            elif case == 8:
                segments.append((left, top))
            elif case == 9:
                segments.append((top, bottom))
            elif case == 10:
                segments.extend([(left, bottom), (top, right)])
            elif case == 11:
                segments.append((top, right))
            elif case == 12:
                segments.append((left, right))
            elif case == 13:
                segments.append((bottom, right))
            elif case == 14:
                segments.append((left, bottom))
    return segments


def contour_segments_to_strokes(segments: list[tuple[tuple[int, int], tuple[int, int]]]) -> list[Stroke]:
    graph: dict[tuple[int, int], set[tuple[int, int]]] = {}
    unused: set[tuple[tuple[int, int], tuple[int, int]]] = set()

    def edge_key(a: tuple[int, int], b: tuple[int, int]) -> tuple[tuple[int, int], tuple[int, int]]:
        return (a, b) if a <= b else (b, a)

    for a, b in segments:
        if a == b:
            continue
        graph.setdefault(a, set()).add(b)
        graph.setdefault(b, set()).add(a)
        unused.add(edge_key(a, b))

    def take_next(current: tuple[int, int], previous: tuple[int, int] | None) -> tuple[int, int] | None:
        candidates = [point for point in graph.get(current, set()) if edge_key(current, point) in unused]
        if not candidates:
            return None
        if previous is not None and len(candidates) > 1:
            candidates.sort(key=lambda point: point == previous)
        return candidates[0]

    def consume(start: tuple[int, int], next_point: tuple[int, int]) -> list[tuple[int, int]]:
        path = [start, next_point]
        unused.remove(edge_key(start, next_point))
        previous = start
        current = next_point
        while True:
            following = take_next(current, previous)
            if following is None:
                break
            unused.remove(edge_key(current, following))
            path.append(following)
            previous, current = current, following
            if current == start:
                break
        return path

    strokes: list[Stroke] = []
    while unused:
        start, next_point = next(iter(unused))
        raw_path = consume(start, next_point)
        if len(raw_path) < 2:
            continue
        # Undo the padding shift and 2x coordinate scale.
        stroke = [((x / 2.0) - 1.0, (y / 2.0) - 1.0) for x, y in raw_path]
        if len(stroke) >= 2:
            strokes.append(stroke)
    return strokes


def outline_to_strokes(image: BoolImage, options: RasterTraceOptions) -> list[Stroke]:
    strokes: list[Stroke] = []
    for stroke in contour_segments_to_strokes(marching_square_segments(image)):
        simplified = simplify_stroke(stroke, detail_tolerance_px(options))
        if len(simplified) < 2 or stroke_length(simplified) < options.min_stroke_length_px:
            continue
        strokes.append(simplified)
    return strokes


def normalize_strokes(strokes: list[Stroke]) -> NormalizedStrokes:
    points = [point for stroke in strokes for point in stroke]
    if not points:
        return NormalizedStrokes([], 0.0, 0.0)
    min_x = min(point[0] for point in points)
    min_y = min(point[1] for point in points)
    max_x = max(point[0] for point in points)
    max_y = max(point[1] for point in points)
    width = max(1.0, max_x - min_x)
    height = max(1.0, max_y - min_y)
    return NormalizedStrokes([[(x - min_x, y - min_y) for x, y in stroke] for stroke in strokes], width, height)


def svg_polyline(stroke: Stroke) -> str:
    points = " ".join(f"{x:.3f},{y:.3f}" for x, y in stroke)
    return f'<polyline points="{html.escape(points)}" />'


def strokes_to_svg(strokes: NormalizedStrokes) -> str:
    view_width = max(1.0, strokes.width)
    view_height = max(1.0, strokes.height)
    body = "\n  ".join(svg_polyline(stroke) for stroke in strokes.strokes)
    return (
        f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {view_width:.3f} {view_height:.3f}" '
        'fill="none" stroke="black" stroke-width="1" stroke-linecap="round" stroke-linejoin="round">\n'
        f"  {body}\n"
        "</svg>\n"
    )


def validate_options(options: RasterTraceOptions) -> None:
    if options.trace_mode not in {"line_art", "outline"}:
        raise ValueError("trace_mode must be line_art or outline")
    if options.trace_detail not in {"simple", "balanced", "high"}:
        raise ValueError("trace_detail must be simple, balanced, or high")
    if options.threshold_mode not in {"auto", "manual"}:
        raise ValueError("threshold_mode must be auto or manual")
    if not 0 <= options.threshold_value <= 255:
        raise ValueError("threshold_value must be between 0 and 255")
    if options.max_segments < 1:
        raise ValueError("max_segments must be >= 1")
    if not 0 <= options.hatch_threshold <= 255:
        raise ValueError("hatch_threshold must be between 0 and 255")
    if options.hatch_pitch_px < 1:
        raise ValueError("hatch_pitch_px must be >= 1")


def hatch_mask(gray: GrayImage, options: RasterTraceOptions) -> BoolImage:
    threshold = max(0, min(255, int(options.hatch_threshold)))
    return [[(value >= threshold) if options.invert else (value < threshold) for value in row] for row in gray]


def hatch_strokes(image: BoolImage, options: RasterTraceOptions) -> list[Stroke]:
    width, height = image_size(image)
    pitch = max(1, int(options.hatch_pitch_px))
    strokes: list[Stroke] = []
    for y in range(0, height, pitch):
        x = 0
        while x < width:
            while x < width and not image[y][x]:
                x += 1
            start = x
            while x < width and image[y][x]:
                x += 1
            end = x - 1
            if end - start >= options.min_stroke_length_px:
                strokes.append([(float(start), float(y)), (float(end), float(y))])
    return strokes


def trace_raster_image_to_svg(data: bytes, options: RasterTraceOptions) -> RasterTraceResult:
    validate_options(options)
    warnings: list[str] = []
    gray = load_grayscale(data)
    foreground = threshold_image(gray, options, warnings)
    if not has_foreground(foreground):
        raise ValueError("画像trace結果が空です")
    if options.trace_mode == "line_art":
        traced = zhang_suen_skeletonize(foreground) if options.skeletonize else foreground
        raw_strokes = image_to_strokes(traced, options)
    else:
        raw_strokes = outline_to_strokes(foreground, options)
    if options.hatch_enabled:
        raw_strokes.extend(hatch_strokes(hatch_mask(gray, options), options))
    strokes = normalize_strokes(raw_strokes)
    if not strokes.strokes:
        raise ValueError("画像trace結果が空です")
    segment_count = sum(len(stroke) - 1 for stroke in strokes.strokes)
    if segment_count > options.max_segments:
        raise ValueError(f"segment count {segment_count} exceeds max_segments {options.max_segments}")
    return RasterTraceResult(
        svg=strokes_to_svg(strokes),
        stroke_count=len(strokes.strokes),
        segment_count=segment_count,
        warnings=warnings,
    )


def trace_mode_from_value(value: object) -> str:
    normalized = str(value or "outline").strip().lower().replace("-", "_").replace(" ", "_")
    if normalized in {"line_art", "lineart"}:
        return "line_art"
    if normalized in {"outline", "outline_trace"}:
        return "outline"
    raise ValueError("trace_mode must be Line Art or Outline Trace")
