# G-code Check

Phase 7の最小G-code parser/interpreterを確認する手順です。

## CSV

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-0001 \
  --csv tools/serial_tool/examples/gcode_check.csv \
  --startup-delay 4 \
  --echo
```

`--queue-mode`を使う場合も、`G28`は`HOME`と同じく完了ログまで待ちます。

## Expected

| Command | Expected log |
|---|---|
| `G28` | `HOME complete` |
| `G21` | `GCODE units=MM` |
| `G90` | `GCODE distance=ABSOLUTE` |
| `G1 X10 Y10 F600` | `GCODE G1 -> XY ...` and `ACK_XY target=(10.000,10.000)` |
| `G91` | `GCODE distance=RELATIVE` |
| `G20` | `GCODE units=INCH X/Y converted to mm; F remains mm/min` |
| `M3` | `PEN DOWN` |
| `M5` | `PEN UP` |
| `M114` | `POS ...` |

`G0` and `G1` use the existing `XY` motion path, so soft limits, homing checks, planner, trapezoid planning, look-ahead, timed segments, and hard-limit handling remain in the same safety path as serial `XY`.

## Notes

- `G20` changes only X/Y coordinate units. `F` is still interpreted as mm/min.
- Arc moves, extrusion, Z, checksums beyond line truncation at `*`, and modal motion without an explicit `G0`/`G1` are not implemented.
- One G/M command per line is supported.
