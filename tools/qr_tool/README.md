# QR Tool

QR文字列やURLから、既存のSerial Toolで送れるプロッタ用CSVと、同じハッチング線を表示する確認用SVGを生成します。
ファームウェアにはQRエンコード処理を入れず、生成後のCSVは`PENUP`、`PENDOWN`、`XY`だけを使います。

## Setup

リポジトリ直下のvenvに追加します。

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r tools/qr_tool/requirements.txt
```

## Generate

```bash
python tools/qr_tool/qr_to_plot_csv.py \
  --text "HELLO COREXY" \
  --output tools/serial_tool/examples/qr_hello.csv \
  --preview-svg tools/qr_tool/qr_hello.svg \
  --origin-x 10 \
  --origin-y 10 \
  --module-mm 1.0 \
  --hatch-pitch-mm 0.35 \
  --draw-feed 600 \
  --travel-feed 1800 \
  --error-correction M
```

`qrcode`のquiet zoneは4 modulesです。黒セルは横方向の連続runに結合し、各run矩形の外周を描いてから、内部をペンアップなしの45度ジグザグハッチングで塗ります。CSVの最初と最後には`PENUP`が入ります。
生成CSVの先頭には`CONFIG`、`SELFTEST`、`TMC_INIT`、`TMC_STATUS`、`PENUP`、`ZERO`、`ALARM_CLEAR`、`LIMIT_STATUS`、`HOME`、`POS`のbring-up確認preambleが入ります。

## Send

実機へ送る前にdry-runで行数とコマンドを確認してください。

```bash
python tools/serial_tool/serial_send.py \
  --csv tools/serial_tool/examples/qr_hello.csv \
  --dry-run
```

QR CSVは通常XY移動を使うため、現在のファームウェア設定では先にhomingを完了して`HOMED=YES`にしてください。
未homed状態では`XY`が`NACK_XY ... reason=rejected`になり、機械は動かず、その場でペンの上げ下げだけが実行されます。
Core2がSerial port openでリセットされる環境では、前回の`serial_send.py`実行で完了したhoming状態が次回実行時に消えることがあります。
QR送信直前の同じ接続で`POS`が`HOMED=YES`を返す状態にしてから送ってください。

まず安全な状態で以下を確認します。

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/config_check.csv \
  --startup-delay 4 \
  --echo
```

次にhomingします。動作範囲とlimit switchを確認し、E-stopまたはモータ電源を切れる状態で実行してください。

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/homing_check.csv \
  --startup-delay 4 \
  --timeout 30 \
  --echo
```

送信例:

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/qr_hello.csv \
  --startup-delay 4 \
  --timeout 30 \
  --queue-mode \
  --echo
```

生成CSVの`XY`行には`expect=ACK_XY target=`が入ります。未homed、soft limit超過、alarm中などで`NACK_XY`になった場合は、Serial Toolがその行で停止します。

## Parameters

| Option | Meaning |
|---|---|
| `--text` | QRに入れる文字列またはURL |
| `--output` | Serial Toolへ渡すCSV出力先 |
| `--preview-svg` | 実際に出力されるハッチング線のSVG確認出力先 |
| `--origin-x`, `--origin-y` | QR左上の原点位置mm |
| `--module-mm` | 1 QR moduleの一辺mm。大きいほど読みやすいが描画範囲と時間が増える |
| `--hatch-pitch-mm` | ハッチング線の間隔mm。小さいほど黒セルが濃くなるが行数が増える |
| `--draw-feed` | ペンダウン描画時の`XY` feed mm/min |
| `--travel-feed` | ペンアップ移動時の`XY` feed mm/min |
| `--error-correction` | QR誤り訂正レベル。`L`, `M`, `Q`, `H`から選択 |
| `--version` | QR version 1-40固定。省略時は入力文字列に合わせて自動 |

出力座標はQR matrixの行方向に合わせ、左上原点から+Y方向へ進みます。実機の紙の向きや座標系に合わせて`origin`を決めてください。

## 読めない場合の調整順

1. `--module-mm`を大きくする
2. `--hatch-pitch-mm`を小さくする
3. `--draw-feed`を下げる
4. `--error-correction`を`Q`または`H`へ上げる

それでも読めない場合は、ペン先径、インクのにじみ、紙送り方向、XY直角度、ペン圧を確認してください。ハッチ線が太くつぶれる場合は、`hatch-pitch-mm`を小さくする前に`module-mm`を大きくします。

## Firmware Boundary

このツールはホスト側でQR matrixとハッチング線を作るだけです。CoreXY変換、soft limit、planner、TMC2209制御はファームウェア側が担当します。
