# Concentric Squares Check

動き出し・動き終わりで生じる線の歪みを、水平/垂直の短い線分で切り分けるための描画チェックです。
同じ中心`(27.5, 27.5)`に、辺長`42 / 34 / 26 / 18 / 10 mm`の正方形を5個重ねて描きます。
反時計回り版、時計回り版、高速版を比較できます。

## Safety

- このCSVは実機を動かし、ペンを下げます。E-stopまたはモータ電源をすぐ切れる状態で実行してください。
- CSV内でhomingします。開始前に原点方向の移動が安全な状態にしてください。
- 現在のsoft limitは`X/Y 0..55mm`想定です。最大正方形は`6.5..48.5mm`を使い、端から余白を残します。
- 通常版と時計回り版は`XY`のfeedを省略し、ファームウェア側の`DEFAULT_FEED_MM_MIN`を使います。
- 高速版は描画feedを`1800 mm/min`、ペンアップ移動feedを`2400 mm/min`に上げます。通常版で完走し、温度と脱調に問題がないことを確認してから実行してください。
- 異音、脱調、ベルト飛び、ペンの引っかかり、ドライバ過熱があれば即停止してください。

## CSV

- `tools/serial_tool/examples/concentric_squares_check.csv`: 反時計回り
- `tools/serial_tool/examples/concentric_squares_clockwise_check.csv`: 時計回り
- `tools/serial_tool/examples/concentric_squares_high_speed_check.csv`: 反時計回り、高速版

## Run

反時計回り:

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/concentric_squares_check.csv \
  --startup-delay 4 \
  --startup-drain 1 \
  --timeout 12 \
  --echo
```

時計回り:

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/concentric_squares_clockwise_check.csv \
  --startup-delay 4 \
  --startup-drain 1 \
  --timeout 12 \
  --echo
```

高速版:

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/concentric_squares_high_speed_check.csv \
  --startup-delay 0 \
  --startup-drain 1 \
  --timeout 12 \
  --queue-mode \
  --echo
```

高速版CSVは`delay_ms=0`、`expect`空欄です。
`--queue-mode`なしで実行すると`CommandQueue full`で後続コマンドが落ち、ペン操作順序も崩れる可能性があります。

送信予定だけを確認する場合:

```bash
python tools/serial_tool/serial_send.py \
  --csv tools/serial_tool/examples/concentric_squares_check.csv \
  --dry-run
```

```bash
python tools/serial_tool/serial_send.py \
  --csv tools/serial_tool/examples/concentric_squares_clockwise_check.csv \
  --dry-run
```

```bash
python tools/serial_tool/serial_send.py \
  --csv tools/serial_tool/examples/concentric_squares_high_speed_check.csv \
  --dry-run
```

## What To Check

| 観察点 | 見る内容 |
|---|---|
| 各辺の始点 | ペンを下げた直後や加速直後に角が丸まる、膨らむ、欠けるか |
| 各辺の終点 | 減速時に線が伸びる、角がずれる、オーバーシュートするか |
| 水平辺と垂直辺の差 | X方向だけ、Y方向だけ、または両方で歪むか |
| 描画方向の差 | 反時計回りと時計回りで角の歪み位置が入れ替わるか |
| 速度差 | 通常版と高速版で角の丸まり、終点オーバーシュート、閉じズレが増えるか |
| 正方形サイズ差 | 大きい正方形だけ歪むなら速度/加減速、小さい正方形も歪むならペン圧や機械ガタを疑う |
| 閉じ位置 | 各正方形の最後の角が始点と一致するか |

最終`POS`で`ALARM=NO`、`LIMIT_X=OPEN`、`LIMIT_Y=OPEN`を確認します。
