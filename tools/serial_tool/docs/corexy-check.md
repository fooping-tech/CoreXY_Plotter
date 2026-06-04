# CoreXY Check

CoreXY変換ログと短いXY移動を確認するチェックです。
方向確認よりも、ファームウェアが期待するA/B stepを出しているかを見る目的です。

## CSV

`tools/serial_tool/examples/corexy_check.csv`

## Run

このチェックで`ACK_XY`を見るには、`CONFIG`で`require_homed_xy=1`の場合、先にhomingを完了して`POS`が`HOMED=YES`になっている必要があります。
未homingの場合、XYは`REJECT: machine is not homed`と`NACK_XY ...`を返します。

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/corexy_check.csv \
  --startup-delay 4 \
  --timeout 5 \
  --echo
```

## Expected Logs

各Serialコマンドはparseとキュー投入に成功すると`ACK QUEUED <command>`を返します。
XY移動はmotion側で受理されると、以下の`ACK_XY`も返します。
安全確認などで拒否された場合は`NACK_XY`が返ります。

| Command | Expected ACK |
|---|---|
| `XY 10 0 600` | `ACK_XY target=(10.000,0.000) A=800 B=800` |
| `XY 0 10 600` | `ACK_XY target=(0.000,10.000) A=800 B=-800` |
| `XY 10 10 600` | `ACK_XY target=(10.000,10.000) A=1600 B=0` |
| `XY 20 0 600` from current `(10,10)` | `ACK_XY target=(20.000,0.000) A=0 B=1600` |

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
