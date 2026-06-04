# High-Speed Check

CoreXYの通常XY移動を、現在の`MAX_FEED_MM_MIN`上限付近で確認するチェックです。
`../1stepper_test`の速度スイープと同じ考え方で、ファームウェアの通常Serialコマンドだけを使い、速度条件をCSV行として並べます。

## Safety

- このCSVは実機を高速に動かします。E-stopまたはモータ電源をすぐ切れる状態で実行してください。
- 先にhomingを完了し、`POS`が`HOMED=YES`であることを確認してください。
- 現在のsoft limitは`X/Y 0..55mm`想定です。CSVは`10..45mm`だけを使います。
- 異音、脱調、ベルト飛び、ドライバ過熱があれば即停止してください。

## CSV

- `tools/serial_tool/examples/high_speed_check.csv`: 短い確認用
- `tools/serial_tool/examples/high_speed_sweep_check.csv`: 1200から3000 mm/minまで段階確認

## Run

先にhoming:

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/homing_check.csv \
  --startup-delay 4 \
  --timeout 5 \
  --echo
```

高速移動チェック:

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/high_speed_check.csv \
  --startup-delay 2 \
  --timeout 8 \
  --echo
```

どこまで速度を上げられるか見る場合:

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/high_speed_sweep_check.csv \
  --startup-delay 2 \
  --timeout 8 \
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
| `CONFIG` | `max_feed=3000.000`、soft limitがCSVの移動範囲を含む |
| `POS` | `HOMED=YES`、`ALARM=NO` |
| `XY ... 1200..3000` | `ACK_XY`が返り、X往復で脱調や異音がない |
| diagonal validation | `ACK_XY`が返り、斜め往復で脱調や異音がない |
| final `POS` | `ALARM=NO`、位置が最後のtarget付近 |

## Notes

- feedはファームウェアの`XY <x_mm> <y_mm> <feed_mm_min>`契約に合わせて`mm/min`です。
- `3000 mm/min`は`50 mm/s`です。`STEPS_PER_MM=80`では`4000 steps/s`相当です。
- 現在の`MAX_MOTOR_SPEED_STEPS_S=5000`では、理論上のfeed上限は`3750 mm/min`相当です。
- `Motion stopped: alarm reason=hard limit active away from home`が出る場合は、速度評価を止めて`LIMIT_STATUS`を確認してください。原点から離れた位置でlimitがONなら、脱調判定より先にlimit配線、switch戻り、ノイズ、debounceを切り分けます。
- 現在の`StepperBackendFastAccel::moveABSteps()`はbring-up用の独立A/B moveです。厳密なXY補間や速度スイープ判定は、将来のMotionBlock/PlannerQueue/SegmentGenerator追加後に拡張してください。
