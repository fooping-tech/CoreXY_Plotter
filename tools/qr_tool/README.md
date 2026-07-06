# QR Tool

QR文字列やURLから、既存のSerial Toolで送れるプロッタ用G-codeと、同じハッチング線を表示する確認用SVGを生成します。
ファームウェアにはQRエンコード処理を入れず、生成後のG-codeは`G21`、`G90`、`G0`、`G1`、`M3`、`M5`、`G4`だけを使います。
CSV出力も可能ですが、通常の描画ジョブはG-codeと`--job-lifecycle`で実行します。

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
  --gcode-output tools/qr_tool/examples/gcode/qr_hello.gcode \
  --preview-svg tools/qr_tool/examples/qr_hello.svg \
  --origin-x 10 \
  --origin-y 10 \
  --module-mm 1.0 \
  --hatch-pitch-mm 0.35 \
  --draw-feed 600 \
  --travel-feed 1800 \
  --error-correction M
```

`qrcode`のquiet zoneは4 modulesです。黒セルは上下左右につながる接続成分へ結合し、各成分をペンアップなしの横方向ジグザグで塗ります。
G-code出力にはbring-up preambleを入れません。正式ジョブとして送る場合はSerial Toolの`--job-lifecycle`を使って、ファームウェア側に`JOB_BEGIN`/`JOB_END`を送ります。

## Send

実機へ送る前にdry-runで行数とコマンドを確認してください。

```bash
python tools/serial_tool/serial_send.py \
  --gcode tools/qr_tool/examples/gcode/qr_hello.gcode \
  --job-lifecycle \
  --queue-mode \
  --stream-gcode-motion \
  --dry-run
```

実機送信前に、Core2とプロッタが安全に動ける状態か確認します。

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/config_check.csv \
  --startup-delay 4 \
  --echo
```

`JOB_BEGIN_AUTO_HOME=false`の場合は、ジョブ送信前にhomingを完了して`HOMED=YES`にしてください。
Core2がSerial port openでリセットされる環境では、前回の`serial_send.py`実行で完了したhoming状態が次回実行時に消えることがあります。

必要なら先にhomingします。動作範囲とlimit switchを確認し、E-stopまたはモータ電源を切れる状態で実行してください。

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/homing_check.csv \
  --startup-delay 4 \
  --timeout 30 \
  --echo
```

G-codeジョブとして送ります。

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --gcode tools/qr_tool/examples/gcode/qr_hello.gcode \
  --startup-delay 4 \
  --timeout 10 \
  --queue-mode \
  --stream-gcode-motion \
  --job-lifecycle \
  --echo
```

`--stream-gcode-motion`では、G-codeの`G0/G1`は`ACK QUEUED`後に先行投入し、`M3/M5`、`G4`、`G21/G90`、`JOB_BEGIN/JOB_END`は完了ログを待ちます。前の移動が長い場合、Serial Toolは推定motion時間を次の非motion行のtimeoutへ自動で足します。実機が推定より大幅に遅い場合は`--motion-timeout-margin`を長くしてください。

CSVも生成したい場合は`--output`を追加します。

```bash
python tools/qr_tool/qr_to_plot_csv.py \
  --text "HELLO COREXY" \
  --output tools/serial_tool/examples/qr_hello.csv \
  --gcode-output tools/qr_tool/examples/gcode/qr_hello.gcode \
  --preview-svg tools/qr_tool/examples/qr_hello.svg
```

CSVを送る場合は、診断/旧方式として以下のように実行します。

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/qr_hello.csv \
  --startup-delay 4 \
  --timeout 30 \
  --queue-mode \
  --echo
```

未homed、soft limit超過、alarm中などで`NACK_XY`、`REJECT:`、`ERROR:`になった場合は、Serial Toolがその行で停止します。`timeout after ... waiting for ...`は、ファームウェア拒否ではなくホスト側の待ち時間切れです。

## Parameters

| Option | Meaning |
|---|---|
| `--text` | QRに入れる文字列またはURL |
| `--output` | Serial Toolへ渡すCSV出力先。G-codeだけ使う場合は省略可 |
| `--gcode-output` | Serial Toolの`--gcode`へ渡すG-code出力先 |
| `--preview-svg` | 実際に出力されるハッチング線のSVG確認出力先 |
| `--origin-x`, `--origin-y` | QR左上の原点位置mm |
| `--module-mm` | 1 QR moduleの一辺mm。大きいほど読みやすいが描画範囲と時間が増える |
| `--hatch-pitch-mm` | ハッチング線の間隔mm。小さいほど黒セルが濃くなるが行数が増える |
| `--draw-feed` | ペンダウン描画時の`XY` feed mm/min |
| `--travel-feed` | ペンアップ移動時の`XY` feed mm/min |
| `--dwell-ms` | G-code出力で`M3`/`M5`後に入れる`G4`待ち時間ms |
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
