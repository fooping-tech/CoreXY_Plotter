# Abort Check

`ABORT`コマンドと、serial tool中断時の復旧前提を確認するためのチェックです。
このCSVはXY移動やhomingを行わず、`ABORT`でalarmへ遷移することだけを確認します。

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/abort_check.csv \
  --startup-delay 4 \
  --startup-drain 1 \
  --timeout 5 \
  --echo
```

期待結果:

- `ABORT`送信後の`POS`で`ALARM=YES`になる
- `ZERO -> ALARM_CLEAR`後の`POS`で`ALARM=NO`になる

実行中のXY移動やHOMEを`Ctrl-C`で中断した場合、serial toolはport close前に`ABORT`を送信します。
その後の復旧は、現在位置を信用せず`ZERO -> ALARM_CLEAR -> HOME`の順に行ってください。
