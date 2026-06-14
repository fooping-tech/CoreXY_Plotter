#!/usr/bin/env python3
"""Convert a limited SVG subset into pen-plotter G-code."""

from __future__ import annotations

import argparse
import math
import re
import sys
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Iterable
from xml.etree import ElementTree


Point = tuple[float, float]
Stroke = list[Point]

COMMAND_RE = re.compile(r"[MmZzLlHhVvCcSsQqTtAa]|[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?")
NUMBER_RE = re.compile(r"[-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?")
LENGTH_RE = re.compile(r"^\s*([-+]?(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][-+]?\d+)?)")
TRANSFORM_RE = re.compile(r"([a-zA-Z]+)\s*\(([^)]*)\)")
SUPPORTED_TAGS = {"path", "polyline", "polygon", "line", "rect", "circle", "ellipse"}


@dataclass(frozen=True)
class SvgToGcodeOptions:
    width_mm: float = 50.0
    height_mm: float = 50.0
    margin_mm: float = 5.0
    feed_mm_min: float = 800.0
    travel_feed_mm_min: float = 1200.0
    simplify_tolerance_mm: float = 0.2
    min_stroke_length_mm: float = 0.5
    optimize_stroke_order: bool = True
    curve_steps: int = 20


@dataclass
class SvgToGcodeResult:
    filename: str
    gcode: str
    stroke_count: int
    segment_count: int
    warnings: list[str]


@dataclass(frozen=True)
class Matrix:
    a: float = 1.0
    b: float = 0.0
    c: float = 0.0
    d: float = 1.0
    e: float = 0.0
    f: float = 0.0

    def multiply(self, other: "Matrix") -> "Matrix":
        return Matrix(
            self.a * other.a + self.c * other.b,
            self.b * other.a + self.d * other.b,
            self.a * other.c + self.c * other.d,
            self.b * other.c + self.d * other.d,
            self.a * other.e + self.c * other.f + self.e,
            self.b * other.e + self.d * other.f + self.f,
        )

    def apply(self, point: Point) -> Point:
        x, y = point
        return (self.a * x + self.c * y + self.e, self.b * x + self.d * y + self.f)


def strip_namespace(tag: str) -> str:
    return tag.rsplit("}", 1)[-1].lower()


def parse_float(value: str | None, default: float = 0.0) -> float:
    if value is None:
        return default
    match = LENGTH_RE.match(value)
    if not match:
        return default
    return float(match.group(1))


def parse_points(value: str | None) -> Stroke:
    if not value:
        return []
    numbers = [float(match.group(0)) for match in NUMBER_RE.finditer(value)]
    return [(numbers[i], numbers[i + 1]) for i in range(0, len(numbers) - 1, 2)]


def parse_transform(value: str | None, warnings: list[str]) -> Matrix:
    matrix = Matrix()
    if not value:
        return matrix
    for name, raw_args in TRANSFORM_RE.findall(value):
        args = [float(match.group(0)) for match in NUMBER_RE.finditer(raw_args)]
        lower_name = name.lower()
        if lower_name == "translate":
            tx = args[0] if args else 0.0
            ty = args[1] if len(args) > 1 else 0.0
            next_matrix = Matrix(e=tx, f=ty)
        elif lower_name == "scale":
            sx = args[0] if args else 1.0
            sy = args[1] if len(args) > 1 else sx
            next_matrix = Matrix(a=sx, d=sy)
        else:
            warnings.append(f"ignored unsupported transform: {name}")
            continue
        matrix = matrix.multiply(next_matrix)
    return matrix


def cubic_point(p0: Point, p1: Point, p2: Point, p3: Point, t: float) -> Point:
    u = 1.0 - t
    return (
        u * u * u * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t * t * t * p3[0],
        u * u * u * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t * t * t * p3[1],
    )


def quadratic_point(p0: Point, p1: Point, p2: Point, t: float) -> Point:
    u = 1.0 - t
    return (
        u * u * p0[0] + 2 * u * t * p1[0] + t * t * p2[0],
        u * u * p0[1] + 2 * u * t * p1[1] + t * t * p2[1],
    )


def tokenize_path(data: str) -> list[str]:
    return [match.group(0) for match in COMMAND_RE.finditer(data)]


def is_command(token: str) -> bool:
    return len(token) == 1 and token.isalpha()


def has_number(tokens: list[str], index: int) -> bool:
    return index < len(tokens) and not is_command(tokens[index])


def read_number(tokens: list[str], index: int) -> tuple[float, int]:
    if index >= len(tokens) or is_command(tokens[index]):
        raise ValueError("path data ended before expected number")
    return float(tokens[index]), index + 1


def path_to_strokes(data: str | None, warnings: list[str], curve_steps: int) -> list[Stroke]:
    if not data:
        return []
    tokens = tokenize_path(data)
    strokes: list[Stroke] = []
    current: Point = (0.0, 0.0)
    start: Point = (0.0, 0.0)
    stroke: Stroke = []
    index = 0
    command = ""

    def finish_stroke() -> None:
        nonlocal stroke
        if stroke:
            strokes.append(stroke)
            stroke = []

    def point_arg(relative: bool) -> None:
        nonlocal current
        x, next_index = read_number(tokens, index_state[0])
        y, next_index = read_number(tokens, next_index)
        index_state[0] = next_index
        current = (current[0] + x, current[1] + y) if relative else (x, y)

    index_state = [0]
    while index_state[0] < len(tokens):
        token = tokens[index_state[0]]
        if is_command(token):
            command = token
            index_state[0] += 1
        elif not command:
            warnings.append("ignored path data before first command")
            break

        lower = command.lower()
        relative = command.islower()
        try:
            if lower == "m":
                first = True
                while has_number(tokens, index_state[0]):
                    x, next_index = read_number(tokens, index_state[0])
                    y, next_index = read_number(tokens, next_index)
                    index_state[0] = next_index
                    current = (current[0] + x, current[1] + y) if relative else (x, y)
                    if first:
                        finish_stroke()
                        stroke = [current]
                        start = current
                        first = False
                    else:
                        stroke.append(current)
                command = "l" if relative else "L"
            elif lower == "l":
                while has_number(tokens, index_state[0]):
                    point_arg(relative)
                    stroke.append(current)
            elif lower == "h":
                while has_number(tokens, index_state[0]):
                    x, next_index = read_number(tokens, index_state[0])
                    index_state[0] = next_index
                    current = (current[0] + x, current[1]) if relative else (x, current[1])
                    stroke.append(current)
            elif lower == "v":
                while has_number(tokens, index_state[0]):
                    y, next_index = read_number(tokens, index_state[0])
                    index_state[0] = next_index
                    current = (current[0], current[1] + y) if relative else (current[0], y)
                    stroke.append(current)
            elif lower == "c":
                while has_number(tokens, index_state[0]):
                    values = []
                    for _ in range(6):
                        value, next_index = read_number(tokens, index_state[0])
                        index_state[0] = next_index
                        values.append(value)
                    p1 = (values[0], values[1])
                    p2 = (values[2], values[3])
                    p3 = (values[4], values[5])
                    if relative:
                        p1 = (current[0] + p1[0], current[1] + p1[1])
                        p2 = (current[0] + p2[0], current[1] + p2[1])
                        p3 = (current[0] + p3[0], current[1] + p3[1])
                    p0 = current
                    for step in range(1, max(2, curve_steps) + 1):
                        stroke.append(cubic_point(p0, p1, p2, p3, step / max(2, curve_steps)))
                    current = p3
            elif lower == "q":
                while has_number(tokens, index_state[0]):
                    values = []
                    for _ in range(4):
                        value, next_index = read_number(tokens, index_state[0])
                        index_state[0] = next_index
                        values.append(value)
                    p1 = (values[0], values[1])
                    p2 = (values[2], values[3])
                    if relative:
                        p1 = (current[0] + p1[0], current[1] + p1[1])
                        p2 = (current[0] + p2[0], current[1] + p2[1])
                    p0 = current
                    for step in range(1, max(2, curve_steps) + 1):
                        stroke.append(quadratic_point(p0, p1, p2, step / max(2, curve_steps)))
                    current = p2
            elif lower == "z":
                if stroke and current != start:
                    stroke.append(start)
                current = start
                finish_stroke()
            else:
                warnings.append(f"ignored unsupported path command: {command}")
                finish_stroke()
                while has_number(tokens, index_state[0]):
                    index_state[0] += 1
        except ValueError as exc:
            warnings.append(f"ignored malformed path segment: {exc}")
            finish_stroke()
            break
    finish_stroke()
    return strokes


def element_to_strokes(element: ElementTree.Element, warnings: list[str], curve_steps: int) -> list[Stroke]:
    tag = strip_namespace(element.tag)
    if tag == "path":
        return path_to_strokes(element.get("d"), warnings, curve_steps)
    if tag in {"polyline", "polygon"}:
        points = parse_points(element.get("points"))
        if tag == "polygon" and len(points) > 1 and points[0] != points[-1]:
            points.append(points[0])
        return [points] if points else []
    if tag == "line":
        return [[
            (parse_float(element.get("x1")), parse_float(element.get("y1"))),
            (parse_float(element.get("x2")), parse_float(element.get("y2"))),
        ]]
    if tag == "rect":
        x = parse_float(element.get("x"))
        y = parse_float(element.get("y"))
        width = parse_float(element.get("width"))
        height = parse_float(element.get("height"))
        if width <= 0 or height <= 0:
            return []
        return [[(x, y), (x + width, y), (x + width, y + height), (x, y + height), (x, y)]]
    if tag in {"circle", "ellipse"}:
        cx = parse_float(element.get("cx"))
        cy = parse_float(element.get("cy"))
        rx = parse_float(element.get("r")) if tag == "circle" else parse_float(element.get("rx"))
        ry = parse_float(element.get("r")) if tag == "circle" else parse_float(element.get("ry"))
        if rx <= 0 or ry <= 0:
            return []
        steps = max(12, curve_steps * 2)
        points = [
            (cx + math.cos((2.0 * math.pi * i) / steps) * rx, cy + math.sin((2.0 * math.pi * i) / steps) * ry)
            for i in range(steps + 1)
        ]
        return [points]
    return []


def collect_svg_strokes(root: ElementTree.Element, warnings: list[str], curve_steps: int) -> list[Stroke]:
    strokes: list[Stroke] = []

    def visit(element: ElementTree.Element, parent_transform: Matrix) -> None:
        tag = strip_namespace(element.tag)
        transform = parent_transform.multiply(parse_transform(element.get("transform"), warnings))
        if tag in {"text", "image"}:
            warnings.append(f"ignored unsupported SVG element: {tag}")
            return
        if tag in {"lineargradient", "radialgradient"}:
            warnings.append(f"ignored unsupported SVG gradient: {tag}")
            return
        if tag in {"defs", "pattern", "clippath", "mask"}:
            return
        if element.get("fill") and element.get("fill", "").strip().lower() not in {"none", "transparent"}:
            warnings.append(f"ignored fill on <{tag}>; stroke outlines only")
        if tag in SUPPORTED_TAGS:
            for stroke in element_to_strokes(element, warnings, curve_steps):
                strokes.append([transform.apply(point) for point in stroke])
        for child in list(element):
            visit(child, transform)

    visit(root, Matrix())
    return strokes


def stroke_length(stroke: Stroke) -> float:
    return sum(math.dist(stroke[i - 1], stroke[i]) for i in range(1, len(stroke)))


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


def remove_duplicate_points(stroke: Stroke) -> Stroke:
    cleaned: Stroke = []
    for point in stroke:
        if not cleaned or math.dist(cleaned[-1], point) > 1e-6:
            cleaned.append(point)
    return cleaned


def fit_strokes_to_canvas(strokes: list[Stroke], options: SvgToGcodeOptions) -> list[Stroke]:
    points = [point for stroke in strokes for point in stroke]
    if not points:
        return []
    min_x = min(point[0] for point in points)
    max_x = max(point[0] for point in points)
    min_y = min(point[1] for point in points)
    max_y = max(point[1] for point in points)
    svg_width = max_x - min_x
    svg_height = max_y - min_y
    drawable_width = options.width_mm - 2.0 * options.margin_mm
    drawable_height = options.height_mm - 2.0 * options.margin_mm
    if (svg_width <= 0 and svg_height <= 0) or drawable_width <= 0 or drawable_height <= 0:
        return []
    scale_candidates = []
    if svg_width > 0:
        scale_candidates.append(drawable_width / svg_width)
    if svg_height > 0:
        scale_candidates.append(drawable_height / svg_height)
    scale = min(scale_candidates)
    fitted_width = svg_width * scale
    fitted_height = svg_height * scale
    offset_x = options.margin_mm + (drawable_width - fitted_width) / 2.0
    offset_y = options.margin_mm + (drawable_height - fitted_height) / 2.0

    fitted: list[Stroke] = []
    for stroke in strokes:
        out: Stroke = []
        for x, y in stroke:
            x_mm = offset_x + (x - min_x) * scale
            y_mm = offset_y + (max_y - y) * scale
            x_mm = min(options.width_mm, max(0.0, x_mm))
            y_mm = min(options.height_mm, max(0.0, y_mm))
            out.append((x_mm, y_mm))
        fitted.append(out)
    return fitted


def optimize_strokes(strokes: list[Stroke]) -> list[Stroke]:
    remaining = [stroke[:] for stroke in strokes]
    ordered: list[Stroke] = []
    current: Point = (0.0, 0.0)
    while remaining:
        best_index = 0
        best_reverse = False
        best_distance = float("inf")
        for index, stroke in enumerate(remaining):
            start_distance = math.dist(current, stroke[0])
            end_distance = math.dist(current, stroke[-1])
            if start_distance < best_distance:
                best_index = index
                best_reverse = False
                best_distance = start_distance
            if end_distance < best_distance:
                best_index = index
                best_reverse = True
                best_distance = end_distance
        stroke = remaining.pop(best_index)
        if best_reverse:
            stroke.reverse()
        ordered.append(stroke)
        current = stroke[-1]
    return ordered


def fmt(value: float) -> str:
    text = f"{value:.3f}".rstrip("0").rstrip(".")
    return text if text and text != "-0" else "0"


def strokes_to_gcode(strokes: list[Stroke], options: SvgToGcodeOptions) -> str:
    lines = ["G21", "G90", "M5"]
    for stroke in strokes:
        start = stroke[0]
        if lines[-1] != "M5":
            lines.append("M5")
        lines.extend([
            f"G0 X{fmt(start[0])} Y{fmt(start[1])} F{fmt(options.travel_feed_mm_min)}",
            "M3",
        ])
        for point in stroke[1:]:
            lines.append(f"G1 X{fmt(point[0])} Y{fmt(point[1])} F{fmt(options.feed_mm_min)}")
        lines.append("M5")
    if lines[-1] != "M5":
        lines.append("M5")
    return "\n".join(lines) + "\n"


def validate_options(options: SvgToGcodeOptions) -> None:
    if options.width_mm <= 0 or options.height_mm <= 0:
        raise ValueError("width_mm and height_mm must be > 0")
    if options.margin_mm < 0:
        raise ValueError("margin_mm must be >= 0")
    if options.margin_mm * 2 >= min(options.width_mm, options.height_mm):
        raise ValueError("margin_mm is too large for the requested canvas")
    if options.feed_mm_min <= 0 or options.travel_feed_mm_min <= 0:
        raise ValueError("feed values must be > 0")
    if options.simplify_tolerance_mm < 0 or options.min_stroke_length_mm < 0:
        raise ValueError("simplify_tolerance_mm and min_stroke_length_mm must be >= 0")


def convert_svg_to_gcode(svg: str, options: SvgToGcodeOptions, *, timestamp: datetime | None = None) -> SvgToGcodeResult:
    validate_options(options)
    if not svg.strip():
        raise ValueError("svg is required")
    warnings: list[str] = []
    try:
        root = ElementTree.fromstring(svg)
    except ElementTree.ParseError as exc:
        raise ValueError(f"SVG parse failed: {exc}") from exc
    if strip_namespace(root.tag) != "svg":
        raise ValueError("input must be an <svg> document")

    raw_strokes = collect_svg_strokes(root, warnings, options.curve_steps)
    fitted = fit_strokes_to_canvas(raw_strokes, options)
    cleaned: list[Stroke] = []
    for stroke in fitted:
        simplified = simplify_stroke(remove_duplicate_points(stroke), options.simplify_tolerance_mm)
        simplified = remove_duplicate_points(simplified)
        if len(simplified) < 2:
            continue
        if stroke_length(simplified) < options.min_stroke_length_mm:
            continue
        cleaned.append(simplified)
    if options.optimize_stroke_order:
        cleaned = optimize_strokes(cleaned)
    if not cleaned:
        raise ValueError("SVG produced no drawable strokes")
    gcode = strokes_to_gcode(cleaned, options)
    stamp = (timestamp or datetime.now()).strftime("%Y%m%d_%H%M%S")
    return SvgToGcodeResult(
        filename=f"generated_svg_{stamp}.gcode",
        gcode=gcode,
        stroke_count=len(cleaned),
        segment_count=sum(len(stroke) - 1 for stroke in cleaned),
        warnings=warnings,
    )


def options_from_mapping(data: dict[str, object]) -> SvgToGcodeOptions:
    return SvgToGcodeOptions(
        width_mm=float(data.get("width_mm", 50)),
        height_mm=float(data.get("height_mm", 50)),
        margin_mm=float(data.get("margin_mm", 5)),
        feed_mm_min=float(data.get("feed_mm_min", 800)),
        travel_feed_mm_min=float(data.get("travel_feed_mm_min", 1200)),
        simplify_tolerance_mm=float(data.get("simplify_tolerance_mm", 0.2)),
        min_stroke_length_mm=float(data.get("min_stroke_length_mm", 0.5)),
        optimize_stroke_order=bool(data.get("optimize_stroke_order", True)),
    )


def parse_args(argv: Iterable[str]) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Convert SVG into plotter G-code.")
    parser.add_argument("svg", type=Path)
    parser.add_argument("-o", "--output", type=Path)
    parser.add_argument("--width-mm", type=float, default=50.0)
    parser.add_argument("--height-mm", type=float, default=50.0)
    parser.add_argument("--margin-mm", type=float, default=5.0)
    parser.add_argument("--feed-mm-min", type=float, default=800.0)
    parser.add_argument("--travel-feed-mm-min", type=float, default=1200.0)
    parser.add_argument("--simplify-tolerance-mm", type=float, default=0.2)
    parser.add_argument("--min-stroke-length-mm", type=float, default=0.5)
    parser.add_argument("--no-optimize-stroke-order", action="store_true")
    return parser.parse_args(list(argv))


def main(argv: Iterable[str] = sys.argv[1:]) -> int:
    args = parse_args(argv)
    options = SvgToGcodeOptions(
        width_mm=args.width_mm,
        height_mm=args.height_mm,
        margin_mm=args.margin_mm,
        feed_mm_min=args.feed_mm_min,
        travel_feed_mm_min=args.travel_feed_mm_min,
        simplify_tolerance_mm=args.simplify_tolerance_mm,
        min_stroke_length_mm=args.min_stroke_length_mm,
        optimize_stroke_order=not args.no_optimize_stroke_order,
    )
    result = convert_svg_to_gcode(args.svg.read_text(encoding="utf-8"), options)
    output = args.output or Path(result.filename)
    output.write_text(result.gcode, encoding="utf-8")
    for warning in result.warnings:
        print(f"warning: {warning}", file=sys.stderr)
    print(f"{output}: {result.stroke_count} strokes, {result.segment_count} segments")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
