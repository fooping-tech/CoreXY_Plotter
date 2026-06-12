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

G-codeファイルを直接送る場合は`--gcode`を使います。空行、`;`で始まるコメント行、`%`行は送信しません。
描画前にalarm clearとhomingが必要な場合は、`--preamble-csv`で準備CSVを前置します。
正式ジョブ運用では、事前bring-up後に`--job-lifecycle`を指定し、ファームウェア側の`JOB_BEGIN`/`JOB_END`で開始/終了処理を行います。

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-0001 \
  --preamble-csv tools/serial_tool/examples/gcode_preamble.csv \
  --gcode tools/text_tool/examples/gcode/text_robo.gcode \
  --startup-delay 4 \
  --queue-mode \
  --stream-gcode-motion \
  --echo
```

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-0001 \
  --gcode tools/text_tool/examples/gcode/text_robo.gcode \
  --job-lifecycle \
  --startup-delay 4 \
  --queue-mode \
  --stream-gcode-motion \
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

デバッグ用に`PlotterConfig.h`由来のruntime configを一時変更したい場合は、入力CSV/G-codeの前に`--reset-config`と`--set-config KEY=VALUE`を指定できます。変更はRAM上だけで、再起動すると`PlotterConfig.h`の既定値へ戻ります。

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
| [Look-ahead Check](docs/lookahead-check.md) | JunctionPlanner、junction deviation、連続XYバッチの確認 | `examples/lookahead_check.csv` |
| [G-code Check](docs/gcode-check.md) | Phase 7の最小G-code parser/interpreter確認 | `examples/gcode_check.csv` |
| [Job Lifecycle Check](docs/job-lifecycle-check.md) | Phase 10.5のG-codeジョブ開始/終了処理確認 | `examples/job_lifecycle_check.csv` |
| [Runtime Config Check](docs/runtime-config-check.md) | `PlotterConfig.h`由来debug configのSerial runtime override確認 | `examples/runtime_config_check.csv` |
| [High-Speed Check](docs/high-speed-check.md) | homing後の通常XY移動を上限feed付近で確認 | `examples/high_speed_check.csv`, `examples/high_speed_sweep_check.csv` |
| [Concentric Squares Check](docs/concentric-squares-check.md) | 動き出し・動き終わりの線歪みを5重正方形で調査 | `examples/concentric_squares_check.csv`, `examples/concentric_squares_clockwise_check.csv`, `examples/concentric_squares_high_speed_check.csv` |
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
| `command` | yes | ファームウェアへ送る1行コマンド。例: `XY 10 0` |
| `delay_ms` | no | 送信後の待ち時間。空なら`--default-delay-ms`を使用 |
| `expect` | no | 受信ログに含まれるべき部分文字列。不一致なら非ゼロ終了 |
| `comment` | no | 人間用メモ。送信されません |

`delay_ms`は各コマンド送信後の最小読み取り時間です。`expect`がある場合は、`delay_ms`経過後に`expect`を受信し、受信が短時間idleになると次の行へ進みます。
最大待ち時間は`max(delay_ms, --timeout, 推定motion時間 + --motion-timeout-margin)`です。`--timeout`の既定値は30秒、`--motion-timeout-margin`の既定値は5秒です。
Serial Toolは`XY`、G-code `G0/G1`、`G4`の実行時間を概算し、長い移動やdwellではホスト側timeoutを自動で延長します。
feedが指定されていないmotionの推定には`--estimate-feed-mm-min`を使います。既定値は1200mm/minです。
この自動延長を無効にする場合は`--no-auto-motion-timeout`を指定してください。
`HOME`のように完了時間が読みにくいコマンドは、CSV側の`delay_ms`を短くし、必要に応じて実行時の`--timeout`をさらに長くしてください。

各コマンドの開始時と終了時には`TIMING START`、`TIMING END`を表示します。
`t=...s`は`--startup-delay`と`--startup-drain`後、最初のCSVコマンドを送る直前を0とした相対時刻です。
`TIMING END`の`dt=...s`は、そのCSV行の開始から終了までの経過時間です。

`--queue-mode`では、各行は`ACK QUEUED`を受信してから次の行へ進みます。
`ERROR: CommandQueue full`を受信した場合は、同じ行を`--queue-retry-delay-ms`ごとに再送します。
`HOME`、`HOME_X`、`HOME_Y`は後続motionを先に積まないよう、queue投入後も完了ログまで待ちます。
これらのhomingコマンドはCSVの`delay_ms=0`でも既定で最大30秒まで完了ログを待ちます。
このモードではCSVの`delay_ms=0`と空の`expect`を使って、固定待ちなしでCommandQueueへ詰められます。

CSVの`XY`を連続して滑らかに描きたい場合は、`--queue-mode --stream-xy-motion`を使います。
CSV由来の`XY`は`ACK QUEUED`を見つけた時点でserial idleと`ACK_XY target=`完了ログを待たずに次行へ進みます。
送信速度を落とさないため、成功時の`--echo`、`TIMING START/END`、ACK表示はstream対象の`XY`では抑制します。`PENUP/PENDOWN`、`HOME`、`POS`、エラー、queue fullは従来通り表示します。

`--gcode --queue-mode --stream-gcode-motion`を指定すると、G-codeファイル由来の`G0/G1`は`ACK QUEUED`だけを確認して次の行へ進み、`ACK_XY target=`の完了ログを待ちません。
日本語テキストや細かい折れ線G-codeを滑らかに描きたい場合はこのモードを使ってください。従来の完了待ち送信では各線分の完了後に次行を送るため、ファームウェア側のlook-aheadへ連続XYが溜まりにくく、線分ごとに停止して見えることがあります。
stream対象の`G0/G1`は、`ACK QUEUED`を見つけた時点でserial idleを待たずに次行へ進みます。また送信速度を落とさないため、成功時の`--echo`、`TIMING START/END`、ACK表示はstream対象行では抑制します。エラー、queue full、`M3/M5`、`G4`、`G28`などの非motion行は従来通り表示します。
`M3/M5`、`G4`、`G28`、`M114`、`G20/G21/G90/G91`は従来通り、それぞれの完了ログやmodalログを待ちます。
先行投入した`G0/G1`の`ACK_XY target=`は後続コマンドの応答読み取り中に流れてくる場合がありますが、後続コマンドの完了判定には使いません。
`--stream-gcode-motion`または`--stream-xy-motion`中は、先行投入したmotionの推定残り時間を次の非stream行のtimeoutへ足します。長いtravel move直後の`M3/M5/JOB_END`待ちで、前の移動が終わる前にホスト側timeoutへ到達する問題を避けるためです。
期待するACKや完了ログが最大待ち時間内に出ない場合は、`timeout after ... waiting for ...`として表示します。timeout時は、その時点までに受信したSerialログも`timeout partial serial log`として表示します。これは`NACK`や`REJECT:`などのファームウェア拒否とは別の、ホスト側待ち時間切れです。

`--gcode --job-lifecycle`を指定すると、G-code本文の前に`JOB_BEGIN`、最後に`JOB_END`を自動で送ります。
G-code行の送信中に`NACK`、`REJECT:`、alarm、`ERROR:`を検出した場合は、後続行を止めて`JOB_ABORT`を送ります。
`JOB_BEGIN`はファームウェア側でalarm、TMC ready、homed、pen up、motion queue idleを確認します。TMC未readyなら自動で`TMC_INIT`相当を実行します。`JOB_BEGIN_AUTO_HOME=false`では未homed時に拒否し、`JOB_BEGIN_AUTO_HOME=true`では未homed時にHOME相当を自動実行します。
`JOB_BEGIN`は自動HOMEを含む場合があるため、`--timeout`の既定値に関係なく最大60秒まで`JOB_BEGIN OK`を待ちます。
`JOB_END`はpen up後に`X=5mm, Y=Y_MAX_MM-5mm`へ退避し、A/B両モータで短い終了ジングルを鳴らします。

`--reset-config`と`--set-config KEY=VALUE`は、CSV/G-code本文より前に`CONFIG_RESET`と`CONFIG_SET`を送ります。複数の`--set-config`は指定順に実行されます。TMC関連keyを変更した場合、ファームウェア側でTMC初期化済みなら通常profileを再適用します。

ファームウェアはparseとキュー投入に成功したコマンドへ`ACK QUEUED <command>`を返します。
XY移動はmotion側で受理されると`ACK_XY target=(x,y) A=a_steps B=b_steps F=feed`も返します。
`XY <x_mm> <y_mm>`はファームウェア側の`DEFAULT_FEED_MM_MIN`を使います。
速度評価や一時的なoverrideが必要なCSVだけ、`XY <x_mm> <y_mm> <feed_mm_min>`で明示feedを指定します。
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
XY 10 0,700,A=800,Validate +X CoreXY mapping
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
SVG、画像、G-code風データなどから`PENUP`、`PENDOWN`、`XY <x_mm> <y_mm>`のCSVを生成する処理は、送信処理とは別モジュールとして追加してください。
速度条件を検査したいCSVでは`XY <x_mm> <y_mm> <feed_mm_min>`を使えます。

Python側でCoreXYのA/B変換、soft limit判定、planner相当の補間を重複実装しないでください。
それらはファームウェア側の`CoreXYKinematics`、`SafetyManager`、将来のplannerが担当します。

## G-code File Format

`.gcode`は1行1コマンドとして送信します。`serial_send.py`は以下の行をスキップします。

- 空行
- `;`で始まるコメント行
- `%`だけの行

インラインコメントはファームウェア側のG-code parserが`;`以降を無視するため、そのまま送ります。`--gcode`では、コマンド種別に応じて既定の確認ログを待ちます。`G0/G1`は`ACK_XY target=`、`M3/M5`は`PEN DOWN`/`PEN UP`、`G4`は`DWELL P=`を待ちます。
`--stream-gcode-motion`を併用した場合だけ、G-codeファイル由来の`G0/G1`はqueue投入確認で先へ進みます。ストローク間の停止が気になる場合は、Text Tool側の`--dwell-ms`も下げて調整してください。
`NACK`、`REJECT:`、`ALARM=YES`、`machine is alarmed`、`ERROR:`を受信した場合は、その行で失敗扱いにして停止します。

描画前の暫定bring-up準備には`tools/serial_tool/examples/gcode_preamble.csv`を使えます。このCSVは`SELFTEST`、`ZERO`、`ALARM_CLEAR`、`LIMIT_STATUS`、`G28`、`POS`を送り、homing完了と`HOMED=YES`を確認します。正式ジョブでは`--job-lifecycle`へ移行します。

## Safety

- 初回確認はモータ電源を切るか、`SIMULATION_MODE`で行ってください。
- `XY`や`TEST_A`/`TEST_B`を含むCSVは実機を動かす可能性があります。
- `HOME`、`HOME_X`、`HOME_Y`を含むCSVはlimit方向へ実機を動かします。E-stopまたはモータ電源を切れる状態で実行してください。
- `expect`はログ確認用であり、機械的な安全確認の代替ではありません。
