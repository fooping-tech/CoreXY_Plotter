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
python -m pip install -r tools/qr_tool/requirements.txt
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

QR G-code creation on the Job page delegates to `tools/qr_tool/qr_to_plot_csv.py`, so install
the QR tool requirements when using that feature:

```bash
python -m pip install -r tools/qr_tool/requirements.txt
```

Text G-code creation delegates to `tools/text_tool/kst32b_to_gcode.py` and requires
the local KST32B font file:

```text
tools/text_tool/fonts/KST32B.TXT
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

## G-code layout

The Job page can load multiple G-code files, place them in the preview, scale
them, and choose the draw order. `SEND JOB` sends the combined G-code to the
plotter. `SAVE G-CODE` saves the same combined G-code to a timestamped
`.gcode` file. On browsers that support the File System Access API, `SAVE G-CODE`
opens the OS save dialog. Other browsers fall back to a normal file download.

Each loaded G-code item has order controls and a remove button. Use the up/down
buttons to choose draw order, or the `x` button to remove that item from the
layout.

The QR panel on the Job page creates QR hatch-fill G-code from text or a URL and
adds it to the same layout list. The generated QR can be positioned, scaled,
previewed, saved with the layout, and sent with the normal `SEND JOB` flow.
The Text Generator panel uses the same layout, preview, save, and send flow.

Manual UI check after WebUI layout changes:

```text
1. Open the Job page.
2. Confirm Select G-code files is the first Job control and has the file icon.
3. Load two G-code files, select each row, and use up/down/remove controls.
4. Confirm Send Job and Save G-code have icons and remain below the file list.
5. Open Text Generator and QR Generator, confirm Create is beside the input.
6. Generate QR and text G-code, then confirm the preview start point is on the body, not at X0 Y0.
7. Save the combined G-code and inspect that it starts with G21, G90, M5, G0 to the first draw point, then M3.
```

## Current Scope

- Dashboard
- Manual control
- G-code preview
- QR G-code creation
- Text G-code creation
- Job sending through `serial_send.py`
- Console log stream
- Serial target settings
- WebUI-editable send timeout and G-code streaming settings

## Job Send Defaults

The Job page sends G-code through:

```text
tools/serial_tool/serial_send.py
  --gcode <temporary_file>
  --port <selected_port>
  --baud 115200
  --queue-mode
  [--stream-gcode-motion]
  [--job-lifecycle]
  --timeout <job_timeout_s>
  --motion-timeout-margin <seconds>
  --queue-retry-delay-ms <milliseconds>
  --queue-retry-timeout <seconds>
```

Settings pageの`serial_send.py Defaults`で、manual command用timeout、G-code job用timeout、
motion timeout margin、queue retry、`--stream-gcode-motion`、`--job-lifecycle`、
auto motion timeoutを変更できます。保存した値は以後のmanual command、`JOB_ABORT`、
`SEND JOB`で使われます。

`--stream-gcode-motion`では`G0/G1`を先行投入しますが、`M3/M5`や`G4`は完了ログを待ちます。
長いtravel move直後の`M3/M5`で`timeout after ... waiting for 'PEN DOWN'`などが出る場合は、
前の移動が終わる前にホスト側timeoutへ到達しています。Serial Toolは推定motion時間をtimeoutへ自動で足しますが、実機が推定より大幅に遅い場合は`--motion-timeout-margin`を調整します。

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
