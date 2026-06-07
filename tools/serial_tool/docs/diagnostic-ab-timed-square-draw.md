# Diagnostic AB_TIMED Square Draw

`AB_TIMED`でA/Bモータを直接timed segment実行し、通常の`XY`描画経路との差を切り分けるための描画チェックです。
空走ではなく、`PENDOWN`して紙に線を残します。

## Base CSV

このCSVは、最新の四角観察用CSVである以下をベースにしています。

- `tools/serial_tool/examples/concentric_squares_clockwise_check.csv`
- `tools/serial_tool/examples/concentric_squares_check.csv`

初期化シーケンス、中心、正方形サイズ、描画feed `300 mm/min`、開始点移動の考え方を踏襲しています。

## CSV

- `tools/serial_tool/examples/diagnostic_ab_timed_square_draw.csv`

## Run

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/diagnostic_ab_timed_square_draw.csv \
  --startup-delay 4 \
  --startup-drain 1 \
  --timeout 8 \
  --echo
```

送信予定だけを確認する場合:

```bash
python tools/serial_tool/serial_send.py \
  --csv tools/serial_tool/examples/diagnostic_ab_timed_square_draw.csv \
  --dry-run
```

## What This Bypasses

描画辺は`AB_TIMED a_steps b_steps duration_us`で動かします。
これは`XY`、`CoreXYKinematics`、`TrapezoidPlanner`、`SegmentGenerator`、`SegmentQueue`を通しません。

ただし、各正方形の開始点までは安全と比較のため通常の`XY`で移動します。

## Judgment

| 結果 | 疑う場所 |
|---|---|
| 通常XY描画では歪むが、AB_TIMED描画では四角が閉じる | `TrapezoidPlanner`、`SegmentGenerator`、`SegmentQueue`、またはXYコマンド処理側 |
| AB_TIMED描画でも歪む | `StepperBackendFastAccel`、FastAccelStepper `moveTimed()`使用方法、A/B同期開始、`duration_us`指定 |
| AB_TIMEDで小さい四角だけ悪化する | 短距離`moveTimed()`、`duration_us`最小値、step数丸め、A/B step配分 |
| AB_TIMEDで大きい四角は正常、小さい四角はズレる | 台形加速以前の短時間timed move処理 |
| 時計回りと反時計回りでズレ方向が変わる | A/B符号、方向反転、片側モータの開始/停止タイミング差 |
| 通常XYもAB_TIMEDも同じように歪む | backend以下の問題が濃厚。ソフト観点では`StepperBackendFastAccel`とFastAccelStepper設定を重点確認 |
