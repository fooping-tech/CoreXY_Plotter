# Runtime Config Check

`PlotterConfig.h`由来の一部debug configを、再ビルドなしでSerialから一時変更できることを確認する手順です。

## 対象

- `CONFIG_GET`
- `CONFIG_SET <KEY> <VALUE>`
- `CONFIG_RESET`
- Serial Toolの`--set-config KEY=VALUE`
- Serial Toolの`--reset-config`

## 実行

まずCSVで確認します。

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-0001 \
  --csv tools/serial_tool/examples/runtime_config_check.csv \
  --startup-delay 4 \
  --echo
```

引数でoverrideを前置する場合:

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-0001 \
  --csv tools/serial_tool/examples/config_check.csv \
  --reset-config \
  --set-config DEFAULT_FEED_MM_MIN=900 \
  --set-config PEN_DOWN_ANGLE_DEG=72 \
  --startup-delay 4 \
  --echo
```

## 期待結果

- `CONFIG_GET`で`CONFIG_VALUE KEY=value`が表示される
- `CONFIG_SET DEFAULT_FEED_MM_MIN 900`で`CONFIG_SET DEFAULT_FEED_MM_MIN=900`が返る
- 次の`CONFIG_GET`で`CONFIG_VALUE DEFAULT_FEED_MM_MIN=900.000000`が表示される
- `CONFIG_RESET`後は`PlotterConfig.h`の既定値へ戻る

## 注意

- runtime configはRAM上のdebug overrideです。再起動すると消えます。
- `STEPS_PER_MM`、soft limit、homing位置などを変えた場合は、既存の論理座標と実位置の整合を信用せず、`ZERO`、`HOME`、または再起動で基準を取り直してください。
- GPIO、UART baud、task設定、queue容量などのcompile-time設定はruntime config対象外です。
