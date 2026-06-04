# Limit Check

X/Y limit switch入力だけを確認するチェックです。
Homingの前に、X switchが`X_RAW` / `X_DEBOUNCED`だけをONにし、Y switchが`Y_RAW` / `Y_DEBOUNCED`だけをONにすることを確認します。

## CSV

`tools/serial_tool/examples/limit_check.csv`

## Run

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/limit_check.csv \
  --startup-delay 4 \
  --timeout 6 \
  --echo
```

## Manual Steps

`--echo`で`LIMIT_STATUS`が表示されるたびに、CSVの順番に合わせてswitchを操作してください。

| Sample | Switch state | Expected log |
|---|---|---|
| 1 | Both released | `X_RAW=OFF X_DEBOUNCED=OFF Y_RAW=OFF Y_DEBOUNCED=OFF` |
| 2 | Only X pressed | `X_RAW=ON X_DEBOUNCED=ON Y_RAW=OFF Y_DEBOUNCED=OFF` |
| 3 | Only Y pressed | `X_RAW=OFF X_DEBOUNCED=OFF Y_RAW=ON Y_DEBOUNCED=ON` |
| 4 | Both released | `X_RAW=OFF X_DEBOUNCED=OFF Y_RAW=OFF Y_DEBOUNCED=OFF` |

## Troubleshooting

| Symptom | Likely cause |
|---|---|
| Pressing X shows `Y_RAW=ON` | X/Y limit wiring is swapped, or X switch is connected to `Y_LIMIT_PIN` |
| Pressing X leaves `X_RAW=OFF` | X switch is not connected to GPIO36, switch polarity is wrong, or input is floating |
| Y is ON with no switch pressed | GPIO35 input is floating or pulled to active level; add/verify external pull-up/pull-down |
| Raw changes but debounced does not | Hold the switch longer than `HOMING_LIMIT_DEBOUNCE_MS` and rerun |

## Firmware Pin Assignment

- `X_LIMIT_PIN`: GPIO36
- `Y_LIMIT_PIN`: GPIO35
- `LIMIT_ACTIVE_LOW=true` means switch active is LOW.

GPIO35/GPIO36 are input-only pins and do not provide useful internal pull-up for this use. Use external pull-up/pull-down wiring that matches `LIMIT_ACTIVE_LOW`.
