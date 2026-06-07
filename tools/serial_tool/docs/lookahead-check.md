# Look-ahead Check

Phase 10の`JunctionPlanner`、junction deviation、reverse/forward pass、連続XYバッチ処理を確認する手順です。

## CSV

`--queue-mode`を使って連続XYをCommandQueueへ詰めます。

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/lookahead_check.csv \
  --startup-delay 4 \
  --timeout 5 \
  --queue-mode \
  --echo
```

## Expected

| Command | Expected log |
|---|---|
| `CONFIG` | `LOOKAHEAD junction_deviation=` |
| queued `XY` group | `LOOKAHEAD blocks=` with `blocks` greater than 1 when the queue is filled fast enough |
| each `XY` | `entry=` and `exit=` in the `XY batch=` log |
| each `XY` | `TRAPEZOID ...` followed by `SEGMENTS count=` and `ACK_XY` |

`LOOKAHEAD blocks=1`しか出ない場合は、serial送信間隔や起動ログの混雑でmotionTaskの収集窓に次のXYが入っていません。
`LOOKAHEAD_BATCH_COLLECT_MS`を一時的に増やすか、`--queue-mode`で再実行してください。

## Mechanical Checks

- 四角の角で停止せず、ただし角の丸まりが許容範囲か確認する
- `JUNCTION_DEVIATION_MM`を大きくした場合、角の丸まり、脱調、閉じズレが増えないか確認する
- `CLASSIC_JERK_LIMIT_MM_S`を下げた場合、角で十分に減速するか確認する
- 実機で温度、停止距離、limit入力ノイズ、連続実行耐性は未確認として記録する
