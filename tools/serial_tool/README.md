# Serial Tool

CSVで定義したCoreXY plotter firmwareコマンドを、USB Serial経由で順番に送信するPythonツールです。
初期版はコマンド送信に徹し、CoreXY変換やmotion判断はファームウェア側へ任せます。

## Setup

リポジトリ直下でvenvを作成して使います。

```bash
python3 -m venv .venv
source .venv/bin/activate
python -m pip install -r tools/serial_tool/requirements.txt
```

## Dry Run

CSVの読み込みと送信予定だけを確認します。pyserialや実機接続は不要です。

```bash
python tools/serial_tool/serial_send.py \
  --csv tools/serial_tool/examples/config_check.csv \
  --dry-run
```

## Send Commands

Core2のUSB Serialポートを指定して送信します。baudはファームウェアの`SERIAL_BAUD`に合わせて既定で`115200`です。

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-0001 \
  --csv tools/serial_tool/examples/config_check.csv \
  --startup-delay 4 \
  --echo
```

macOSではポート名が環境ごとに異なります。以下のコマンドで確認してください。

```
ls /dev/cu.*
```

Core2はシリアルポートを開いた時にリセットされることがあります。その場合、起動中に最初のコマンドが失われるため、`--startup-delay`を長めにしてください。ログが遅れて返る場合は`--timeout`も調整します。
起動直後に出ているログを読み捨てる時間は`--startup-drain`で指定します。`--timeout`は各コマンド応答の最大待ち時間であり、起動時読み捨て時間には使いません。
このツールは既定ではDTR/RTSを変更しません。USBシリアルアダプタに合わせて必要な場合だけ`--dtr`、`--no-dtr`、`--rts`、`--no-rts`を指定してください。
macOSで`/dev/cu.*`が不安定な場合は、対応する`/dev/tty.*`も試してください。

応答が空のままの場合は、PlatformIO monitorなど別プロセスが同じポートを開いていないか確認してください。

```bash
lsof /dev/cu.usbserial-023591AC
```

別プロセスが表示される場合は、そのSerial Monitorを閉じてから再実行してください。

`ERROR: could not open serial port ... (22, 'Invalid argument')` が出る場合は、コマンド送信前にmacOS/USBシリアルdriverがtermios設定を拒否しています。この状態ではツール側のclose処理以前にopenできていません。以下を確認してください。

```bash
lsof /dev/cu.usbserial-023591AC
stty -f /dev/cu.usbserial-023591AC -a
stty -f /dev/cu.usbserial-023591AC sane
```

`stty ... sane` でも `tcsetattr: Invalid argument` が出る場合は、USBシリアルadapterを抜き差しするか、USB接続をリセットしてから再実行してください。

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/config_check.csv \
  --startup-delay 6 \
  --startup-drain 1 \
  --timeout 5 \
  --echo
```

## Check Documents

このREADMEから以下の順に辿ると、一通りの配線チェック、機能チェック、`PlotterConfig.h`の調整まで進められます。

| Check | Purpose | CSV |
|---|---|---|
| [Bringup Check](docs/bringup-check.md) | Serial、基本設定、TMC、CoreXYログの初期確認 | `examples/bringup.csv` |
| [Direction Check](docs/direction-check.md) | XY実機方向とA/Bモータ方向反転設定 | `examples/config_check.csv`, `examples/xy_direction_check.csv` |
| [CoreXY Check](docs/corexy-check.md) | CoreXY変換ログと短いXY移動 | `examples/corexy_check.csv` |
| [Limit Check](docs/limit-check.md) | X/Y limit switch入力のpin、極性、debounce確認 | `examples/limit_check.csv` |
| [Homing Check](docs/homing-check.md) | X/Y limitを使った二段階homingとhomed状態 | `examples/homing_check.csv` |
| [Abort Check](docs/abort-check.md) | ABORTと中断後のalarm復旧確認 | `examples/abort_check.csv` |
| [Trapezoid Check](docs/trapezoid-check.md) | MotionBlockの台形/三角加減速計画ログ | `examples/trapezoid_check.csv` |
| [Timed Segment Check](docs/timed-segment-check.md) | DDA timed segment生成とFastAccelStepper `moveTimed()`投入 | `examples/timed_segment_check.csv` |
| [High-Speed Check](docs/high-speed-check.md) | homing後の通常XY移動を上限feed付近で確認 | `examples/high_speed_check.csv`, `examples/high_speed_sweep_check.csv` |
| [Concentric Squares Check](docs/concentric-squares-check.md) | 動き出し・動き終わりの線歪みを5重正方形で調査 | `examples/concentric_squares_check.csv`, `examples/concentric_squares_clockwise_check.csv` |
| [Diagnostic AB_TIMED Square Draw](docs/diagnostic-ab-timed-square-draw.md) | `AB_TIMED`でA/Bを直接timed実行して四角の歪みを比較 | `examples/diagnostic_ab_timed_square_draw.csv` |
| [Servo On/Off Check](docs/servo-on-off-check.md) | ペン上げ/下げサーボ角度と配線 | `examples/servo_check.csv` |
| [LED Check](docs/led-check.md) | NEOPIXEL配線、色、輝度、pattern | `examples/led_check.csv` |
| [Melody Check](docs/melody-check.md) | TMC UART、Aモータ、診断メロディprofile | `examples/melody_check.csv` |

設定変更が必要な場合は、該当ドキュメントに従って`include/PlotterConfig.h`を編集し、再ビルド・再書き込みしてください。

```bash
pio run
pio run --target upload
```

## CSV Format

CSVはヘッダ行を必須とし、以下の列を使います。

| Column | Required | Description |
|---|---:|---|
| `command` | yes | ファームウェアへ送る1行コマンド。例: `XY 10 0 600` |
| `delay_ms` | no | 送信後の待ち時間。空なら`--default-delay-ms`を使用 |
| `expect` | no | 受信ログに含まれるべき部分文字列。不一致なら非ゼロ終了 |
| `comment` | no | 人間用メモ。送信されません |

`delay_ms`は各コマンド送信後の最小読み取り時間です。`expect`がある場合は、`delay_ms`経過後に`expect`を受信し、受信が短時間idleになると次の行へ進みます。
最大待ち時間は`max(delay_ms, --timeout)`です。`HOME`のように完了時間が読みにくいコマンドは、CSV側の`delay_ms`を短くし、実行時の`--timeout`を長くしてください。

ファームウェアはparseとキュー投入に成功したコマンドへ`ACK QUEUED <command>`を返します。
XY移動はmotion側で受理されると`ACK_XY target=(x,y) A=a_steps B=b_steps F=feed`も返します。
拒否されたXY移動は`NACK_XY ...`を返します。

実行中に`Ctrl-C`で中断した場合、ツールはserial portを閉じる前に`ABORT`を送信します。
ファームウェア側は実行中のmotion/homingを停止し、alarm状態にしてhomed状態を無効化します。
中断後に復旧する場合は、現在位置を信用せず`ZERO -> ALARM_CLEAR -> HOME`の順に実行してください。

例:

```csv
command,delay_ms,expect,comment
CONFIG,500,,Show firmware configuration
SELFTEST,500,SELFTEST PASS,Validate CoreXY conversion
ZERO,500,ZERO,Reset logical origin
XY 10 0 600,700,A=800,Validate +X CoreXY mapping
```

## Firmware References

このツールを変更する場合は、リポジトリ直下の以下を参照してください。

- `AGENTS.md`: プロジェクト全体の構造方針と禁止事項
- `SPEC.md`: Serialコマンド仕様、XYコマンド仕様
- `PLANS.md`: bring-up手順と手動テスト列
- `README.md`: 実機安全注意と既知制限
- `include/PlotterConfig.h`: `SERIAL_BAUD`などの定数

## Drawing Data Extension

将来、絵をデータ化する場合も送信処理はこのCSV形式を入口にします。
SVG、画像、G-code風データなどから`PENUP`、`PENDOWN`、`XY <x_mm> <y_mm> <feed_mm_min>`のCSVを生成する処理は、送信処理とは別モジュールとして追加してください。

Python側でCoreXYのA/B変換、soft limit判定、planner相当の補間を重複実装しないでください。
それらはファームウェア側の`CoreXYKinematics`、`SafetyManager`、将来のplannerが担当します。

## Safety

- 初回確認はモータ電源を切るか、`SIMULATION_MODE`で行ってください。
- `XY`や`TEST_A`/`TEST_B`を含むCSVは実機を動かす可能性があります。
- `HOME`、`HOME_X`、`HOME_Y`を含むCSVはlimit方向へ実機を動かします。E-stopまたはモータ電源を切れる状態で実行してください。
- `expect`はログ確認用であり、機械的な安全確認の代替ではありません。
