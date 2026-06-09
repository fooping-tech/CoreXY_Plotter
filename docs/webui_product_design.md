# Host WebUI Product Design Brief

## Scope

The initial WebUI runs on a PC or Raspberry Pi and controls the M5Stack Core2 firmware through USB Serial.
The firmware remains the source of truth for motion safety, alarms, homing, and command validation.

Job sending must reuse `tools/serial_tool/serial_send.py`.
The WebUI must not reimplement queue retry, ACK waiting, Job Lifecycle wrapping, or failure abort behavior.

## Primary Users

- Operator bringing up and testing the CoreXY plotter
- Operator sending generated G-code jobs
- Developer diagnosing firmware, serial, homing, and safety behavior

## Product Principles

- Make machine state visible before controls.
- Keep unsafe actions visually distinct.
- Disable controls when the host does not have enough state to make a safe request.
- Keep logs close to the action that produced them.
- Preview before sending, but never treat preview as a replacement for firmware safety.

## Information Architecture

### Dashboard

- Serial connection state
- Machine state: `READY`, `ALARM`, `NEED HOME`, `HOMING`
- Position: X/Y
- Pen: up/down
- Homed state
- Limit X/Y
- TMC ready
- Recent firmware log

### Manual Control

- `HOME`
- `ALARM_CLEAR`
- `PENUP`
- `PENDOWN`
- Jog: up/down/left/right
- Jog step: 0.1 mm, 1 mm, 5 mm

Rules:

- Jog and pen controls are disabled unless the host state says homed, not alarmed, and not homing.
- Job execution disables manual jog.
- Unknown state disables motion-producing controls.

### Job

- G-code file selection
- G-code preview canvas
- File bounds
- Soft limit box
- Warning list
- Send job
- Abort job

`serial_send.py` default options for job sending:

```text
--gcode <file>
--port <serial_port>
--baud 115200
--queue-mode
--stream-gcode-motion
--job-lifecycle
```

### Console

- Firmware output
- Host bridge output
- Sent command lines
- ACK/NACK/ERROR classification
- Manual command input for diagnostics

### Settings

- Serial port
- Baudrate
- Startup delay
- Queue mode
- Stream G-code motion mode
- Jog step default

## G-code Preview Requirements

Supported for MVP:

- `G0` and `G1` XY line segments
- `G20` and `G21`
- `G90` and `G91`
- `M3` and `M5`
- `G28` as a home marker
- `G4` as a dwell marker

Preview rendering:

- Pen-down path and pen-up travel use different colors.
- Soft limit rectangle is always visible.
- File bounds are shown.
- Segments outside soft limits are highlighted and listed as warnings.
- Unsupported G-code lines are listed as warnings.

## Non-goals For MVP

- Running an HTTP server on the ESP32
- Replacing firmware safety checks
- Editing G-code in the browser
- Full GRBL compatibility
- Arc interpolation preview for `G2`/`G3`
- Pause/resume
- Feed override
