# AGENTS.md

Scope: This file applies to `tools/serial_tool` and its descendants.

This directory contains host-side Python tools for sending firmware commands over USB Serial. Keep this tool aligned with the firmware contract documented at the repository root.

## Required References

Before changing behavior, inspect the relevant root-level firmware documents:

- `../../AGENTS.md`
- `../../SPEC.md`
- `../../PLANS.md`
- `../../README.md`
- `../../include/PlotterConfig.h`

## Boundaries

- Use a repository-root venv by default: `.venv`.
- Keep dependencies in `requirements.txt`.
- Do not implement CoreXY A/B kinematics in this tool.
- Do not implement motion planning, look-ahead, junction deviation, or soft-limit policy in this tool.
- Do not bypass firmware command parsing with any binary or private protocol unless the firmware contract is updated first.
- Treat each CSV `command` as one firmware Serial line.

## CSV Contract

The stable input format is:

- `command`: required firmware command line.
- `delay_ms`: optional delay after sending.
- `expect`: optional substring expected in the serial response.
- `comment`: optional human-readable note.

Future drawing-data generators should emit this CSV contract instead of coupling directly to pyserial.

## Extension Policy

Separate responsibilities:

- CSV sending stays in `serial_send.py`.
- Drawing, SVG, image, or path conversion should be added as separate modules or scripts.
- Generated drawing data should use firmware-space commands such as `PENUP`, `PENDOWN`, and `XY <x_mm> <y_mm>`. Use `XY <x_mm> <y_mm> <feed_mm_min>` only when the CSV is intentionally testing or overriding feed.

Keep host-side tools conservative. The firmware owns machine state, safety checks, CoreXY conversion, and motion execution.
