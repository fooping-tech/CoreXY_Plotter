# Concentric Squares Check

動き出し・動き終わりで生じる線の歪みを、水平/垂直の短い線分で切り分けるための描画チェックです。
同じ中心`(27.5, 27.5)`に、辺長`42 / 34 / 26 / 18 / 10 mm`の正方形を5個重ねて描きます。
反時計回り版と時計回り版を同じ条件で比較できます。

## Safety

- このCSVは実機を動かし、ペンを下げます。E-stopまたはモータ電源をすぐ切れる状態で実行してください。
- CSV内でhomingします。開始前に原点方向の移動が安全な状態にしてください。
- 現在のsoft limitは`X/Y 0..55mm`想定です。最大正方形は`6.5..48.5mm`を使い、端から余白を残します。
- 異音、脱調、ベルト飛び、ペンの引っかかり、ドライバ過熱があれば即停止してください。

## CSV

- `tools/serial_tool/examples/concentric_squares_check.csv`: 反時計回り
- `tools/serial_tool/examples/concentric_squares_clockwise_check.csv`: 時計回り

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

## What To Check

| 観察点 | 見る内容 |
|---|---|
| 各辺の始点 | ペンを下げた直後や加速直後に角が丸まる、膨らむ、欠けるか |
| 各辺の終点 | 減速時に線が伸びる、角がずれる、オーバーシュートするか |
| 水平辺と垂直辺の差 | X方向だけ、Y方向だけ、または両方で歪むか |
| 描画方向の差 | 反時計回りと時計回りで角の歪み位置が入れ替わるか |
| 正方形サイズ差 | 大きい正方形だけ歪むなら速度/加減速、小さい正方形も歪むならペン圧や機械ガタを疑う |
| 閉じ位置 | 各正方形の最後の角が始点と一致するか |

最終`POS`で`ALARM=NO`、`LIMIT_X=OPEN`、`LIMIT_Y=OPEN`を確認します。
