#!/usr/bin/env python3
"""Convert KST32B CSF/1 stroke font text into plotter G-code."""

from __future__ import annotations

import argparse
import re
import sys
from dataclasses import dataclass
from pathlib import Path


GRID_WIDTH = 30
GRID_HEIGHT = 32


Point = tuple[float, float]
Stroke = list[Point]


@dataclass(frozen=True)
class Glyph:
    strokes: list[Stroke]
    advance_units: int


def x_from_move(byte: int) -> int | None:
    if 0x21 <= byte <= 0x26:
        return byte - 0x21
    if 0x28 <= byte <= 0x3F:
        return byte - 0x28 + 6
    return None


def x_from_draw(byte: int) -> int | None:
    if 0x40 <= byte <= 0x5B:
        return byte - 0x40
    if 0x5E <= byte <= 0x5F:
        return byte - 0x5E + 28
    return None


def x_from_next(byte: int) -> int | None:
    if 0x60 <= byte <= 0x7D:
        return byte - 0x60
    return None


def y_from_move(byte: int) -> int | None:
    if byte == 0x7E:
        return 0
    if 0xA1 <= byte <= 0xBF:
        return byte - 0xA1 + 1
    return None


def y_from_draw(byte: int) -> int | None:
    if 0xC0 <= byte <= 0xDF:
        return byte - 0xC0
    return None


def decode_csf1(payload: bytes, advance_units: int) -> Glyph:
    """Decode one KST32B CSF/1 glyph record.

    CSF/1 stores X and Y updates separately. A "next X" byte prepares the
    target X for the following Y draw, which is how diagonal strokes stay
    compact without inserting an intermediate horizontal segment.
    """

    current_x = 0
    current_y = 0
    target_x = 0
    target_y = 0
    strokes: list[Stroke] = []
    current_stroke: Stroke | None = None

    def move_to(x: int, y: int) -> None:
        nonlocal current_x, current_y, target_x, target_y, current_stroke
        current_x = target_x = x
        current_y = target_y = y
        current_stroke = None

    def draw_to(x: int, y: int) -> None:
        nonlocal current_x, current_y, target_x, target_y, current_stroke
        if current_stroke is None:
            current_stroke = [(float(current_x), float(current_y))]
            strokes.append(current_stroke)
        if current_x != x or current_y != y:
            current_stroke.append((float(x), float(y)))
        current_x = target_x = x
        current_y = target_y = y

    for byte in payload:
        if byte == 0x20:
            break

        value = x_from_move(byte)
        if value is not None:
            target_x = value
            current_stroke = None
            continue

        value = y_from_move(byte)
        if value is not None:
            target_y = value
            move_to(target_x, target_y)
            continue

        value = x_from_next(byte)
        if value is not None:
            target_x = value
            continue

        value = x_from_draw(byte)
        if value is not None:
            target_x = value
            draw_to(target_x, target_y)
            continue

        value = y_from_draw(byte)
        if value is not None:
            target_y = value
            draw_to(target_x, target_y)
            continue

        # 0x27, 0x5c, and 0x5d are reserved. Any other byte is also outside
        # CSF/1's command alphabet, so treat it as a record terminator.
        break

    return Glyph(strokes=[stroke for stroke in strokes if len(stroke) >= 2], advance_units=advance_units)


def glyph_advance_units(code: int, strokes: list[Stroke]) -> int:
    if 0x0020 <= code <= 0x00DF:
        return 15
    if not strokes:
        return GRID_WIDTH
    max_x = max(point[0] for stroke in strokes for point in stroke)
    return max(GRID_WIDTH, int(max_x) + 1)


def load_kst32b(path: Path) -> dict[int, Glyph]:
    glyphs: dict[int, Glyph] = {}
    for raw_line in path.read_bytes().splitlines():
        stripped = raw_line.lstrip()
        if not stripped or stripped.startswith(b"*"):
            continue
        if len(stripped) < 6:
            continue
        try:
            code = int(stripped[:4].decode("ascii"), 16)
        except (UnicodeDecodeError, ValueError):
            continue
        if len(stripped) > 4 and stripped[4:5] not in (b" ", b"\t"):
            continue

        payload = stripped[5:].rstrip(b"\r\n")
        glyph = decode_csf1(payload, GRID_WIDTH)
        glyphs[code] = Glyph(glyph.strokes, glyph_advance_units(code, glyph.strokes))
    return glyphs


def char_to_kst_code(char: str) -> int | None:
    codepoint = ord(char)
    if 0x20 <= codepoint <= 0x7E:
        return codepoint

    try:
        encoded = char.encode("cp932")
    except UnicodeEncodeError:
        encoded = b""
    if len(encoded) == 1 and 0xA1 <= encoded[0] <= 0xDF:
        return encoded[0]

    try:
        iso2022 = char.encode("iso2022_jp")
    except UnicodeEncodeError:
        return None

    marker = b"\x1b$B"
    start = iso2022.find(marker)
    if start < 0:
        return None
    start += len(marker)
    if len(iso2022) < start + 2:
        return None
    first, second = iso2022[start], iso2022[start + 1]
    if 0x21 <= first <= 0x7E and 0x21 <= second <= 0x7E:
        return (first << 8) | second
    return None


def fallback_box() -> Glyph:
    stroke = [
        (0.0, 0.0),
        (float(GRID_WIDTH), 0.0),
        (float(GRID_WIDTH), float(GRID_HEIGHT)),
        (0.0, float(GRID_HEIGHT)),
        (0.0, 0.0),
    ]
    return Glyph([stroke], GRID_WIDTH)


def transform_point(
    point: Point,
    origin_x_mm: float,
    origin_y_mm: float,
    scale: float,
    flip_y: bool,
) -> Point:
    x_units, y_units = point
    if flip_y:
        y_units = GRID_HEIGHT - y_units
    return origin_x_mm + x_units * scale, origin_y_mm + y_units * scale


def fmt_coord(value: float) -> str:
    return f"{value:.3f}"


def same_point(a: Point | None, b: Point, tolerance_mm: float = 0.0005) -> bool:
    if a is None:
        return False
    return abs(a[0] - b[0]) <= tolerance_mm and abs(a[1] - b[1]) <= tolerance_mm


def text_to_gcode(
    glyphs: dict[int, Glyph],
    text: str,
    start_x_mm: float,
    start_y_mm: float,
    size_mm: float,
    char_spacing_mm: float,
    line_spacing_mm: float,
    feed_mm_min: float,
    rapid_feed_mm_min: float,
    dwell_ms: int,
    flip_y: bool,
    missing_glyph: str,
) -> list[str]:
    scale = size_mm / GRID_HEIGHT
    x_mm = start_x_mm
    y_mm = start_y_mm
    current_position_mm: Point | None = None
    display_text = text.replace("\n", "\\n")
    lines = [
        "G21",
        "G90",
        "M5",
        "",
        f"; text: {display_text}",
    ]

    box = fallback_box()

    for char in text:
        if char == "\r":
            continue
        if char == "\n":
            x_mm = start_x_mm
            y_mm += size_mm + line_spacing_mm
            lines.append("")
            lines.append("; newline")
            continue
        if char == "\t":
            x_mm += (GRID_WIDTH * scale + char_spacing_mm) * 4
            continue
        if char == " ":
            x_mm += (15 * scale) + char_spacing_mm
            continue

        code = char_to_kst_code(char)
        glyph = glyphs.get(code) if code is not None else None
        if glyph is None:
            message = f"warning: missing glyph for {char!r}"
            if code is not None:
                message += f" (KST 0x{code:04X})"
            print(message, file=sys.stderr)
            if missing_glyph == "skip":
                x_mm += GRID_WIDTH * scale + char_spacing_mm
                continue
            glyph = box
            lines.append(f"; warning: missing glyph for {char}")

        lines.append(f"; char: {char}")
        for stroke in glyph.strokes:
            if len(stroke) < 2:
                continue
            first_x, first_y = transform_point(stroke[0], x_mm, y_mm, scale, flip_y)
            first_point_mm = (first_x, first_y)
            if not same_point(current_position_mm, first_point_mm):
                lines.append(f"G0 X{fmt_coord(first_x)} Y{fmt_coord(first_y)} F{rapid_feed_mm_min:g}")
                current_position_mm = first_point_mm
            lines.append("M3")
            if dwell_ms > 0:
                lines.append(f"G4 P{dwell_ms}")
            for point in stroke[1:]:
                draw_x, draw_y = transform_point(point, x_mm, y_mm, scale, flip_y)
                lines.append(f"G1 X{fmt_coord(draw_x)} Y{fmt_coord(draw_y)} F{feed_mm_min:g}")
                current_position_mm = (draw_x, draw_y)
            lines.append("M5")
            if dwell_ms > 0:
                lines.append(f"G4 P{dwell_ms}")
        x_mm += glyph.advance_units * scale + char_spacing_mm

    lines.append("")
    lines.append("M5")
    return lines


GCODE_X_RE = re.compile(r"(?:^|\s)X([-+]?\d+(?:\.\d*)?)")
GCODE_Y_RE = re.compile(r"(?:^|\s)Y([-+]?\d+(?:\.\d*)?)")


def gcode_bounds(lines: list[str]) -> tuple[float, float, float, float] | None:
    min_x: float | None = None
    max_x: float | None = None
    min_y: float | None = None
    max_y: float | None = None
    for line in lines:
        if not (line.startswith("G0 ") or line.startswith("G1 ")):
            continue
        x_match = GCODE_X_RE.search(line)
        y_match = GCODE_Y_RE.search(line)
        if x_match is None or y_match is None:
            continue
        x_value = float(x_match.group(1))
        y_value = float(y_match.group(1))
        min_x = x_value if min_x is None else min(min_x, x_value)
        max_x = x_value if max_x is None else max(max_x, x_value)
        min_y = y_value if min_y is None else min(min_y, y_value)
        max_y = y_value if max_y is None else max(max_y, y_value)
    if min_x is None or max_x is None or min_y is None or max_y is None:
        return None
    return min_x, max_x, min_y, max_y


def bounds_fit(
    bounds: tuple[float, float, float, float] | None,
    max_x_mm: float | None,
    max_y_mm: float | None,
) -> bool:
    if bounds is None:
        return True
    _, max_x, _, max_y = bounds
    if max_x_mm is not None and max_x > max_x_mm:
        return False
    if max_y_mm is not None and max_y > max_y_mm:
        return False
    return True


def bounds_error(
    bounds: tuple[float, float, float, float] | None,
    max_x_mm: float | None,
    max_y_mm: float | None,
) -> str:
    if bounds is None:
        return "no drawable coordinates were generated"
    min_x, max_x, min_y, max_y = bounds
    details = [f"bounds X=[{min_x:.3f},{max_x:.3f}] Y=[{min_y:.3f},{max_y:.3f}]"]
    if max_x_mm is not None:
        details.append(f"max_x={max_x_mm:.3f}")
    if max_y_mm is not None:
        details.append(f"max_y={max_y_mm:.3f}")
    return " ".join(details)


def generate_lines(
    glyphs: dict[int, Glyph],
    text: str,
    args: argparse.Namespace,
    size_mm: float,
) -> list[str]:
    return text_to_gcode(
        glyphs=glyphs,
        text=text,
        start_x_mm=args.x,
        start_y_mm=args.y,
        size_mm=size_mm,
        char_spacing_mm=args.char_spacing,
        line_spacing_mm=args.line_spacing,
        feed_mm_min=args.feed,
        rapid_feed_mm_min=args.rapid_feed,
        dwell_ms=args.dwell_ms,
        flip_y=args.flip_y,
        missing_glyph=args.missing_glyph,
    )


def generate_fitting_lines(
    glyphs: dict[int, Glyph],
    text: str,
    args: argparse.Namespace,
) -> tuple[list[str], float]:
    lines = generate_lines(glyphs, text, args, args.size)
    bounds = gcode_bounds(lines)
    if bounds_fit(bounds, args.max_x, args.max_y):
        return lines, args.size
    if not args.auto_scale_to_fit:
        raise ValueError(
            "generated G-code exceeds configured bounds: "
            f"{bounds_error(bounds, args.max_x, args.max_y)}. "
            "Use --auto-scale-to-fit or reduce --size/--char-spacing."
        )

    low = 0.1
    high = args.size
    best_lines: list[str] | None = None
    best_size = low
    for _ in range(28):
        midpoint = (low + high) / 2.0
        candidate = generate_lines(glyphs, text, args, midpoint)
        if bounds_fit(gcode_bounds(candidate), args.max_x, args.max_y):
            best_lines = candidate
            best_size = midpoint
            low = midpoint
        else:
            high = midpoint

    if best_lines is None:
        raise ValueError(
            "could not fit generated G-code into configured bounds: "
            f"{bounds_error(bounds, args.max_x, args.max_y)}"
        )
    print(
        f"info: auto-scaled --size {args.size:.3f} -> {best_size:.3f} mm",
        file=sys.stderr,
    )
    return best_lines, best_size


def read_input_text(args: argparse.Namespace) -> str:
    if args.text is not None:
        return args.text
    return Path(args.input_file).read_text(encoding="utf-8").rstrip("\r\n")


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Convert Japanese text to plotter G-code using KST32B stroke font data.",
    )
    parser.add_argument("--font", required=True, help="Path to KST32B.TXT")
    source = parser.add_mutually_exclusive_group(required=True)
    source.add_argument("--text", help="Japanese text to convert")
    source.add_argument("--input-file", help="UTF-8 text file to convert")
    parser.add_argument("--x", type=float, default=10.0, help="Start X coordinate in mm")
    parser.add_argument("--y", type=float, default=10.0, help="Start Y coordinate in mm")
    parser.add_argument("--size", type=float, default=20.0, help="Character height in mm")
    parser.add_argument("--char-spacing", type=float, default=3.0, help="Character spacing in mm")
    parser.add_argument("--line-spacing", type=float, default=6.0, help="Line spacing in mm")
    parser.add_argument("--feed", type=float, default=3000.0, help="Draw feed in mm/min")
    parser.add_argument("--rapid-feed", type=float, default=8000.0, help="Pen-up feed in mm/min")
    parser.add_argument("--dwell-ms", type=int, default=80, help="Dwell after pen up/down in ms")
    parser.add_argument("--flip-y", action="store_true", help="Flip glyph Y axis")
    parser.add_argument("--max-x", type=float, help="Reject or auto-scale output above this X mm")
    parser.add_argument("--max-y", type=float, help="Reject or auto-scale output above this Y mm")
    parser.add_argument(
        "--auto-scale-to-fit",
        action="store_true",
        help="Reduce --size until generated coordinates fit --max-x/--max-y",
    )
    parser.add_argument(
        "--missing-glyph",
        choices=("box", "skip"),
        default="box",
        help="How to handle missing glyphs",
    )
    parser.add_argument("-o", "--output", required=True, help="Output G-code file path")
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    font_path = Path(args.font)
    if not font_path.exists():
        print(f"error: font file not found: {font_path}", file=sys.stderr)
        return 2

    if args.size <= 0:
        print("error: --size must be positive", file=sys.stderr)
        return 2
    if args.dwell_ms < 0:
        print("error: --dwell-ms must be zero or positive", file=sys.stderr)
        return 2

    glyphs = load_kst32b(font_path)
    if not glyphs:
        print(f"error: no glyph records were decoded from {font_path}", file=sys.stderr)
        return 2

    text = read_input_text(args)
    if args.auto_scale_to_fit and args.max_x is None and args.max_y is None:
        print("error: --auto-scale-to-fit requires --max-x and/or --max-y", file=sys.stderr)
        return 2

    try:
        lines, _ = generate_fitting_lines(glyphs, text, args)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 2

    output_path = Path(args.output)
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text("\n".join(lines) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
