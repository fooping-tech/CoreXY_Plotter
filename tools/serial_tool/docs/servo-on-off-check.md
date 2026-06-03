# Servo On/Off Check

ペン上げ/下げサーボの配線、角度、電源を確認するチェックです。
ファームウェアコマンド名は`PENUP`と`PENDOWN`です。

## CSV

`tools/serial_tool/examples/servo_check.csv`

## Run

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/servo_check.csv \
  --startup-delay 4 \
  --timeout 5 \
  --echo
```

## Checks

| Step | What to confirm |
|---|---|
| `CONFIG` | `PEN=32`が実配線と一致する |
| `POS` | 起動時は`PEN=UP` |
| `PENDOWN` | サーボがペン下げ方向へ動く |
| `PENUP` | サーボがペン上げ方向へ動く |
| repeated `PENDOWN`/`PENUP` | 引っかかりや過大角度がない |

## Related Config

- `PEN_SERVO_PIN`
- `PEN_UP_ANGLE_DEG`
- `PEN_DOWN_ANGLE_DEG`

## Adjust

角度が合わない場合は`include/PlotterConfig.h`の角度を調整します。

```cpp
constexpr uint8_t PEN_UP_ANGLE_DEG = 30;
constexpr uint8_t PEN_DOWN_ANGLE_DEG = 70;
```

変更後は再ビルド・再書き込みしてください。

```bash
pio run
pio run --target upload
```

サーボ電源はCore2本体からではなく、外部5V電源を推奨します。
