# Host WebUI

PC or Raspberry Pi hosted WebUI for the CoreXY plotter.

The WebUI talks to the M5Stack Core2 over USB Serial. G-code job sending is delegated to
`tools/serial_tool/serial_send.py`; the WebUI does not reimplement queue retry, ACK waiting,
or Job Lifecycle behavior.

## Run with a virtual environment

From the repository root:

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r tools/serial_tool/requirements.txt
python tools/webui/server.py
```

Open:

```text
http://127.0.0.1:8787
```

If port `8787` is already in use, choose another port:

```bash
python tools/webui/server.py --port 8791
```

Open:

```text
http://localhost:8791
```

If another computer on the same network needs to open the WebUI, bind to all
interfaces:

```bash
python tools/webui/server.py --host 0.0.0.0 --port 8791
```

Still open it on the host machine as:

```text
http://localhost:8791
```

## Dependencies

The WebUI server uses Python standard library modules only.

Actual serial sending still uses `tools/serial_tool/serial_send.py`, so `pyserial` is required
when sending commands or jobs:

```bash
python -m pip install -r tools/serial_tool/requirements.txt
```

## Serial connection

Open the Settings page and select the M5Stack Core2 USB serial port, then press
`Connect`. On macOS the port usually looks like:

```text
/dev/cu.usbserial-023591AC
```

If the port list is empty or slow to refresh, type the port path into `Manual port`
and press `Connect`. `Connect` sets the serial target used by later commands. To
confirm real communication with the firmware, open the Console page and send:

```text
POS
```

Manual jog commands from the WebUI use `900 mm/min` by default.

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
