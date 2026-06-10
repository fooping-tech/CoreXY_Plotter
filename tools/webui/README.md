# Host WebUI

PC or Raspberry Pi hosted WebUI for the CoreXY plotter.

The WebUI talks to the M5Stack Core2 over USB Serial. G-code job sending is delegated to
`tools/serial_tool/serial_send.py`; the WebUI does not reimplement queue retry, ACK waiting,
or Job Lifecycle behavior.

## Run

From the repository root:

```bash
python tools/webui/server.py
```

Open:

```text
http://127.0.0.1:8787
```

## Dependencies

The WebUI server uses Python standard library modules only.

Actual serial sending still uses `tools/serial_tool/serial_send.py`, so `pyserial` is required
when sending commands or jobs:

```bash
python -m pip install -r tools/serial_tool/requirements.txt
```

## Current Scope

- Dashboard
- Manual control
- G-code preview
- Job sending through `serial_send.py`
- Console log stream
- Serial target settings

## Job Send Defaults

The Job page sends G-code through:

```text
tools/serial_tool/serial_send.py
  --gcode <temporary_file>
  --port <selected_port>
  --baud 115200
  --queue-mode
  --stream-gcode-motion
  --job-lifecycle
```

## Safety Model

The WebUI disables controls based on the host-visible state, but firmware remains the source of
truth. Motion-producing commands still go through firmware `CommandMessage`, `MotionTask`, and
`SafetyManager` validation.

## Preview Limits

The MVP preview supports:

- `G0` / `G1`
- `G20` / `G21`
- `G90` / `G91`
- `M3` / `M5`
- `G28` marker
- `G4` marker

Unsupported preview commands are listed as warnings. Preview warnings block `SEND JOB` until the
G-code is adjusted. This is a UI safety gate only; firmware validation still applies.
