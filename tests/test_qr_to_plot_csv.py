from __future__ import annotations

import argparse
import csv
import sys
from pathlib import Path


REPO_ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(REPO_ROOT / "tools" / "qr_tool"))

import qr_to_plot_csv  # noqa: E402


def make_args(tmp_path: Path) -> argparse.Namespace:
    return argparse.Namespace(
        text="HELLO",
        output=tmp_path / "qr.csv",
        preview_svg=tmp_path / "qr.svg",
        origin_x=0.0,
        origin_y=0.0,
        module_mm=1.0,
        hatch_pitch_mm=0.5,
        draw_feed=600.0,
        travel_feed=1800.0,
        error_correction="M",
        version=1,
    )


def test_short_text_generates_csv_with_plotter_commands(tmp_path: Path) -> None:
    args = make_args(tmp_path)
    rows, paths, matrix_size_modules = qr_to_plot_csv.generate_plot_data(args)
    qr_to_plot_csv.write_csv(rows, args.output)

    with args.output.open(newline="", encoding="utf-8") as csv_file:
        csv_rows = list(csv.DictReader(csv_file))
    commands = [row["command"] for row in csv_rows]

    assert matrix_size_modules > 0
    assert paths
    assert any(path.kind == "outline" for path in paths)
    assert any(path.kind == "zigzag hatch" for path in paths)
    assert all(
        len(path.points_mm) > 2 for path in paths if path.kind == "zigzag hatch"
    )
    assert commands[:10] == [
        "CONFIG",
        "SELFTEST",
        "TMC_INIT",
        "TMC_STATUS",
        "PENUP",
        "ZERO",
        "ALARM_CLEAR",
        "LIMIT_STATUS",
        "HOME",
        "POS",
    ]
    assert commands[-1] == "PENUP"
    assert "PENDOWN" in commands
    assert any(command.startswith("XY ") for command in commands)
    assert all(
        row["expect"] == qr_to_plot_csv.XY_EXPECT
        for row in csv_rows
        if row["command"].startswith("XY ")
    )


def test_preview_svg_contains_generated_hatch_lines(tmp_path: Path) -> None:
    args = make_args(tmp_path)
    rows, paths, matrix_size_modules = qr_to_plot_csv.generate_plot_data(args)
    assert rows

    qr_to_plot_csv.write_preview_svg(
        paths=paths,
        matrix_size_modules=matrix_size_modules,
        origin_x_mm=args.origin_x,
        origin_y_mm=args.origin_y,
        module_mm=args.module_mm,
        output_path=args.preview_svg,
        label="test",
    )

    svg_text = args.preview_svg.read_text(encoding="utf-8")
    assert "<svg" in svg_text
    assert "<polyline " in svg_text
    assert 'data-kind="outline"' in svg_text
    assert 'data-kind="zigzag hatch"' in svg_text
