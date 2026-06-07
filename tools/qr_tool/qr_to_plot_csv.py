#!/usr/bin/env python3
"""Generate plotter CSV commands and an SVG preview from QR text."""

from __future__ import annotations

import argparse
import csv
import math
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable, Sequence
from xml.sax.saxutils import escape

import qrcode
from qrcode.constants import (
    ERROR_CORRECT_H,
    ERROR_CORRECT_L,
    ERROR_CORRECT_M,
    ERROR_CORRECT_Q,
)


CSV_HEADER = ("command", "delay_ms", "expect", "comment")
XY_EXPECT = "ACK_XY target="
PREAMBLE_ROWS = (
    ("CONFIG", "Show firmware configuration before high-speed square distortion check"),
    ("SELFTEST", "Validate CoreXY conversion before high-speed drawing"),
    ("TMC_INIT", "Initialize TMC2209 UART manager"),
    ("TMC_STATUS", "Show TMC2209 status before high-speed drawing"),
    ("PENUP", "Start with pen up"),
    ("ZERO", "Forget any stale logical position before clearing alarm"),
    ("ALARM_CLEAR", "Clear any previous alarm before homing"),
    ("LIMIT_STATUS", "Record limit state before homing"),
    ("HOME", "Home before drawing high-speed concentric squares"),
    ("POS", "Confirm machine is homed"),
)
DEFAULT_ORIGIN_X_MM = 0.0
DEFAULT_ORIGIN_Y_MM = 0.0
DEFAULT_MODULE_MM = 1.0
DEFAULT_HATCH_PITCH_MM = 0.35
DEFAULT_DRAW_FEED_MM_MIN = 600.0
DEFAULT_TRAVEL_FEED_MM_MIN = 1800.0
DEFAULT_ERROR_CORRECTION = "M"
QUIET_ZONE_MODULES = 4
POSITION_DECIMALS = 3


ERROR_CORRECTION_LEVELS = {
    "L": ERROR_CORRECT_L,
    "M": ERROR_CORRECT_M,
    "Q": ERROR_CORRECT_Q,
    "H": ERROR_CORRECT_H,
}


@dataclass(frozen=True)
class RectModules:
    x: int
    y: int
    width: int
    height: int = 1


@dataclass(frozen=True)
class RectMm:
    x0_mm: float
    y0_mm: float
    x1_mm: float
    y1_mm: float


@dataclass(frozen=True)
class StrokePath:
    points_mm: tuple[tuple[float, float], ...]
    kind: str


@dataclass(frozen=True)
class CsvRow:
    command: str
    delay_ms: int
    expect: str
    comment: str


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Generate CoreXY plotter CSV and SVG preview for a QR code."
    )
    parser.add_argument("--text", required=True, help="Text or URL to encode.")
    parser.add_argument(
        "--output",
        required=True,
        type=Path,
        help="Output CSV path for tools/serial_tool/serial_send.py.",
    )
    parser.add_argument(
        "--preview-svg",
        type=Path,
        help="Optional SVG path showing the generated hatch strokes.",
    )
    parser.add_argument(
        "--origin-x",
        type=float,
        default=DEFAULT_ORIGIN_X_MM,
        help=f"QR top-left origin X in mm. Default: {DEFAULT_ORIGIN_X_MM}.",
    )
    parser.add_argument(
        "--origin-y",
        type=float,
        default=DEFAULT_ORIGIN_Y_MM,
        help=f"QR top-left origin Y in mm. Default: {DEFAULT_ORIGIN_Y_MM}.",
    )
    parser.add_argument(
        "--module-mm",
        type=float,
        default=DEFAULT_MODULE_MM,
        help=f"QR module size in mm. Default: {DEFAULT_MODULE_MM}.",
    )
    parser.add_argument(
        "--hatch-pitch-mm",
        type=float,
        default=DEFAULT_HATCH_PITCH_MM,
        help=f"Spacing between fill hatch lines in mm. Default: {DEFAULT_HATCH_PITCH_MM}.",
    )
    parser.add_argument(
        "--draw-feed",
        type=float,
        default=DEFAULT_DRAW_FEED_MM_MIN,
        help=f"Feed for pen-down hatch strokes in mm/min. Default: {DEFAULT_DRAW_FEED_MM_MIN}.",
    )
    parser.add_argument(
        "--travel-feed",
        type=float,
        default=DEFAULT_TRAVEL_FEED_MM_MIN,
        help=f"Feed for pen-up travel moves in mm/min. Default: {DEFAULT_TRAVEL_FEED_MM_MIN}.",
    )
    parser.add_argument(
        "--error-correction",
        choices=tuple(ERROR_CORRECTION_LEVELS.keys()),
        default=DEFAULT_ERROR_CORRECTION,
        help=f"QR error correction level. Default: {DEFAULT_ERROR_CORRECTION}.",
    )
    parser.add_argument(
        "--version",
        type=int,
        help="Optional fixed QR version from 1 to 40. Default: auto fit.",
    )
    return parser.parse_args()


def validate_args(args: argparse.Namespace) -> None:
    if not args.text:
        raise ValueError("--text must not be empty")
    if args.module_mm <= 0:
        raise ValueError("--module-mm must be > 0")
    if args.hatch_pitch_mm <= 0:
        raise ValueError("--hatch-pitch-mm must be > 0")
    if args.draw_feed <= 0:
        raise ValueError("--draw-feed must be > 0")
    if args.travel_feed <= 0:
        raise ValueError("--travel-feed must be > 0")
    if args.version is not None and not 1 <= args.version <= 40:
        raise ValueError("--version must be between 1 and 40")


def generate_qr_matrix(
    text: str,
    error_correction: str = DEFAULT_ERROR_CORRECTION,
    version: int | None = None,
) -> list[list[bool]]:
    qr = qrcode.QRCode(
        version=version,
        error_correction=ERROR_CORRECTION_LEVELS[error_correction],
        box_size=1,
        border=QUIET_ZONE_MODULES,
    )
    qr.add_data(text)
    qr.make(fit=version is None)
    return [[bool(cell) for cell in row] for row in qr.get_matrix()]


def find_horizontal_runs(matrix: Sequence[Sequence[bool]]) -> list[RectModules]:
    rects: list[RectModules] = []
    for y, row in enumerate(matrix):
        x = 0
        while x < len(row):
            if not row[x]:
                x += 1
                continue
            start_x = x
            while x < len(row) and row[x]:
                x += 1
            rects.append(RectModules(x=start_x, y=y, width=x - start_x))
    return rects


def module_rect_to_mm(
    rect: RectModules,
    origin_x_mm: float,
    origin_y_mm: float,
    module_mm: float,
) -> RectMm:
    return RectMm(
        x0_mm=origin_x_mm + rect.x * module_mm,
        y0_mm=origin_y_mm + rect.y * module_mm,
        x1_mm=origin_x_mm + (rect.x + rect.width) * module_mm,
        y1_mm=origin_y_mm + (rect.y + rect.height) * module_mm,
    )


def outline_path(rect: RectMm) -> StrokePath:
    return StrokePath(
        points_mm=(
            (rect.x0_mm, rect.y0_mm),
            (rect.x1_mm, rect.y0_mm),
            (rect.x1_mm, rect.y1_mm),
            (rect.x0_mm, rect.y1_mm),
            (rect.x0_mm, rect.y0_mm),
        ),
        kind="outline",
    )


def zigzag_hatch_path(rect: RectMm, hatch_pitch_mm: float) -> StrokePath | None:
    segments: list[tuple[tuple[float, float], tuple[float, float]]] = []
    c_min = rect.y0_mm - rect.x1_mm
    c_max = rect.y1_mm - rect.x0_mm
    c_step = hatch_pitch_mm * math.sqrt(2.0)
    c = c_min + c_step
    while c < c_max:
        x_start = max(rect.x0_mm, rect.y0_mm - c)
        x_end = min(rect.x1_mm, rect.y1_mm - c)
        if x_end - x_start > 0.01:
            segments.append(((x_start, x_start + c), (x_end, x_end + c)))
        c += c_step

    if not segments:
        return None

    points: list[tuple[float, float]] = []
    for index, (start, end) in enumerate(segments):
        segment_points = (start, end) if index % 2 == 0 else (end, start)
        if not points:
            points.extend(segment_points)
        else:
            points.append(segment_points[0])
            points.append(segment_points[1])
    return StrokePath(points_mm=tuple(points), kind="zigzag hatch")


def build_stroke_paths(
    rects: Iterable[RectModules],
    origin_x_mm: float,
    origin_y_mm: float,
    module_mm: float,
    hatch_pitch_mm: float,
) -> list[StrokePath]:
    paths: list[StrokePath] = []
    for rect in rects:
        rect_mm = module_rect_to_mm(rect, origin_x_mm, origin_y_mm, module_mm)
        paths.append(outline_path(rect_mm))
        hatch_path = zigzag_hatch_path(rect_mm, hatch_pitch_mm)
        if hatch_path is not None:
            paths.append(hatch_path)
    return paths


def format_mm(value: float) -> str:
    return f"{value:.{POSITION_DECIMALS}f}".rstrip("0").rstrip(".")


def format_feed(value: float) -> str:
    return f"{value:.3f}".rstrip("0").rstrip(".")


def xy_command(x_mm: float, y_mm: float, feed_mm_min: float) -> str:
    return f"XY {format_mm(x_mm)} {format_mm(y_mm)} {format_feed(feed_mm_min)}"


def build_csv_rows(
    paths: Sequence[StrokePath],
    draw_feed_mm_min: float,
    travel_feed_mm_min: float,
) -> list[CsvRow]:
    rows: list[CsvRow] = [
        CsvRow(command, 0, "", comment) for command, comment in PREAMBLE_ROWS
    ]
    rows.append(
        CsvRow("PENUP", 250, "PENUP", "ensure pen is raised before QR travel")
    )
    for index, path in enumerate(paths, start=1):
        start_x_mm, start_y_mm = path.points_mm[0]
        rows.append(
            CsvRow(
                xy_command(start_x_mm, start_y_mm, travel_feed_mm_min),
                0,
                XY_EXPECT,
                f"travel to QR {path.kind} {index}",
            )
        )
        rows.append(CsvRow("PENDOWN", 120, "PENDOWN", f"start QR {path.kind} {index}"))
        for point_index, (x_mm, y_mm) in enumerate(path.points_mm[1:], start=1):
            rows.append(
                CsvRow(
                    xy_command(x_mm, y_mm, draw_feed_mm_min),
                    0,
                    XY_EXPECT,
                    f"draw QR {path.kind} {index}.{point_index}",
                )
            )
        rows.append(CsvRow("PENUP", 120, "PENUP", f"finish QR {path.kind} {index}"))
    rows.append(CsvRow("PENUP", 250, "PENUP", "final pen-up"))
    return rows


def write_csv(rows: Iterable[CsvRow], output_path: Path) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    with output_path.open("w", newline="", encoding="utf-8") as csv_file:
        writer = csv.DictWriter(csv_file, fieldnames=CSV_HEADER)
        writer.writeheader()
        for row in rows:
            writer.writerow(
                {
                    "command": row.command,
                    "delay_ms": row.delay_ms,
                    "expect": row.expect,
                    "comment": row.comment,
                }
            )


def write_preview_svg(
    paths: Sequence[StrokePath],
    matrix_size_modules: int,
    origin_x_mm: float,
    origin_y_mm: float,
    module_mm: float,
    output_path: Path,
    label: str,
) -> None:
    output_path.parent.mkdir(parents=True, exist_ok=True)
    width_mm = matrix_size_modules * module_mm
    height_mm = matrix_size_modules * module_mm
    min_x = origin_x_mm
    min_y = origin_y_mm
    max_x = origin_x_mm + width_mm
    max_y = origin_y_mm + height_mm

    with output_path.open("w", encoding="utf-8") as svg_file:
        svg_file.write(
            f'<svg xmlns="http://www.w3.org/2000/svg" '
            f'width="{format_mm(width_mm)}mm" height="{format_mm(height_mm)}mm" '
            f'viewBox="{format_mm(min_x)} {format_mm(min_y)} '
            f'{format_mm(width_mm)} {format_mm(height_mm)}">\n'
        )
        svg_file.write(f"  <title>{escape(label)}</title>\n")
        svg_file.write('  <rect width="100%" height="100%" fill="white"/>\n')
        svg_file.write(
            f'  <rect x="{format_mm(min_x)}" y="{format_mm(min_y)}" '
            f'width="{format_mm(max_x - min_x)}" height="{format_mm(max_y - min_y)}" '
            'fill="none" stroke="#d0d0d0" stroke-width="0.05"/>\n'
        )
        svg_file.write('  <g fill="none" stroke="black" stroke-width="0.18" stroke-linecap="round" stroke-linejoin="round">\n')
        for path in paths:
            points = " ".join(
                f"{format_mm(x_mm)},{format_mm(y_mm)}"
                for x_mm, y_mm in path.points_mm
            )
            svg_file.write(
                f'    <polyline points="{points}" data-kind="{escape(path.kind)}"/>\n'
            )
        svg_file.write("  </g>\n")
        svg_file.write("</svg>\n")


def generate_plot_data(args: argparse.Namespace) -> tuple[list[CsvRow], list[StrokePath], int]:
    matrix = generate_qr_matrix(
        text=args.text,
        error_correction=args.error_correction,
        version=args.version,
    )
    rects = find_horizontal_runs(matrix)
    paths = build_stroke_paths(
        rects=rects,
        origin_x_mm=args.origin_x,
        origin_y_mm=args.origin_y,
        module_mm=args.module_mm,
        hatch_pitch_mm=args.hatch_pitch_mm,
    )
    rows = build_csv_rows(
        paths=paths,
        draw_feed_mm_min=args.draw_feed,
        travel_feed_mm_min=args.travel_feed,
    )
    return rows, paths, len(matrix)


def main() -> int:
    args = parse_args()
    try:
        validate_args(args)
        rows, paths, matrix_size_modules = generate_plot_data(args)
        write_csv(rows, args.output)
        if args.preview_svg is not None:
            write_preview_svg(
                paths=paths,
                matrix_size_modules=matrix_size_modules,
                origin_x_mm=args.origin_x,
                origin_y_mm=args.origin_y,
                module_mm=args.module_mm,
                output_path=args.preview_svg,
                label=f"QR preview for {args.text}",
            )
    except Exception as exc:
        print(f"ERROR: {exc}")
        return 1

    print(
        f"Wrote {args.output} with {len(rows)} rows and {len(paths)} stroke paths "
        f"({matrix_size_modules}x{matrix_size_modules} modules)."
    )
    if args.preview_svg is not None:
        print(f"Wrote {args.preview_svg}.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
