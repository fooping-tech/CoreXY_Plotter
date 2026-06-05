# Timed Segment Check

Phase 9のDDA timed segment生成とFastAccelStepper `moveTimed()`投入を確認する手順です。

## CSV

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-0001 \
  --csv tools/serial_tool/examples/timed_segment_check.csv \
  --startup-delay 4 \
  --echo
```

## Expected

| Command | Expected log |
|---|---|
| `XY 10 0 600` | `TRAPEZOID ...` followed by `SEGMENTS count=` and `ACK_XY` |
| `XY 10 10 600` | `SEGMENTS count=` and `ACK_XY` |
| `XY 40 40 1200` | `SEGMENTS count=` and `ACK_XY` |

`SegmentGenerator` converts the planned `MotionBlock` into short timed A/B segments using cumulative DDA rounding.
`StepperBackendFastAccel` submits each segment through FastAccelStepper `moveTimed()`.

At this phase, look-ahead and junction deviation are still not implemented.
