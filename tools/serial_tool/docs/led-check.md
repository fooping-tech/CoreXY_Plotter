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
| `LED_AUTO 1` | 自動ステータス表示へ切り替わる |
| `LED_STATUS_SET IDLE` | 暗い青の呼吸パターン |
| `LED_STATUS_SET HOMING` | 黄のCHASE |
| `LED_STATUS_SET DRAWING_PEN_UP` | 青のCHASE |
| `LED_STATUS_SET DRAWING_PEN_DOWN` | 緑のCHASE |
| `LED_STATUS_SET PAUSED` | 黄のALERT |
| `LED_STATUS_SET COMPLETED` | 緑のSUCCESS |
| `LED_STATUS_SET ERROR` | 赤のALERT |
| `LED_AUTO 0` | 手動表示優先へ切り替わる |
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
- `LED`、`LED_PIXEL`、`LED_PATTERN`、`LED_OFF`は手動表示として扱い、自動ステータス表示を無効化します。自動表示へ戻す場合は`LED_AUTO 1`を送ってください。
