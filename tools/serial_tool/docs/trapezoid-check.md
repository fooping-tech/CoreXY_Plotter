# Trapezoid Check

Phase 8の台形加減速計画と、Phase 9のtimed segment生成までをSerialログで確認する手順です。

## CSV

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-0001 \
  --csv tools/serial_tool/examples/trapezoid_check.csv \
  --startup-delay 4 \
  --echo
```

## Expected

| Command | Expected log |
|---|---|
| `CONFIG` | `accel=37.500` |
| `ALARM_CLEAR` | `ALARM_CLEAR complete` |
| `LIMIT_STATUS` | Record whether X/Y limit starts ON or OFF |
| `HOME` | `HOME complete` |
| `XY 0.5 0 600` | `TRAPEZOID profile=TRIANGULAR` and `SEGMENTS count=` |
| `XY 50 0 600` | `TRAPEZOID profile=TRAPEZOID` and `SEGMENTS count=` |

`TrapezoidPlanner`は`MotionBlock`へacceleration、cruise、decelerationの距離と時間を保持します。
その後、`SegmentGenerator`がDDAでtimed segmentを作り、`StepperBackendFastAccel`がFastAccelStepper `moveTimed()`へ投入します。

このCSVの`expect`列はprofile種別の確認を優先しているため、`SEGMENTS count=`は`--echo`の受信ログで確認してください。
timed segment生成そのものを主目的に確認する場合は、[Timed Segment Check](timed-segment-check.md)を使います。

`HOME`は対象axisのlimitが最初からONでも開始できます。
開始時ONの場合は`HOMING_START_BACKOFF_MM`まで逃げて、limitがOFFになってからslow seekへ進みます。

`HOME`が`homing backoff limit still on`で止まる場合は、直前の`LIMIT_STATUS`でlimit入力を確認してください。
開始時ONから`HOMING_START_BACKOFF_MM`逃げてもOFFにならない場合は、limit switchが押されたまま、入力がGNDへ短絡している、配線が逆、または`LIMIT_ACTIVE_LOW`の極性が合っていない可能性があります。
