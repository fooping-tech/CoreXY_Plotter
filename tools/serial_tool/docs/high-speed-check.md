# High-Speed Check

CoreXYの通常XY移動を、現在の`MAX_FEED_MM_MIN`上限付近で確認するチェックです。
`../1stepper_test`の速度スイープと同じ考え方で、ファームウェアの通常Serialコマンドだけを使い、速度条件をCSV行として並べます。

## Safety

- このCSVは実機を高速に動かします。E-stopまたはモータ電源をすぐ切れる状態で実行してください。
- CSV内でhomingします。開始前に原点方向の移動が安全な状態にしてください。
- 現在のsoft limitは`X/Y 0..55mm`想定です。CSVは`10..45mm`だけを使います。
- 異音、脱調、ベルト飛び、ドライバ過熱があれば即停止してください。

## CSV

- `tools/serial_tool/examples/high_speed_check.csv`: 短い確認用
- `tools/serial_tool/examples/high_speed_sweep_check.csv`: 1200から5000 mm/minまで段階確認

## Run

高速移動チェック:

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/high_speed_check.csv \
  --startup-delay 2 \
  --startup-drain 1 \
  --timeout 60 \
  --echo
```

どこまで速度を上げられるか見る場合:

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/high_speed_sweep_check.csv \
  --startup-delay 2 \
  --startup-drain 1 \
  --timeout 60 \
  --echo
```

送信予定だけを確認する場合:

```bash
python tools/serial_tool/serial_send.py \
  --csv tools/serial_tool/examples/high_speed_check.csv \
  --dry-run
```

## Expected Logs

| Step | What to confirm |
|---|---|
| `CONFIG` | `max_feed=8485.281`、soft limitがCSVの移動範囲を含む |
| `HOME` | `HOME complete` |
| `POS` | `HOMED=YES`、`ALARM=NO` |
| `XY ... 1200..5000` | `ACK_XY`が返り、X往復で脱調や異音がない |
| diagonal validation | `ACK_XY`が返り、斜め往復で脱調や異音がない |
| final `POS` | `ALARM=NO`、位置が最後のtarget付近 |

## Notes

- feedは`mm/min`です。通常の`XY <x_mm> <y_mm>`は`DEFAULT_FEED_MM_MIN`を使いますが、この高速チェックでは速度条件そのものを確認するため`XY <x_mm> <y_mm> <feed_mm_min>`で明示します。
- `HOME`行は`HOME complete`を`expect`で待ちます。`--timeout 60`はHOMEや長い移動の最大待ち時間で、起動時読み捨て時間は`--startup-drain`で別管理します。
- `5000 mm/min`は`83.333 mm/s`です。`STEPS_PER_MM=80`、CoreXY最悪条件`sqrt(2)`では片側モータ約`9428 steps/s`相当です。
- 現在の`MAX_MOTOR_SPEED_STEPS_S=20000`、`COREXY_MAX_MOTOR_GAIN=sqrt(2)`、`SPEED_SAFETY=0.80`では、`MAX_FEED_MM_MIN`は約`8485 mm/min`です。
- 現在の`DEFAULT_ACCEL_MM_S2=100.0`では、短い移動では高いfeedへ到達する前に減速へ入ることがあります。
- `Motion stopped: alarm reason=hard limit active away from home`が出る場合は、速度評価を止めて`LIMIT_STATUS`を確認してください。原点から離れた位置でlimitがONなら、脱調判定より先にlimit配線、switch戻り、ノイズ、debounceを切り分けます。
- 通常XY移動は`MotionBlock`、`TrapezoidPlanner`、`SegmentGenerator`、FastAccelStepper `moveTimed()`経由で実行します。
