# Serialコマンドリファレンス

全Serialコマンドと対応G-codeの完全な一覧。コマンド仕様の正はこのファイルとする。
挙動の背景仕様(応答ルール、homing手順、Job Lifecycle詳細)は`SPEC.md` §15/§16/§21を参照。
実装は`src/CommandDispatcher.cpp`(parse)と`src/tasks/MotionTask.cpp`(実行)にある。
コマンドを追加・変更した場合はこのファイルを同時に更新する。

## 共通応答ルール

- parse成功しキューへ投入できた行は`ACK QUEUED <command>`(LED系は投入のみでACKなし、`ABORT`/`JOB_ABORT`は`ACK QUEUED`または`ACK <name> requested`)
- parse失敗は`ERROR: ...`
- job実行中(`RUNNING`)に許可されないコマンドは`REJECT: command <name> not allowed while job_state=...`
- motion側で拒否された`XY`/`G0`/`G1`は`NACK_XY target=(x,y) reason=...`
- `GCODE`/`XY`/`AB_TIMED`/`JOB_END`はCommandQueue満杯でも破棄せず、投入成功まで待つ

## 診断・bring-up系

| コマンド | 引数 | 成功応答 | 失敗応答 | 前提条件 |
|---|---|---|---|---|
| `HELP` | なし | コマンド一覧を複数行表示 | - | なし |
| `CONFIG` | なし | ピン、motion定数、homing、job設定等を複数行表示 | - | なし |
| `POS` | なし | `POS X=... Y=... A=... B=... HOMED=... ALARM=...` | - | なし |
| `ZERO` | なし | `ZERO logical origin reset; this is not homing`。homedを無効化 | - | job中不可 |
| `TEST_A <steps>` | steps(int32) | 完了までブロック。homedを無効化 | `ERROR: backend rejected TEST_A` | job中不可。bring-up専用 |
| `TEST_B <steps>` | steps(int32) | 同上 | 同上 | 同上 |
| `AB_TIMED <a_steps> <b_steps> <duration_us>` | int16範囲のsteps、`duration_us >= 1000` | queue前後の診断ログ+`ACK_AB_TIMED a_steps=... b_steps=... duration_us=...` | `NACK_AB_TIMED reason=abort\|alarm\|backend_not_ready\|no_steps\|duration_too_short\|steps_out_of_range\|queue_*\|start_error\|stopped` | TMC自動init。alarm中不可。job中不可。診断専用(planner/kinematicsをバイパス) |
| `XY <x_mm> <y_mm> [feed_mm_min]` | feed省略時`DEFAULT_FEED_MM_MIN` | `ACK_XY target=(x,y) A=... B=... F=...`(ゼロ距離はno-opでACK) | `NACK_XY target=(x,y) reason=rejected\|abort\|alarm\|planner\|planner_queue_full\|segment\|backend` | TMC自動init。homed必須(`HOMING_REQUIRE_HOMED_FOR_XY_MOVE`)。soft limit内。alarm中不可。job中はG-code由来のみ許可 |
| `SELFTEST` | なし | `SELFTEST PASS` | `SELFTEST FAIL current=... expected=...` | なし |
| `MELODY` | なし | メロディ再生。TMC profileを一時変更し完了後復元 | 実行不可条件はログ表示 | motion idle、alarmなし、TMC ready。job中不可。診断専用 |

## Homing / Safety系

| コマンド | 引数 | 成功応答 | 失敗応答 | 前提条件 |
|---|---|---|---|---|
| `HOME` | なし | X→Y順にhoming。完了で`HOMED=YES` | alarm遷移+LED ERROR | TMC自動init。job中不可(`JOB_BEGIN`の自動HOMEを除く) |
| `HOME_X` / `HOME_Y` | なし | 単軸homing | 同上 | 同上 |
| `HOME_STATUS` | なし | `HOME_STATUS state=... homed=... limitX=...` | - | なし |
| `LIMIT_STATUS` | なし | `LIMIT_STATUS X_RAW=... X_DEBOUNCED=... Y_RAW=... Y_DEBOUNCED=...` | - | なし |
| `ALARM_CLEAR` | なし | `ALARM_CLEAR complete`。limit ON中の解除はhomedを無効化 | - | job中不可 |
| `ABORT` | なし | 即時停止要求+`ABORT complete`。alarm遷移、homed無効化 | - | 常時可(commandTaskで即時flag) |

## Job Lifecycle系

| コマンド | 引数 | 成功応答 | 失敗応答 | 前提条件 |
|---|---|---|---|---|
| `JOB_BEGIN` | なし | `JOB_BEGIN OK seq=... homed=YES tmc=READY pen=UP` | `JOB_BEGIN rejected reason=job_not_idle\|*_not_empty\|tmc_not_ready\|not_homed\|auto_home_failed\|<alarm>` | queue空。TMC自動init。未homedなら自動HOME(`JOB_BEGIN_AUTO_HOME`) |
| `JOB_END` | なし | pen up→park移動→ジングル→`JOB_END OK seq=...` | `JOB_END rejected reason=no_active_job`、`JOB_END failed reason=queue_not_empty\|park_failed\|jingle_failed` | job実行中。queue空 |
| `JOB_ABORT` | なし | `JOB_ABORT requested`+`JOB_ABORT complete`。job状態`ABORTED`、alarm遷移 | `JOB_ABORT rejected reason=no_active_job` | job実行中のみ |
| `JOB_STATUS` | なし | `JOB_STATUS state=... result=... last_error=... seq=...` | - | なし |

## TMC / Pen系

| コマンド | 引数 | 成功応答 | 失敗応答 | 前提条件 |
|---|---|---|---|---|
| `TMC_INIT` | なし | TMC A/B初期化。`tmc_ready`更新 | 初期化失敗ログ | なし |
| `TMC_STATUS` | なし | 両ドライバのconnection、IFCNT、microsteps、電流等 | - | なし |
| `PENUP` | なし | `PEN UP` | - | job中はG-code `M5`のみ許可 |
| `PENDOWN` | なし | `PEN DOWN` | - | job中はG-code `M3`のみ許可 |

## LED系

LED系はcommandTaskから`LedCommandQueue`へ直接投入され、motionと独立に常時実行できる。
応答は`LedPatternEngine`からの`OK: ...`。queue満杯時は`ERROR: LedCommandQueue full`。

| コマンド | 引数 | 応答 |
|---|---|---|
| `LED <r> <g> <b>` | RGB 0..255 | `OK: LED r=... g=... b=...`。手動モードへ移行 |
| `LED_PIXEL <index> <r> <g> <b>` | index 0..`NEOPIXEL_LED_COUNT-1` | `OK: LED_PIXEL index=...`。範囲外は`ERROR:` |
| `LED_OFF` | なし | `OK: LED_OFF`。手動モードへ移行 |
| `LED_PATTERN <pattern>` | `OFF\|SOLID\|PACIFICA\|FIRE\|BREATH\|CHASE\|PROGRESS\|ALERT\|SUCCESS` | `OK: LED_PATTERN <pattern>`。手動モードへ移行 |
| `LED_BRIGHTNESS <value>` | 0..`NEOPIXEL_BRIGHTNESS_MAX` | `OK: LED_BRIGHTNESS <value>` |
| `LED_PARAM <name> <value>` | `BRIGHTNESS\|HUE\|SATURATION\|SPEED\|INTENSITY\|COOLING\|SPARKING`、0..255 | `OK: LED_PARAM <name> <value>` |
| `LED_AUTO <0\|1>` | 0=手動、1=状態連動 | `OK: LED_AUTO <0\|1>` |
| `LED_STATUS_SET <status>` | `IDLE\|HOMING\|DRAWING_PEN_UP\|DRAWING_PEN_DOWN\|PROCESSING\|PAUSED\|COMPLETED\|WARNING\|ERROR` | `OK: LED_STATUS_SET <status>`。自動表示へ強制復帰し指定状態を表示 |
| `LED_STATUS` | なし | `LED_STATUS mode=... status=... pattern=... brightness=...` |

## G-code

先頭が`G`/`M`の行は`GcodeParser`→`GcodeInterpreter`で解釈される。1行1コード。
Z軸、arc(G2/G3)、checksum、`G4 S`秒指定は未対応。

| G-code | 変換先 | 備考 |
|---|---|---|
| `G0 [X] [Y] [F]` | `XY`経路(rapid相当) | modal単位/座標モードとmodal feedを適用 |
| `G1 [X] [Y] [F]` | `XY`経路(linear feed) | 同上 |
| `G4 P<ms>` | `DWELL` | motionTaskで指定ms待つのみ。`DWELL P=...ms`をログ |
| `G20` | modal更新 | X/Y入力をinch解釈(mmへ変換)。`F`はmm/minのまま |
| `G21` | modal更新 | X/Y入力をmm解釈 |
| `G28` | `HOME`経路 | X→Y順のhoming |
| `G90` | modal更新 | absolute positioning |
| `G91` | modal更新 | relative positioning |
| `M3` | `PENDOWN`経路 | `PEN DOWN`をログ |
| `M5` | `PENUP`経路 | `PEN UP`をログ |
| `M114` | `POS`経路 | 現在位置表示 |

modal状態(単位、absolute/relative)は`JOB_BEGIN`でMM/ABSOLUTE/デフォルトfeedへリセットされる。

## job実行中(`RUNNING`)の許可コマンド

- 常時許可: `GCODE`(全G-code)、`JOB_END`、`JOB_ABORT`、`ABORT`、`JOB_STATUS`、`POS`、LED系
- G-code由来のみ許可: `XY`(G0/G1)、`DWELL`(G4)、`PENUP`/`PENDOWN`(M5/M3)
- 拒否: 裸の`XY`、`TEST_A/B`、`AB_TIMED`、`MELODY`、`ZERO`、`ALARM_CLEAR`、`TMC_INIT`、`HOME`系
