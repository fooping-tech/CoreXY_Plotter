# Host WebUI Wireframe

> 位置づけ: 本書はWebUI設計時のワイヤーフレーム経緯資料である。
> 画面構成・操作ルールの仕様の正は`SPEC.md` §20とし、差異がある場合はSPECが優先する。

## Design Direction

The WebUI is a PC/Raspberry Pi hosted control surface for the CoreXY plotter.
The visual direction is dark, compact, machine-status-first, and operator-focused.

The UI should feel closer to a machine control panel than a marketing dashboard.
Avoid decorative hero sections, oversized cards, and explanatory onboarding text.

## Layout Shell

Desktop-first MVP, responsive down to tablet width.

```text
┌────────────────────────────────────────────────────────────────────┐
│ Top Bar                                                            │
│ [CoreXY Plotter] [READY] [Port: /dev/cu...] [JOB_ABORT] [Settings] │
├───────────────┬────────────────────────────────────────────────────┤
│ Nav           │ Page Content                                       │
│ Dashboard     │                                                    │
│ Control       │                                                    │
│ Job           │                                                    │
│ Console       │                                                    │
│ Settings      │                                                    │
└───────────────┴────────────────────────────────────────────────────┘
```

Target viewport:

- Minimum desktop width: 1024 px
- Preferred desktop width: 1280 px
- Minimum useful tablet width: 768 px
- Header height: 56 px
- Sidebar width: 168 px
- Content padding: 16 px
- Panel radius: 8 px maximum

## Color Tokens

```text
bg.canvas      #090B0F
bg.panel       #141820
bg.panelAlt    #1C222C
border.subtle  #2A313D
text.primary   #F4F7FB
text.secondary #A7B0BE
text.muted     #6F7A88
accent.cyan    #36D9E8
state.ready    #38D27A
state.warning  #F6B73C
state.alarm    #F05252
state.idle     #5E6AD2
disabled.bg    #242A33
disabled.text  #687282
preview.travel #5F6B7A
preview.draw   #36D9E8
preview.limit  #F6B73C
preview.error  #F05252
```

State mapping:

| State | Color | Meaning |
|---|---|---|
| `READY` | `state.ready` | Homed, no alarm, usable |
| `ALARM` | `state.alarm` | Motion-producing controls disabled |
| `NEED HOME` | `state.warning` | Home required before jog/job |
| `HOMING` | `state.warning` | Busy; most controls disabled |
| Unknown / disconnected | `text.muted` | No trusted machine state |

## Typography

- UI font: system sans-serif
- Numeric readouts: tabular numerals
- Header labels: 12 px
- Body text: 14 px
- Compact table/log text: 12 px
- Main machine state: 28-36 px
- X/Y position: 28 px

Do not scale font size with viewport width.

## Top Bar

```text
┌────────────────────────────────────────────────────────────────────┐
│ CoreXY Plotter  [READY]  X 12.3  Y 8.0  Pen UP  Port connected     │
│                                           [JOB_ABORT] [⚙]          │
└────────────────────────────────────────────────────────────────────┘
```

Contents:

- Product name
- Machine state pill
- X/Y compact readout
- Pen state
- Serial connection status
- `JOB_ABORT` button
- Settings icon button

Rules:

- `JOB_ABORT` is visible in the top bar whenever a job is active.
- `JOB_ABORT` uses alarm red styling.
- If no job is active, hide or disable `JOB_ABORT` but keep layout stable.
- Unknown serial state shows `Disconnected` and disables motion controls.

## Sidebar Navigation

```text
Dashboard
Control
Job
Console
Settings
```

Rules:

- Active item uses cyan left rail and brighter text.
- No badges except error count on Console.
- Navigation should not shift when state changes.

## Dashboard Page

Purpose: at-a-glance status.

```text
┌──────────────────────────────┬──────────────────────────────┐
│ MACHINE STATE                │ POSITION                     │
│ READY                        │ X 12.30 mm                   │
│ Homed, no alarm              │ Y  8.00 mm                   │
└──────────────────────────────┴──────────────────────────────┘

┌────────────┬────────────┬────────────┬────────────┐
│ Pen        │ Home       │ Limits     │ TMC        │
│ UP         │ X+ Y+      │ X open     │ READY      │
│            │            │ Y open     │            │
└────────────┴────────────┴────────────┴────────────┘

┌────────────────────────────────────────────────────┐
│ Recent Log                                          │
│ 12:01:02 ACK QUEUED POS                             │
│ 12:01:03 POS X=12.30 Y=8.00 ...                     │
└────────────────────────────────────────────────────┘
```

Components:

- Machine state panel
- Position panel
- Four status tiles
- Recent log panel

Rules:

- Alarm state panel turns red.
- Need-home state panel turns amber.
- Limit active appears red even if machine is otherwise ready.

## Manual Control Page

Purpose: safe bring-up and manual positioning.

```text
┌──────────────────────┬──────────────────────┐
│ [HOME]               │ [CLEAR ALARM]         │
└──────────────────────┴──────────────────────┘

┌──────────────────────┬──────────────────────────────┐
│ Jog                  │ Pen                          │
│       [↑]            │ [PEN UP]                     │
│ [←] [1mm] [→]        │ [PEN DOWN]                   │
│       [↓]            │                              │
│ Step: [0.1] [1] [5]  │                              │
└──────────────────────┴──────────────────────────────┘

┌─────────────────────────────────────────────────────┐
│ Safety note / disabled reason                        │
└─────────────────────────────────────────────────────┘
```

Controls:

- `HOME`
- `ALARM_CLEAR`
- Jog up/down/left/right
- Jog step segmented control: `0.1`, `1`, `5` mm
- `PENUP`
- `PENDOWN`

Enable rules:

| Control | Enabled when |
|---|---|
| `HOME` | Connected and not homing |
| `ALARM_CLEAR` | Connected and alarmed |
| Jog | Connected, homed, not alarmed, not homing, no active job |
| Pen | Connected, homed, not alarmed, not homing, no active job |

Disabled reason examples:

- `Connect serial port first`
- `Home required`
- `Alarm active`
- `Job running`
- `Machine state unknown`

## Job Page

Purpose: preview, validate, and send G-code.

```text
┌────────────────────────────────────────────┬────────────────────────┐
│ Preview                                    │ Job Panel              │
│                                            │ File: text_robo.gcode  │
│   ┌────────────────────────────────────┐   │ Lines: 842             │
│   │ 55 x 55 mm soft limit              │   │ Bounds: 4,4 - 50,42    │
│   │                                    │   │ Warnings: 0            │
│   │ cyan draw paths                    │   │                        │
│   │ gray travel paths                  │   │ [SEND JOB]             │
│   │ red out-of-bounds paths            │   │ [JOB_ABORT]            │
│   └────────────────────────────────────┘   │                        │
└────────────────────────────────────────────┴────────────────────────┘

┌────────────────────────────────────────────────────────────────────┐
│ Warnings                                                           │
│ none                                                               │
└────────────────────────────────────────────────────────────────────┘
```

Preview:

- Use SVG or Canvas.
- Fit the full soft limit box by default.
- Keep aspect ratio fixed.
- Show soft limit rectangle.
- Show file bounds rectangle.
- Pen-down path: cyan solid.
- Pen-up travel: muted gray dashed.
- Out-of-bounds segment: red.
- Start marker: small green dot.
- End marker: small cyan dot.

Job panel:

- File name
- Line count
- Parsed segment count
- Bounds
- Estimated travel count
- Warning count
- Send button
- Abort button

Send behavior:

`SEND JOB` calls host bridge, which runs:

```text
tools/serial_tool/serial_send.py
  --gcode <selected_file>
  --port <selected_port>
  --baud 115200
  --queue-mode
  --stream-gcode-motion
  --job-lifecycle
```

Rules:

- `SEND JOB` disabled if disconnected.
- `SEND JOB` disabled if machine state is unknown.
- `SEND JOB` disabled if alarmed.
- `SEND JOB` disabled if not homed, unless firmware/job config explicitly supports auto-home and the UI shows that fact.
- If preview has warnings, require an explicit warning acknowledgement before enabling send.
- During job, preview remains visible and log stream updates below or in Console.

## Console Page

Purpose: diagnose serial and firmware behavior.

```text
┌────────────────────────────────────────────────────────────────────┐
│ Filters: [All] [Errors] [Sent] [Firmware]                          │
├────────────────────────────────────────────────────────────────────┤
│ 12:03:11 > JOB_BEGIN                                               │
│ 12:03:12 < ACK QUEUED JOB_BEGIN                                    │
│ 12:03:14 < JOB_BEGIN OK homed=YES                                  │
│ 12:03:15 < ERROR: ...                                              │
└────────────────────────────────────────────────────────────────────┘

┌────────────────────────────────────────────────────────────────────┐
│ [manual command input                                      ] [Send] │
└────────────────────────────────────────────────────────────────────┘
```

Log colors:

- Sent command: cyan
- ACK: green
- NACK / REJECT / ERROR / ALARM: red
- Firmware info: secondary text
- Host bridge info: muted text

Manual command:

- Intended for diagnostics.
- Disabled while a job is running except `ABORT`/`JOB_ABORT`.
- Shows warning before sending motion-producing commands if state is unknown.

## Settings Page

```text
Serial
  Port:    [select]
  Baud:    [115200]
  Connect: [Connect] [Disconnect]

Job Send
  [x] queue mode
  [x] stream G-code motion
  [x] job lifecycle
  Startup delay: [4.0 s]

Jog
  Default step: [1 mm]
```

Rules:

- Defaults match `serial_send.py`.
- Advanced options are collapsed by default.
- Settings persist locally in browser storage if implemented.

## Responsive Behavior

Tablet width:

- Sidebar collapses into top tabs.
- Job page stacks preview above job panel.
- Console remains full width.

Phone width is not a primary target for MVP.

## Empty / Loading / Error States

Disconnected:

```text
Connect a serial port to view machine state.
```

No G-code selected:

```text
Select a G-code file to preview.
```

Preview parse warning:

```text
Preview contains unsupported commands. Firmware may still reject the job.
```

Serial error:

```text
Serial port closed or unavailable.
```

## Implementation Notes

- Prefer dense, stable panels over large cards.
- Avoid nested cards.
- Use fixed dimensions for jog buttons and status tiles.
- Keep text labels short enough to fit at 1024 px width.
- Do not hide safety state behind tabs.
- The top bar state must remain visible on every page.
