# Bringup Check

基本的な配線、Serial応答、TMC UART、CoreXYログをまとめて確認するチェックです。
初回はモータ電源を切るか、可動部が安全に動ける状態で実行してください。

## CSV

`tools/serial_tool/examples/bringup.csv`

## Run

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/bringup.csv \
  --startup-delay 4 \
  --timeout 5 \
  --echo
```

## Checks

| Step | What to confirm |
|---|---|
| `CONFIG` | `SIMULATION_MODE`、STEP/DIR、TMC UART、limit、PEN、NEOPIXEL、Core割り付けが意図通り |
| `POS` | `ALARM=NO`、limitが意図せずACTIVEになっていない |
| `SELFTEST` | `SELFTEST PASS` |
| `TMC_INIT` | `TMC_INIT`が出る。実機ではdriver connectionが0なら正常 |
| `TMC_STATUS` | A/B addressが0/1、profileが通常設定へ戻っている |
| `XY` | `A=800 B=800`、`A=800 B=-800`、`A=1600 B=0`のログが出る |

## Related Config

- `SERIAL_BAUD`
- `SIMULATION_MODE`
- `MOTOR_A_DIRECTION_INVERTED`
- `MOTOR_B_DIRECTION_INVERTED`
- `TMC_NORMAL_MICROSTEPS`
- `TMC_NORMAL_RMS_CURRENT_MA`
- `X_MIN_MM` / `X_MAX_MM` / `Y_MIN_MM` / `Y_MAX_MM`

## Notes

- このCSVはXY移動を含みます。
- Serial応答が空の場合は、PlatformIO monitorなどがポートを掴んでいないか確認してください。
