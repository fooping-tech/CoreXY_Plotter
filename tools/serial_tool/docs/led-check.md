# LED Check

外付けNEOPIXELの配線、色順、輝度、単独pixel指定、patternを確認するチェックです。

## CSV

`tools/serial_tool/examples/led_check.csv`

## Run

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/led_check.csv \
  --startup-delay 4 \
  --timeout 5 \
  --echo
```

## Checks

| Step | What to confirm |
|---|---|
| `CONFIG` | `NEOPIXEL=33 count=8 brightness_max=64`などが配線と一致する |
| `LED_BRIGHTNESS 24` | 眩しすぎない安全な輝度になる |
| `LED 255 0 0` | 全LEDが赤 |
| `LED 0 255 0` | 全LEDが緑 |
| `LED 0 0 255` | 全LEDが青 |
| `LED_PIXEL 0 255 255 255` | index 0だけを指定できる |
| `LED_PATTERN PACIFICA` / `FIRE` | animationが更新される |
| `LED_OFF` | 最後に消灯する |

## Related Config

- `NEOPIXEL_LED_COUNT`
- `NEOPIXEL_BRIGHTNESS_MAX`
- `NEOPIXEL_BRIGHTNESS_DEFAULT`
- `NEOPIXEL_FRAME_INTERVAL_MS`
- `NEOPIXEL_INITIAL_PATTERN`
- `USER_IO_PIN` / `NEOPIXEL_PIN`

## Notes

- 色が入れ替わる場合はLEDの色順または使用LED型番を確認してください。
- 電源容量不足の場合、白や高輝度で不安定になります。
