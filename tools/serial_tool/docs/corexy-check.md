# CoreXY Check

CoreXY変換ログと短いXY移動を確認するチェックです。
方向確認よりも、ファームウェアが期待するA/B stepを出しているかを見る目的です。

## CSV

`tools/serial_tool/examples/corexy_check.csv`

## Run

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/corexy_check.csv \
  --startup-delay 4 \
  --timeout 5 \
  --echo
```

## Expected Logs

| Command | Expected A/B |
|---|---|
| `XY 10 0 600` | `A=800 B=800` |
| `XY 0 10 600` | `A=800 B=-800` |
| `XY 10 10 600` | `A=1600 B=0` |
| `XY 20 0 600` from current `(10,10)` | `A=0 B=1600` |

## Related Config

- `STEPS_PER_MM`
- `DEFAULT_FEED_MM_MIN`
- `MAX_FEED_MM_MIN`
- `X_MIN_MM` / `X_MAX_MM`
- `Y_MIN_MM` / `Y_MAX_MM`

## Notes

- `XY 20 0 600`は現在位置`(10,10)`から`dx=+10, dy=-10`を作るための移動です。`ZERO`直後には実行しないでください。
- soft limitに引っかかる場合は`ZERO`位置と可動範囲を確認してください。
- CoreXY式はファームウェアの`CoreXYKinematics`だけに置き、Python側へ重複実装しません。
