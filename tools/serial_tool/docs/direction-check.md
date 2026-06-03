# Direction Check

XY方向確認は、ファームウェアのCoreXY計算が正しいことと、実機のA/Bモータ方向が機械座標に合っていることを分けて確認します。
Python側やCSV側でモータ方向を設定しません。方向反転はファームウェアの`include/PlotterConfig.h`で設定します。

```cpp
constexpr bool MOTOR_A_DIRECTION_INVERTED = false;
constexpr bool MOTOR_B_DIRECTION_INVERTED = false;
```

## CSV

- 設定確認: `tools/serial_tool/examples/config_check.csv`
- 方向確認: `tools/serial_tool/examples/xy_direction_check.csv`

## Run Config Check

まず移動なしで設定とTMC UARTを確認します。

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/config_check.csv \
  --startup-delay 4 \
  --timeout 5 \
  --echo
```

## Run Direction Check

短い5mm移動で方向を確認します。各CSV行の`comment`にも観察内容があります。

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/xy_direction_check.csv \
  --startup-delay 4 \
  --timeout 5 \
  --echo
```

## Expected Motion

| Command | Expected motion | Firmware log check |
|---|---|---|
| `XY 5 0 300` | +X方向へ動く。Y方向へ流れない | `A=400 B=400` |
| `XY 0 5 300` | +Y方向へ動く | `A=400 B=-400` |
| `XY 5 5 300` | +X+Yの対角方向へ動く | `A=800 B=0` |

## How To Adjust

観察結果に応じて、`include/PlotterConfig.h`の`MOTOR_A_DIRECTION_INVERTED`または`MOTOR_B_DIRECTION_INVERTED`を変更し、ファームウェアを再ビルド・再書き込みしてください。

```bash
pio run
pio run --target upload
```

| Observation | Action |
|---|---|
| +X命令で-Xへ動く、+Y命令で-Yへ動く | AとBの両方を反転する |
| +X命令が主にY方向へ動き、+Y命令が主にX方向へ動く | A/Bモータの配線またはSTEP/DIR割り当てを入れ替えていないか確認する |
| +X命令で斜めに流れる | A/Bの片方だけ方向が反転している可能性が高い |
| 片方のモータだけ動かない、異音がする | CSVを止め、TMC配線、電源、コイルペア、`TMC_STATUS`を確認する |

方向が合ったら、同じCSVをもう一度実行し、3つの移動が期待方向になることを確認してください。
