# SPEC.md

# M5Stack Core2 CoreXYペンプロッタ ファームウェア仕様書

Version: 0.2  
Status: Draft for Codex implementation  
Target: M5Stack Core2 / ESP32 / PlatformIO / Arduino framework  
Stepper backend: FastAccelStepper  
Motor driver: TMC2209 x2, STEP/DIR + UART shared bus

---

## 1. 目的

この仕様書は、M5Stack Core2を用いたCoreXYペンプロッタ用ファームウェアの仕様を定義する。

このファームウェアは、最初から高機能CNCファームを目指さない。  
まず以下を安全に実現する。

- Core2上で安全に起動する
- ピン割り付けを明確化する
- 2Coreを分担して使う
- CoreXY変換を正しく行う
- TMC2209をSTEP/DIRで駆動する
- TMC2209をUARTアドレス違いで設定する
- SIMULATION_MODEでモータを動かさず検証する
- 将来planner、look-ahead、junction deviationを追加できる構造にする

---

## 2. 非目的

初期版では以下を目的にしない。

- GRBL完全互換
- FluidNC完全互換
- Klipper完全互換
- G-code完全対応
- G2/G3円弧補間
- jerk-limited S字加減速
- input shaping
- SDカード実行
- WebUI
- 高速描画
- 外部MCU分離

これらは将来拡張の候補であり、初期実装には含めない。

---

## 3. ハードウェア仕様

| 項目 | 内容 |
|---|---|
| コントローラ | M5Stack Core2 |
| MCU | ESP32 |
| 表示 | Core2内蔵LCD |
| 入力 | Touch / Button / Serial |
| モータ構成 | CoreXY A/B 2モータ |
| モータドライバ | TMC2209 x2 |
| モータ駆動 | STEP/DIR |
| ドライバ設定 | UART共通バス |
| ペン制御 | Servo PWM |
| X/Yリミット | GPIO入力 |
| 状態LED | 外付けNEOPIXEL。灯数は設定可能 |
| 開発環境 | PlatformIO + Arduino framework |
| ステッパライブラリ | FastAccelStepper |

---

## 4. ピン割り付け

初期ピン割り付けは以下とする。

| 用途 | 信号 | GPIO | 備考 |
|---|---|---:|---|
| AモータSTEP | `MOTOR_A_STEP_PIN` | 25 | CoreXY A = X + Y |
| AモータDIR | `MOTOR_A_DIR_PIN` | 26 | CoreXY A = X + Y |
| BモータSTEP | `MOTOR_B_STEP_PIN` | 27 | CoreXY B = X - Y |
| BモータDIR | `MOTOR_B_DIR_PIN` | 19 | CoreXY B = X - Y |
| A/B共通EN | `EN` | GND固定 | Low active。GPIO制御しない |
| TMC UART TX | `TMC_UART_TX_PIN` | 14 | Serial2 TX |
| TMC UART RX | `TMC_UART_RX_PIN` | 13 | Serial2 RX |
| Xリミット | `X_LIMIT_PIN` | 36 | 入力専用。外付けpull-up推奨 |
| Yリミット | `Y_LIMIT_PIN` | 35 | 入力専用。外付けpull-up推奨 |
| ペンサーボ | `PEN_SERVO_PIN` | 32 | PWM |
| NEOPIXEL | `NEOPIXEL_PIN` | 33 | 外付けNEOPIXEL。データ入力 |

TMC2209 A/Bの`EN`はGND固定で常時activeとする。
電気的な遮断が必要な場合は、外部スイッチ、E-stop回路、またはI/O拡張を追加する。

---

## 5. 使用禁止・注意ピン

| GPIO | 理由 |
|---:|---|
| 21 / 22 | Core2内部I2C。AXP192、RTC、IMU、Touch等で使用 |
| 1 / 3 | USB Serial |
| 34 / 35 / 36 / 38 | 入力専用系。出力不可 |
| 0 / 2 | boot strapに関係するため原則避ける |

GPIO35/36は入力専用としてリミットスイッチに使う。  
出力用途に使ってはいけない。

---

## 6. TMC2209 UART仕様

TMC2209は2個を同じUARTバスに接続し、アドレス違いで制御する。

```text
GPIO14 TX ---- 1kΩ ----+---- TMC2209 A PDN_UART
                       |
                       +---- TMC2209 B PDN_UART
                       |
GPIO13 RX -------------+
```

| ドライバ | UARTアドレス | 役割 |
|---|---:|---|
| TMC2209 A | 0 | CoreXY A |
| TMC2209 B | 1 | CoreXY B |

UART設定:

| 項目 | 値 |
|---|---:|
| UART | Serial2 |
| TX | GPIO14 |
| RX | GPIO13 |
| Baud | 115200 |
| A address | 0 |
| B address | 1 |

注意:

- UARTは設定と診断用
- モータのリアルタイム駆動はSTEP/DIRで行う
- UARTをモーション中に高頻度で読まない
- SG_RESULT等のログは明示的な診断モードだけで使う
- 各TMC2209モジュールのMS1/MS2またはジャンパでアドレスを設定する

---

## 7. Core割り付け仕様

| Core | 担当 |
|---:|---|
| Core 0 | UI、LCD、Touch、Button、Serial入力、ログ |
| Core 1 | Motion、Safety、Stepper、TMC2209 UART、Pen制御 |

### Core 0

Core 0はユーザーインタフェース側を担当する。

- `uiTask`
- `commandTask`
- `logTask`

### Core2 LCD / Touch UI

Core2内蔵LCDは、Serial未接続でも機械状態と安全状態を確認し、bring-up操作を行えるローカルUIとして使う。

UIは黒基調のタッチ向け画面とし、下部タブ、左右フリック、Core2物理A/Cボタンでページを切り替える。

ページ構成:

| ページ | 内容 |
|---|---|
| Status | `READY` / `ALARM` / `NEED HOME` / `HOMING`、X/Y座標、pen、TMC、home状態 |
| Control | `HOME`、`ALARM_CLEAR`、上下左右1mm jog、`PENUP`、`PENDOWN` |
| Detail | homing詳細、feed、A/B step、limit debounced/raw状態 |

操作ルール:

- UI入力は既存の`CommandMessage`を`command_queue`へ投入し、MotionTaskの既存安全経路を通す
- `HOME`はhoming中でなければUIから実行できる
- `ALARM_CLEAR`はalarm中だけUIから実行できる
- 上下左右jog、`PENUP`、`PENDOWN`は`homed == true`、`alarmed == false`、`homing_active == false`の時だけ有効
- jogは現在座標からの相対操作をUI側で絶対`XY`コマンドに変換し、soft limit内へclampする
- UI操作がqueue満杯、soft limit、home未完了などで実行できない場合は短いnoticeを表示する

描画ルール:

- LCD描画はCore 0の`uiTask`に閉じ込め、Core 1からLCD APIを直接呼ばない
- 状態表示はCore 1から`StatusQueue`で受け取った`StatusMessage`を使う
- LCD更新でmotion、safety、stepper処理をブロックしない
- ちらつき抑制のため、可能な場合は全画面`M5Canvas`へ描画してから`pushSprite()`でLCDへ転送する
- canvas確保に失敗した場合は直接LCD描画へフォールバックする

### Core 1

Core 1はモーション制御側を担当する。

- `motionTask`
- `stepperFeedTask`
- `tmcTask`
- `safetyTask`

---

## 8. タスク仕様

| タスク | Core | 優先度目安 | 役割 |
|---|---:|---:|---|
| `uiTask` | 0 | 1 | LCD表示、Touch/Button |
| `commandTask` | 0 | 2 | Serial入力、CommandMessage生成 |
| `logTask` | 0 | 1 | LogQueue処理 |
| `motionTask` | 1 | 5 | CommandQueue処理、Safety、CoreXY変換 |
| `stepperFeedTask` | 1 | 6 | FastAccelStepper投入、実行管理 |
| `tmcTask` | 1 | 3 | TMC2209初期化、設定、低頻度診断 |
| `safetyTask` | 1 | 4 | limit switch、alarm、E-stop placeholder |

重要:

- STEPパルスはタスクで直接生成しない
- STEPパルス生成はFastAccelStepperに任せる
- LCD描画はCore 0でのみ行う
- モータ制御はCore 1でのみ行う

---

## 9. キュー仕様

| キュー | 方向 | 型 | 目的 |
|---|---|---|---|
| `CommandQueue` | Core 0 → Core 1 | `CommandMessage` | 操作指示 |
| `StatusQueue` | Core 1 → Core 0 | `StatusMessage` | 状態表示 |
| `LogQueue` | Core 1 → Core 0 | `LogMessage` | ログ |
| `MotionQueue` | Core 1内部 | `MotionBlock` | 将来planner |
| `SegmentQueue` | Core 1内部 | `MotionSegment` | 将来timed segment |

Core間で共有変数を直接触る設計は避ける。  
必要な情報はキューまたは明確な同期機構で渡す。

### LCD表示仕様

Core2内蔵LCDには、起動後に機械状態を確認できるステータス画面を表示する。

初期画面の表示項目:

| 項目 | 内容 |
|---|---|
| mode | `SIMULATION` / `REAL` |
| position | 現在の `X` / `Y` 座標 [mm] |
| motor | `ACTIVE (EN=GND)` |
| homing | `HOMED` / `NOT HOMED` |
| pen | `UP` / `DOWN` |
| safety | `READY` / `ALARM` |
| limit | X/Yリミット入力状態 |
| TMC | `READY` / `NOT READY` |

表示更新ルール:

- LCD描画はCore 0の`uiTask`だけで行う
- Core 1の機械状態は`StatusQueue`で受け取る
- `StatusMessage`には表示に必要な状態のスナップショットを含める
- 状態変化時、および低頻度の定期更新で画面を更新する
- LCD更新処理でmotion、safety、stepper処理をブロックしない
- Touch/Buttonによる操作画面は将来拡張とし、初期版は状態表示を優先する

---

## 10. ソフトウェア構造

初期構造:

```text
Serial Input
  -> CommandDispatcher
  -> CommandQueue
  -> motionTask
  -> SafetyManager
  -> CoreXYKinematics
  -> StepperBackendFastAccel
```

将来構造:

```text
CommandInput
  -> GcodeParser
  -> GcodeInterpreter
  -> MachineState
  -> MotionBlockBuilder
  -> SafetyManager
  -> PlannerQueue
  -> TrapezoidPlanner
  -> JunctionPlanner
  -> CoreXYKinematics
  -> SegmentGenerator
  -> StepperBackendFastAccel
  -> FastAccelStepper
```

並列モジュール:

```text
TMC2209Manager
  -> Serial2
  -> TMC2209 A/B
```

```text
PenController
  -> Servo
```

```text
NeoPixelController
  -> external NEOPIXEL LED
```

```text
MotorMelodyController
  -> TMC2209Manager
  -> StepperBackendFastAccel
```

### configファイル仕様

調整可能な設定値はモジュールへ直書きせず、configファイルへ集約する。

| ファイル | 責務 |
|---|---|
| `include/PlotterConfig.h` | motion、safety、pen、LCD、NEOPIXEL、メロディ等の動作パラメータ |
| `include/Core2PinMap.h` | Core2固有のGPIO割り付け、TMC UARTアドレス、UART baud |
| `include/TaskConfig.h` | FreeRTOS taskのCore割り付け、優先度、stack等 |

初期実装ではコンパイル時設定とする。実行時設定保存は将来拡張とする。

---

## 11. 機械定数

初期値:

| パラメータ | 値 |
|---|---:|
| `STEPS_PER_MM` | `80.0f` |
| `COREXY_MAX_MOTOR_GAIN` | `1.41421356237f` |
| `SPEED_SAFETY` | `0.80f` |
| `DEFAULT_FEED_RATIO` | `0.40f` |
| `MAX_MOTOR_SPEED_STEPS_S` | `20000` |
| `MAX_FEED_MM_MIN` | `MAX_MOTOR_SPEED_STEPS_S * 60 / (STEPS_PER_MM * COREXY_MAX_MOTOR_GAIN) * SPEED_SAFETY` |
| `DEFAULT_FEED_MM_MIN` | `MAX_FEED_MM_MIN * DEFAULT_FEED_RATIO` |
| `DEFAULT_MOTOR_SPEED_STEPS_S` | `round(DEFAULT_FEED_MM_MIN * STEPS_PER_MM / 60)` |
| `DEFAULT_ACCEL_MM_S2` | `100.0f` |
| `DEFAULT_MOTOR_ACCEL_STEPS_S2` | `round(DEFAULT_ACCEL_MM_S2 * STEPS_PER_MM * COREXY_MAX_MOTOR_GAIN)` |
| `X_MIN_MM` | `0.0f` |
| `X_MAX_MM` | `55.0f` |
| `Y_MIN_MM` | `0.0f` |
| `Y_MAX_MM` | `55.0f` |
| `SERIAL_BAUD` | `115200` |
| `TMC_UART_BAUD` | `115200` |
| `DIR_CHANGE_DELAY_US` | `200` |
| `SIMULATION_MODE` | `0` |

実機描画ではペンが紙に接触して負荷が増えるため、初期加速度は保守的に設定する。
加速度、feed上限、TMC電流、ペン角度は、脱調、発熱、線品質を見ながら実機で再調整する。

---

## 12. 座標系

### XY座標

ユーザーが扱う座標。

```text
X mm
Y mm
```

### CoreXY A/B座標

モータ空間。

```text
A = X + Y
B = X - Y
```

### step座標

整数ステップ数。

```text
a_steps
b_steps
```

XY座標とA/B stepを曖昧な変数名で混ぜてはいけない。

---

## 13. CoreXY変換仕様

`CoreXYKinematics`は、差分mmからA/B差分stepを求める従来APIに加え、
XY絶対座標からA/B絶対step座標を求めるAPIを持つ。

```cpp
target_a_steps = round((target_x_mm + target_y_mm) * steps_per_mm);
target_b_steps = round((target_x_mm - target_y_mm) * steps_per_mm);

a_steps = target_a_steps - planned_a_steps;
b_steps = target_b_steps - planned_b_steps;
```

通常XY/G-code motionの`MotionBlock`は、ブロック単位の`a_steps`/`b_steps`
だけでなく、`target_a_steps`/`target_b_steps`を保持する。これにより、
微小線分を大量にstreamしても、ブロックごとのfloat差分丸め誤差を累積させず、
端数は次ブロック以降の絶対stepターゲットで自然に回収される。

no-op判定はmm長ではなく`a_steps == 0 && b_steps == 0`で行う。
no-opでも`MachineState`のX/Y指令座標、feed、A/B絶対step座標は整合するよう更新する。

確認ケース:

| Current | Target | Expected A | Expected B |
|---|---|---:|---:|
| `(0,0)` | `(10,0)` | `800` | `800` |
| `(0,0)` | `(0,10)` | `800` | `-800` |
| `(0,0)` | `(10,10)` | `1600` | `0` |
| `(10,0)` | `(10,10)` | `800` | `-800` |
| `(10,10)` | `(0,0)` | `-1600` | `0` |

前提:

```text
STEPS_PER_MM = 80.0
```

---

## 14. MachineState仕様

```cpp
struct MachineState {
  float x_mm;
  float y_mm;

  int32_t a_steps;
  int32_t b_steps;

  float feed_mm_min;

  bool homed;
  bool pen_down;
  bool alarmed;
  bool tmc_ready;
};
```

MachineStateはCore 1のmotion側で管理する。  
Core 0へ表示するときはStatusQueueを通す。

---

## 15. 初期Serialコマンド仕様

| コマンド | 内容 |
|---|---|
| `HELP` | コマンド一覧 |
| `CONFIG` | ピン、定数、Core割り付け表示 |
| `POS` | 現在位置と状態 |
| `ZERO` | 論理原点リセット。homingではない |
| `TEST_A <steps>` | Aモータ単独テスト |
| `TEST_B <steps>` | Bモータ単独テスト |
| `AB_TIMED <a_steps> <b_steps> <duration_us>` | 診断専用。XY/planner/segment生成をバイパスしA/B timed segmentを直接実行 |
| `XY <x_mm> <y_mm> [feed_mm_min]` | 診断/bring-up用XY移動またはsimulation。feed省略時は`DEFAULT_FEED_MM_MIN` |
| `PENUP` | ペン上げ |
| `PENDOWN` | ペン下げ |
| `SELFTEST` | CoreXY変換等の自己診断 |
| `TMC_INIT` | TMC2209初期化 |
| `TMC_STATUS` | TMC2209状態表示 |
| `HOME` | X/Y順のhoming |
| `HOME_X` | X軸homing |
| `HOME_Y` | Y軸homing |
| `HOME_STATUS` | homing状態表示 |
| `LIMIT_STATUS` | limit switch状態表示 |
| `ALARM_CLEAR` | alarm解除 |
| `ABORT` | 実行中motion/homingを停止し、alarmへ遷移 |
| `JOB_BEGIN` | 正式G-codeジョブ開始。TMC自動初期化、開始前確認、pen up、G-code modal resetを行う |
| `JOB_END` | 正式G-codeジョブ終了。pen up、退避移動、終了ジングル、queue drain確認、終了状態ログを行う |
| `JOB_ABORT` | G-codeジョブ文脈つき中断。停止経路は`ABORT`と共通でjob状態を`ABORTED`へ遷移 |
| `JOB_STATUS` | job状態、result、last error、sequenceを表示 |
| `LED <r> <g> <b>` | 外付けNEOPIXEL全灯をRGB指定で点灯 |
| `LED_PIXEL <index> <r> <g> <b>` | 指定indexのNEOPIXELをRGB指定で点灯 |
| `LED_OFF` | 外付けNEOPIXELを消灯 |
| `MELODY` | 診断用モータメロディを明示実行 |

受信応答:

- parseに成功し、対象キューへ投入できたコマンドは`ACK QUEUED <command>`を返す。
- `ABORT`はcommandTaskで即時停止要求flagを立てる。CommandQueueへ投入できない場合も`ACK ABORT requested`を返し、motion/homing側のpollで停止を試みる。
- `JOB_ABORT`はjob中断用であり、job外では`JOB_ABORT rejected reason=no_active_job`を返し低レベル停止は行わない。
- parse失敗または非motion系コマンドのキュー満杯の場合は`ERROR: ...`を返し、`ACK QUEUED`は返さない。
- G-code streamで座標を失うことを避けるため、`GCODE`、`XY`、`AB_TIMED`、`JOB_END`などmotionに関係するコマンドはCommandQueue満杯でも破棄しない。commandTaskは`CommandQueue full; waiting to queue ...`をログし、投入成功まで待ってから`ACK QUEUED`を返す。
- serial入力行が長すぎる場合は`ERROR: input line too long`を出し、その行の残りを次の改行まで読み捨てる。途中から別コマンドとして解釈してはいけない。
- motion側で安全確認または実行投入に失敗した場合は`REJECT: ...`または`ERROR: ...`に加えて、XYでは`NACK_XY ...`を返す。
- 現在位置と同じ座標を指定するゼロ距離`XY`または`G0`/`G1`は、planner/segmentへ投入せずno-opとして扱い、feedを更新して`ACK_XY target=(...) A=0 B=0 F=...`を返す。
- `AB_TIMED`は診断専用であり、`XY`、`CoreXYKinematics`、`TrapezoidPlanner`、`SegmentGenerator`、`SegmentQueue`をバイパスする。`a_steps`、`b_steps`、`duration_us`を直接`StepperBackendFastAccel`のtimed segment経路へ渡す。
- `AB_TIMED`は片側stepが0でも許可するが、A/B両方0、`duration_us < AB_TIMED_MIN_DURATION_US`、alarm中、backend未初期化、backend投入失敗の場合は`NACK_AB_TIMED reason=...`を返す。
- `AB_TIMED`成功時は、queue前後の`micros()`、queue結果、A/B running状態、A/B queue entriesをログし、完了後に`ACK_AB_TIMED`を返す。

脱調や手動停止により論理座標が信用できない状態から再homingする場合は、`ALARM_CLEAR`の前に`ZERO`を実行して現在の論理座標とhomed状態を破棄する。
HOMEを扱うserial check CSVでは、原則として`ZERO -> ALARM_CLEAR -> HOME`の順にする。
HOME開始時またはSeekFast中に対象limitのrawまたはdebouncedがONなら、seek方向へ押し込まず即Backoffへ入る。
BackoffからSeekSlowへ移る条件は、対象limitのrawとdebouncedの両方がOFFになることとする。
HomingのSeekFast、Backoff、SeekSlowは短い固定距離moveの反復ではなく、各フェーズの上限距離ぶんの長いmoveを開始し、limit条件成立時にbackendを停止する。
これにより`HOMING_SEEK_FEED_MM_MIN`と`HOMING_SLOW_FEED_MM_MIN`は、加速距離が足りる範囲で実効速度へ反映される。
停止後のMachineStateはFastAccelStepperのA/B現在ステップ差分から更新する。
通常XY/timed segment実行中も、FastAccelStepperの現在A/Bステップ差分からMachineStateのX/Y概算位置を更新しながらSafetyManagerをpollする。
これにより、ブロック完了前でも原点から離れた位置でlimit activeが継続した場合にhard limit alarmへ入れる。
XY batch完了時には、ジョブ開始、HOME、ZEROなどで揃えたbackend基準からの相対A/B stepとMachineStateの相対A/B stepを比較する。不一致があれば`WARN: DRIFT a=... b=...`をログし、timed segment部分投入や状態更新漏れの再発を検出する。
homing完了直後など、通常移動開始時に原点limitがONで、かつ移動方向がそのlimitから離れる方向の場合は、`NORMAL_MOVE_LIMIT_RELEASE_MM`の範囲だけlimit ONを一時許容する。
この範囲内にraw/debouncedがOFFへ戻れば通常移動を継続し、OFFへ戻らない場合は`X home limit did not release`または`Y home limit did not release`でalarm停止する。
`ABORT`で停止した場合は実位置が論理座標と一致する保証がないため、homed状態を無効化しalarm状態にする。復旧は`ZERO -> ALARM_CLEAR -> HOME`の順に行う。

正式な描画入力はG-codeを基本とする。`XY`、`TEST_A`、`TEST_B`、`AB_TIMED`、`MELODY`はbring-up、診断、切り分け用コマンドとして扱い、通常ジョブの外部インターフェースにはしない。
内部実装として`G0`/`G1`が当面`XY`経路へ変換されることは許容するが、外部互換性はG-code側で維持する。

---

## 15.1 Serial Tool CSV送信仕様

`tools/serial_tool/serial_send.py`はCSV行を順番にファームウェアへ送る検査用ツールである。
ファームウェア仕様をPython側で重複実装せず、応答待ちと期待文字列確認だけを行う。

待ち時間の扱い:

- `--startup-delay`: serial port open後、最初のコマンド送信前に固定で待つ時間
- `--startup-drain`: `--startup-delay`後に起動ログを読み捨てる最大時間
- `--timeout`: 各コマンド応答の最大待ち時間。既定値は30秒
- `--motion-timeout-margin`: Serial Toolが推定したmotion時間へ足す余裕時間。既定値は5秒
- `--estimate-feed-mm-min`: feed未指定motionのホスト側timeout推定に使うfeed。既定値は1200mm/min
- `--no-auto-motion-timeout`: XY/G-code motion時間によるtimeout自動延長を無効化する
- CSV `delay_ms`: 各コマンド送信後の最小読み取り時間
- 各行の最大待ち時間は`max(delay_ms, --timeout, 推定motion時間 + --motion-timeout-margin)`
- 推定対象はCSV `XY`、G-code `G0`/`G1`、`G4`である。G-codeでは`G20`/`G21`、`G90`/`G91`、modal feedを追跡する。ファームウェア仕様に合わせ、`G20`時も`F`はmm/minのまま扱い、X/Yだけinchからmmへ変換する
- `--queue-mode`のcompletion待ちは、上記に加えて`HOME`、`G28`、`JOB_BEGIN`、`JOB_END`、`M3`、`M5`などのコマンド別下限timeoutも考慮する
- `expect`が指定されている場合、`delay_ms`経過後に`expect`を受信済みで、受信が短時間idleになったら次の行へ進む
- `--queue-mode`: 各行を送信後、`ACK QUEUED`または`ACK ABORT requested`を受信してから次行へ進む
- `--queue-mode`中に`ERROR: CommandQueue full`を受信した場合は、同じ行を`--queue-retry-delay-ms`間隔で再送する
- `--queue-mode`でも`HOME`、`HOME_X`、`HOME_Y`は後続motionを先に積まないよう、queue投入後に完了ログまで待つ
- `--stream-xy-motion`: `--queue-mode`時、CSV由来の`XY`を`ACK QUEUED`確認だけで先行送信し、`ACK_XY target=`完了ログとserial idleを待たない
- `--stream-gcode-motion`または`--stream-xy-motion`では、先行投入したstream motionの推定時間を次の非stream行のtimeoutへ足す。これにより長いtravel move直後の`M3`/`M5`/`JOB_END`が、前段motion完了前にホスト側timeoutになることを避ける
- `Ctrl-C`で中断された場合、serial portを閉じる前に`ABORT`を送信して短時間応答を読む

タイムスタンプ:

- `--startup-delay`と`--startup-drain`完了後、最初のCSVコマンドを送る直前を`t=0.000s`とする
- 各CSV行の送信処理開始時に`TIMING START`を表示する
- 各CSV行の応答待ち終了時に`TIMING END`を表示する
- `TIMING END`には、その行の開始から終了までの経過時間`dt`と`status`を表示する
- `--stream-xy-motion`対象の`XY`は送信速度を優先し、成功時の`TIMING START/END`、`--echo`表示、ACK表示を抑制する

`--timeout`は各行の基準最大待ち時間として使う。長いXY/G-code motionでは推定motion時間と`--motion-timeout-margin`で自動延長する。
起動ログ読み捨て時間には使わない。

---

## 16. `XY`診断コマンド仕様

`XY`はCoreXY変換、SafetyManager、planner、segment、backendの切り分けに使う診断/bring-up用コマンドである。
正式な描画ジョブはG-code入力を基本とし、`XY` CSVは実機調整や低レベル確認用として維持する。

処理順:

1. target X/Yと任意のfeedをparseする。feed省略時は`DEFAULT_FEED_MM_MIN`を使う
2. SafetyManagerでsoft limit確認
3. feed確認。明示feedは速度評価や一時override用として扱う
4. currentからdelta計算
5. CoreXYKinematicsでA/B step算出
6. TrapezoidPlannerで台形または三角加減速profileを生成
7. SegmentGeneratorでA/B同期timed segmentを生成
8. CoreXY変換、trapezoid profile、segment数をログ出力
9. SIMULATION_MODEなら実モータ出力なし
10. Real modeならStepperBackendへtimed segmentを渡す
11. XY移動が受理されたら`ACK_XY target=(x,y) A=a_steps B=b_steps F=feed`を返す
12. 成功時のみMachineState更新

ログ例:

```text
XY target=(10.000,0.000) current=(0.000,0.000) dx=10.000 dy=0.000 A=800 B=800 F=3394.113
SIMULATION_MODE: no motor output
```

### 台形加減速仕様

`TrapezoidPlanner`は1本のXY線分ごとに以下を計算する。

- nominal speed [mm/s]
- acceleration [mm/s^2]
- acceleration distance
- cruise distance
- deceleration distance
- acceleration/cruise/deceleration time
- short moveではtriangular profile

加減速profileは`MotionBlock`へ保持する。StepperBackendへplanner処理を入れてはいけない。

### timed segment仕様

`SegmentGenerator`は計画済み`MotionBlock`からA/B同期用の`MotionSegment`列を生成する。
実機ではFastAccelStepperの`moveTimed()`経路でA/Bを同じsegment durationへ投入し、CoreXYの直線性を保つ。

timed segment実行中もSafetyManagerを定期pollし、alarm発生時はStepperBackendを停止する。

`StepperBackendFastAccel::queueTimedSegment()`は、A/B両モーターへのsegment投入を
「両方成功」または「位置を信用しないalarm停止」へ寄せる。A側投入成功後にB側が
`RETRY`を返した場合、B側だけを短時間リトライする。B側が`ERROR`を返した場合、
またはリトライtimeoutに達した場合はbackendを停止し、呼び出し側はbackend現在stepから
`MachineState`を再同期したうえでhomedを無効化し、位置の信頼性が失われたことをログする。

---

## 17. TMC初期化仕様

`TMC_INIT`で以下を行う。

| 項目 | 初期値 |
|---|---:|
| UART | Serial2 |
| TX | GPIO14 |
| RX | GPIO13 |
| baud | 115200 |
| A address | 0 |
| B address | 1 |
| microsteps | 16 |
| interpolation | ON |
| current | 通常profileは1100mAを暫定値とする |
| mode | 初期はspreadCycle寄り |
| diagnostics | 低頻度 |

TMC2209ManagerはFastAccelStepperやplannerに依存しない。

実装では`TMCStepper`を使用し、共有`Serial2`上のアドレス`0`と`1`へ個別に
レジスタ設定を書き込む。`TMC_STATUS`では両ドライバの`test_connection()`、
`IFCNT`、microsteps、RMS current、IRUN/IHOLD、chop mode等を確認できる。

TMC UART profileはCoreXY motionの前提である。A/Bのmicrostepsや電流が揃わない状態で
`XY`、`G0/G1`、`HOME`、`AB_TIMED`を実行すると、ファームウェアが同じA/B step数を
出していても実機の左右移動量がずれ、斜め流れや脱調に見える。したがってMotionTaskは
以下のmotion系コマンド実行前に、`machine_state.tmc_ready`または`TMC2209Manager::isReady()`
がfalseなら`TMC_INIT`相当を自動実行する。

- `XY`
- G-code由来の`G0` / `G1`
- `HOME` / `HOME_X` / `HOME_Y`
- `AB_TIMED`

自動`TMC_INIT`が失敗した場合、該当motionは実行せず`tmc_not_ready`相当で拒否する。

### 診断用モータメロディ仕様

`../1stepper_test`の起動メロディを参考に、ステッパモータのSTEP周波数を音符ごとに変更して短いメロディ風の音を鳴らす。

この機能はbring-upと診断専用とする。CoreXY位置ずれを避けるため起動時に自動実行せず、`MELODY`コマンドでのみ明示実行する。

実行ルール:

- `SIMULATION_MODE=1`ではモータを動かさず、実行不可エラーをログ表示する
- 実行前にmotionがidle、alarmなし、limit入力inactive、TMC UART readyであることを確認する
- A/Bモータは診断対象を明確にして実行する
- 音符ごとにSTEP周波数と長さを指定し、各STEPごとにDIRを反転して移動量を抑える
- 音列は参照実装と同じ`523Hz/90ms`、`659Hz/90ms`、`784Hz/120ms`、`1047Hz/180ms`とする
- STEPパルスは`StepperBackendFastAccel`経由で生成する
- `delayMicroseconds()`によるSTEP直接生成は禁止する
- メロディ中だけTMC2209のmicrosteps、RMS current、chop modeを診断用profileへ一時変更する
- TMC設定変更は`TMC2209Manager`だけが行う
- 完了、中断、alarm、limit検出のいずれでも通常profileへ復元する
- メロディ実行ではMachineStateの論理X/Y位置を更新しない
- 通常motionとメロディを同時実行しない

診断用profileの初期案:

| 項目 | 値 |
|---|---:|
| microsteps | 2 |
| RMS current | 1200mA以下から実機に合わせて決定 |
| chop mode | spreadCycle |
| note gap | 25ms |

通常profileの850mAおよび診断用profileの1200mA以下設定は暫定値であり、使用するモータ、TMC2209モジュール、電源、放熱条件を確認してから確定する。

---

## 18. Safety仕様

初期対応:

- soft limit
- feed validation
- limit switch input
- alarm flag
- emergency stop placeholder

soft limit:

| 軸 | 範囲 |
|---|---|
| X | 0〜55 mm |
| Y | 0〜55 mm |

limit switch:

| 信号 | GPIO | active |
|---|---:|---|
| X_LIMIT | 36 | Low |
| Y_LIMIT | 35 | Low |

GPIO35/36は内部pull-upに頼らず、外付けpull-upを前提とする。

通常移動中、homing完了後の原点外位置でlimit activeが検出された場合はhard limit異常として扱う。
ただし、瞬間的な入力ノイズで即alarmにしないため、debounce後に`HARD_LIMIT_UNEXPECTED_ALARM_MS`で指定した時間だけ継続してactiveの場合にalarmへ入る。
2026-06-08時点の暫定値は20msであり、停止距離とlimit入力ノイズ耐性は実機で再確認する。
homing完了直後の退避移動では、`NORMAL_MOVE_LIMIT_RELEASE_MM`の範囲でhome limit releaseを待つ。2026-06-08時点の暫定値は8mmである。

---

## 19. Pen仕様

初期ペン制御:

| 信号 | GPIO |
|---|---:|
| PEN_SERVO | 32 |

角度初期値:

| 状態 | 角度 |
|---|---:|
| PEN_UP | 60度 |
| PEN_DOWN | 66度 |

ペン下げ角度は紙への押し付け力に直結する。
脱調や引っかかりがある場合は、まずペン圧を下げる方向で`PEN_DOWN_ANGLE_DEG`を調整する。

将来的にM3/M5へ接続する。

---

## 19.1 NEOPIXEL LED仕様

外付けNEOPIXELを`GPIO33`へ接続し、状態表示とbring-up確認に使う。LED数は固定せず、configで変更可能にする。

初期対応:

| 項目 | 内容 |
|---|---|
| 信号 | `NEOPIXEL_PIN = 33` |
| LED数 | `NEOPIXEL_LED_COUNT`でビルド時に任意設定 |
| 初期状態 | 消灯 |
| 手動操作 | `LED <r> <g> <b>` / `LED_PIXEL <index> <r> <g> <b>` / `LED_OFF` |
| 更新元 | Core 0のUI側 |

実装ルール:

- NEOPIXEL制御は`NeoPixelController`に閉じ込める
- Core 1からNEOPIXEL APIを直接呼ばない
- 状態連動表示を追加する場合はCore 1から`StatusQueue`で受け取った状態を使う
- LED更新でmotion、safety、stepper処理をブロックしない
- 輝度上限を設定し、初期値は低輝度にする
- `GPIO33`を他用途と同時使用しない
- `LED`は全灯一括、`LED_PIXEL`はindex指定で更新する
- indexは`0 <= index < NEOPIXEL_LED_COUNT`の範囲で検証する
- 灯数に応じて電源容量と配線を確認する

---

## 20. Host WebUI仕様

Host WebUIはPCまたはRaspberry Pi上で動作し、USB Serial経由でM5Stack Core2ファームウェアを制御する。
ESP32上でHTTP serverやWebSocket serverを動かす方式は初期WebUIの対象外とする。

目的:

- Serial未接続のCore2 LCDだけでは見づらい状態、ログ、ジョブ操作をPC画面で確認する
- G-code送信を既存`tools/serial_tool/serial_send.py`へ委譲し、送信・ACK待ち・Job Lifecycleの既存挙動を再利用する
- G-code previewを送信前に表示し、soft limit超過や想定外のペン動作を事前確認しやすくする

構成:

```text
Browser
  -> Host WebUI client
  -> Host bridge server
  -> tools/serial_tool/serial_send.py
  -> USB Serial
  -> M5Stack Core2 firmware
```

初期実装ではHost bridge serverがSerial port列挙、状態取得、ログ配信、ジョブ送信プロセス管理、G-code preview用解析を担当する。

実装配置:

| Path | 役割 |
|---|---|
| `tools/webui/server.py` | Python標準ライブラリベースのHost bridge server |
| `tools/webui/static/index.html` | WebUI shell |
| `tools/webui/static/styles.css` | Product Design方針に沿ったdark machine UI |
| `tools/webui/static/app.js` | Client state、SSE log、manual command、G-code preview |
| `tools/webui/svg_to_gcode.py` | Host側SVG to G-code変換 |
| `tools/webui/README.md` | 起動手順とMVP範囲 |

### 20.1 画面構成

| 画面 | 役割 |
|---|---|
| Dashboard | 接続状態、`READY` / `ALARM` / `NEED HOME` / `HOMING`、X/Y、pen、homed、limit、TMCを表示する |
| Manual Control | `HOME`、`ALARM_CLEAR`、`PENUP`、`PENDOWN`、上下左右jogを提供する |
| Job | G-codeファイル選択、G-code preview、`JOB_BEGIN`/送信/`JOB_END`、`JOB_ABORT`を提供する |
| Console | firmware log、送信行、ACK/NACK/ERROR、手動command入力を表示する |
| Settings | Serial port、baudrate、jog step、送信mode、startup delayなどを設定する |

### 20.2 操作ルール

- WebUIはM5Stack firmwareの安全判定を迂回しない
- Manual jogとpen上下は、Host側表示状態でも`homed == true`、`alarmed == false`、`homing_active == false`の時だけ有効表示にする
- `JOB_ABORT`はジョブ実行中に常に押せる位置へ置く
- Job実行中はmanual jogをdisabledにする
- Host側状態が不明、Serial切断、または状態取得失敗時はmotionを伴う操作をdisabledにする
- 危険操作の最終判定はfirmware側の既存`CommandMessage`/`MotionTask`/`SafetyManager`に委ねる

### 20.3 既存Serial Tool再利用

Job送信は初期実装で`tools/serial_tool/serial_send.py`をsubprocessとして呼び出す。

WebUIから使う既定option:

```text
--gcode <file>
--port <serial_port>
--baud 115200
--queue-mode
--stream-gcode-motion
--job-lifecycle
```

Host bridgeは`serial_send.py`のstdout/stderrをServer-Sent Events等でBrowserへstreamし、`ACK`、`NACK`、`REJECT:`、`ERROR:`、`ALARM=YES`を色分け表示する。
送信ロジック、queue full retry、HOME/JOB_BEGIN/JOB_END timeout、failure時の`JOB_ABORT`送信は`serial_send.py`の責務として維持する。

### 20.3.1 SVG to G-code API

Host bridgeは`POST /api/gcode/from-svg`を提供する。

request:

```json
{
  "svg": "<svg ...>",
  "width_mm": 50,
  "height_mm": 50,
  "margin_mm": 5,
  "feed_mm_min": 800,
  "travel_feed_mm_min": 1200,
  "simplify_tolerance_mm": 0.2,
  "min_stroke_length_mm": 0.5,
  "optimize_stroke_order": true
}
```

response:

```json
{
  "filename": "generated_svg_YYYYMMDD_HHMMSS.gcode",
  "gcode": "G21\nG90\nM5\n...",
  "stroke_count": 0,
  "segment_count": 0,
  "warnings": []
}
```

SVG v1対応要素は`path`、`polyline`、`polygon`、`line`、`rect`、`circle`、`ellipse`とする。
`fill`、gradient、`text`、`image`は描画対象にせず、必要に応じてwarningを返す。
`transform`は`translate`と`scale`を処理し、未対応transformはwarningとして無視する。
Bezier pathは線分近似し、SVG bounding boxを指定`width_mm`/`height_mm`内の`margin_mm`へfitする。
SVGのY下向き座標はプロッタのY上向き座標へ反転する。
1点stroke、M moveだけのpath、`min_stroke_length_mm`未満のstrokeは削除する。
`optimize_stroke_order`有効時は最近傍端点順に並べ、必要ならstrokeを反転する。
生成G-codeは必ず`G21`、`G90`、`M5`で開始し、各strokeを`M5`、pen-up `G0`、`M3`、`G1...`、`M5`で出力する。
不要な`G0 X0 Y0`は出力しない。

### 20.3.2 Image to G-code API

ユーザー向けJob画面では、従来のSVG to G-codeパネルを`Image to G-code`として表示する。
同一パネルから`.svg`、`.png`、`.jpg`、`.jpeg`をアップロードできる。
Generator群の表示順は`Text Generator`、`QR Generator`、`Image to G-code`とし、いずれも折りたたみ可能にする。

内部責務は以下に分離する。

```text
trace_raster_image_to_svg(...)
  PNG/JPEG -> plotter-friendly SVG
svg_to_gcode(...)
  SVG -> G-code
convert_image_to_gcode(...)
  入力種別を判定し、SVGはsvg_to_gcode、PNG/JPEGはtrace_raster_image_to_svg後にsvg_to_gcode
```

Host bridgeは`POST /api/gcode/from-image`を`multipart/form-data`で提供する。

request fields:

```text
file
width_mm
height_mm
margin_mm
feed_mm_min
travel_feed_mm_min
simplify_tolerance_mm
min_stroke_length_mm
optimize_stroke_order
trace_mode
threshold_mode
threshold_value
invert
skeletonize
max_segments
```

response:

```json
{
  "filename": "generated_image_YYYYMMDD_HHMMSS.gcode",
  "gcode": "G21\nG90\nM5\n...",
  "intermediate_svg": "<svg ...>",
  "input_type": "svg",
  "stroke_count": 0,
  "segment_count": 0,
  "warnings": []
}
```

PNG/JPEGは直接G-code化せず、必ず中間SVGを生成してからSVG to G-code処理へ通す。
中間SVGは`viewBox`を持ち、`fill="none"`、`stroke="black"`を基本に、`polyline`中心の単純な線画とする。
1点stroke、孤立点、短すぎるstrokeは含めない。
Line Artはしきい値化後に必要に応じてskeletonizeし、中心線に近いpolylineを出す。
Outline Traceは黒領域の境界をpolyline化する。
`max_segments`を超える場合はエラーとして返し、WebUI上に表示する。
WebUI既定値は`max_segments=12000`とし、細かいラスタ線画が初期設定で過度に失敗しないようにする。
Image to G-codeパネルは、upload、PNG/JPEG trace、SVG to G-code、layout追加の進行状態を表示する。
変換失敗時は失敗ステータスと具体的なエラー文をパネル内およびConsole logへ表示する。

### 20.4 G-code preview

G-code previewはMVPに含める。

初期preview対象:

- `G0` / `G1` のXY直線
- `G20` / `G21` の単位切替
- `G90` / `G91` のabsolute/relative切替
- `M3` / `M5` または`PENDOWN` / `PENUP`相当のpen状態
- `G28`はhome markerとして扱い、描画pathには含めない
- `G4`はdwell markerとして扱い、描画pathには含めない

preview表示:

- soft limit矩形を表示する
- pen down pathとpen up travelを色分けする
- file boundsを表示する
- soft limit外のsegmentを警告表示する
- parserが未対応のG-codeを検出した場合は警告一覧へ出す

previewは送信前確認用であり、firmware側plannerやsafety判定の代替にはしない。

---

## 21. FastAccelStepper仕様

FastAccelStepperは`StepperBackendFastAccel`のみが直接使用する。

初期の`moveABSteps()`はbring-up用であり、厳密なXY補間を保証しない。

将来的には以下へ移行する。

```text
MotionBlock
  -> PlannerQueue
  -> TrapezoidPlanner
  -> JunctionPlanner
  -> SegmentGenerator
  -> StepperBackendFastAccel moveTimed or low-level queue
```

---

## 20.1 Look-ahead / JunctionPlanner仕様

Phase 10では、motionTaskが連続して受信できた`XY`を短いバッチとして`PlannerQueue`へ積み、`JunctionPlanner`が複数`MotionBlock`のentry/exit speedを設定する。

設定値:

| 項目 | 意味 |
|---|---|
| `JUNCTION_DEVIATION_MM` | junction deviation許容値。小さいほど角で減速する |
| `CLASSIC_JERK_LIMIT_MM_S` | classic jerk相当の簡易上限。0以下なら無効 |
| `LOOKAHEAD_BATCH_COLLECT_MS` | motionTaskが連続XYを待つ最大時間 |

実装ルール:

- `JunctionPlanner`は`StepperBackendFastAccel`やFastAccelStepperに依存しない
- look-aheadは`PlannerQueue`内の複数`MotionBlock`だけを対象にする
- reverse passとforward passで、各ブロック長と加速度から到達可能なentry/exit speedへ制限する
- 最初のblock entry speedと最後のblock exit speedは0とする
- CoreXY motor-space速度上限は、最終的な`SegmentGenerator`のtimed segment検査で守る
- `PENUP`、`PENDOWN`、`HOME`などXY以外のコマンド順序を崩さない
- `LOOKAHEAD_BATCH_COLLECT_MS`は実機確認用の暫定値であり、角の丸まり、閉じズレ、コマンド応答性を見て調整する

`CONFIG`はjunction deviation、classic jerk、batch collect時間、PlannerQueue容量を表示する。

---

## 21. 最小G-code仕様

Phase 7では、完全なG-code互換ではなく、既存のSerialコマンドへ安全に接続できる
最小G-codeを実装する。

正式な描画入力はG-codeを基本とする。Text Tool、QR Tool、将来のSD/Web/USB streamingは、最終的にG-codeをファームウェアへ渡す経路へ寄せる。
`XY`などの独自Serialコマンドは、通常運用ではなく診断/bring-up用の低レベルAPIとして扱う。

| G-code | 意味 |
|---|---|
| G0 | 既存`XY`経路を使うrapid相当移動 |
| G1 | 既存`XY`経路を使うlinear feed移動 |
| G4 | `P`ミリ秒のdwell |
| G20 | X/Y入力単位をinchへ切替 |
| G21 | X/Y入力単位をmmへ切替 |
| G28 | 既存`HOME`経路へ接続 |
| G90 | absolute positioning |
| G91 | relative positioning |
| M3 | 既存`PENDOWN`経路へ接続 |
| M5 | 既存`PENUP`経路へ接続 |
| M114 | 既存`POS`経路へ接続 |

F値はmm/minとして扱う。

実装ルール:

- `GcodeParser`は文字列を`ParsedGcode`へ変換するだけで、motionを直接実行しない
- `GcodeInterpreter`はmotion core側で`MachineState`を参照し、absolute/relativeと単位を解決する
- `G0`/`G1`は既存`XY`コマンドへ変換し、SafetyManager、PlannerQueue、TrapezoidPlanner、JunctionPlanner、SegmentGeneratorを通る
- `G4 P<ms>`はmotion taskで指定ミリ秒待つだけで、plannerやstepper backendへ入れない
- `G20`はX/Y入力値だけをinchからmmへ変換し、`F`はmm/minのまま扱う
- 1行に複数のG/Mコードは対応しない
- Z軸、arc、checksum検証、modal motion continuation、`G4 S`秒指定、完全なGRBL互換は対象外とする

---

## 21.0 Job Lifecycle仕様

正式版では、G-codeファイル本文に毎回起動処理や終了処理を書かせない。
描画ジョブの開始/終了処理はファームウェア側のJob Lifecycleで扱う。

初期方針:

- 電源投入時は安全なidle状態へ初期化するだけで、自動homingや自動移動は行わない
- bring-upでは`SELFTEST`、`TMC_INIT`、`ZERO`、`ALARM_CLEAR`、`HOME`などを明示実行してよい
- 正式ジョブ開始時は、alarm、TMC ready、homed、pen状態、motion queue状態をファームウェア側で確認する
- `JOB_BEGIN`はTMC未readyの場合に`TMC_INIT`相当を自動実行する。TMC初期化に失敗した場合だけ`tmc_not_ready`で拒否する
- `JOB_BEGIN_AUTO_HOME=false`では、homedでない場合は`JOB_BEGIN`を`not_homed`で拒否し、自動homingは行わない
- `JOB_BEGIN_AUTO_HOME=true`では、`JOB_BEGIN`時に未homedならTMC初期化後に`HOME`相当を自動実行する。HOME失敗時は`auto_home_failed`で拒否し、G-code本文は実行しない
- `JOB_BEGIN`の開始前確認で`not_homed`、`tmc_not_ready`、queue not empty等を拒否した場合、job状態は`IDLE`へ戻し、次の`JOB_BEGIN`を再試行できるようにする
- `FAILED`/`ABORTED`に残っている場合でも、alarmが解除済みなら次の`JOB_BEGIN`前に`IDLE`へ復帰できる。alarm中は復帰しない
- `JOB_ABORT`はCommandTask受信時点でmotion abort flagを立て、homing中や長いmotion処理中でも停止要求を伝える。motionTaskでjob状態を`ABORTED`へ遷移し、homedを無効化する
- `JOB_BEGIN`成功時は、その時点でhomed確認済みであることをJobControllerに保持し、`RUNNING`/`ENDING`中のG-code由来XY移動のhomedゲートにも使う。`JOB_ABORT`、`ABORT`、`FAILED`、`JOB_END`完了時はこの保持状態を破棄する
- 正式ジョブ終了時は、pen up、`JOB_END_PARK_X_MM`/`JOB_END_PARK_Y_MM`への退避移動、終了ジングル、queue drain、status/log出力をファームウェア側で行う
- G-code本文には描画内容に関係する`G0`/`G1`、`M3`/`M5`、`G4`などを残し、`SELFTEST`、`TMC_INIT`、`ALARM_CLEAR`、`LIMIT_STATUS`、`ZERO`、`CONFIG`は通常含めない
- `RUNNING`中はG-code由来の`XY`、`DWELL`、`PENUP/PENDOWN`相当だけを許可し、Serial手入力の裸`XY`、`TEST_A/B`、`AB_TIMED`、`MELODY`、`ZERO`、`ALARM_CLEAR`、`TMC_INIT`、`HOME`は拒否する
- `JOB_END`はSerial G-code streamの最後にホストが明示送信する。ファームウェアはSerial上のG-codeファイル終端を自動検出しない
- 初期退避位置は`X=5.0mm`、`Y=Y_MAX_MM - 5.0mm`とする。終了ジングルはA/B両モータを交互方向pulseで鳴らす短いオリジナル8-bit風和音であり、MachineStateの論理座標は更新しない

`tools/serial_tool/examples/gcode_preamble.csv`は、正式Job Lifecycle実装までの暫定bring-up/実機確認手順として扱う。

---

## 21.1 KST32B Text Tool仕様

ホスト側ツール`tools/text_tool/kst32b_to_gcode.py`は、KST32BのCSF/1ストロークフォントデータを読み、文字列をプロッタ用G-codeへ変換する。

実装ルール:

- フォント本体はリポジトリへ同梱せず、`--font tools/text_tool/fonts/KST32B.TXT`で指定する
- 入力は`--text`またはUTF-8の`--input-file`のどちらか一方とする
- 出力は`G21`、`G90`、`G0`、`G1`、`M3`、`M5`、必要に応じて`G4 P<ms>`を使う
- KST32Bの30x32格子座標を`--size`の文字高さmmへスケーリングする
- CSF/1のX move命令は、現在Yを維持したペンアップ移動として即時に新しいストローク開始位置へ反映する。次のY命令まで遅延させると、`高`の上点、ASCIIの`H`/`L`/`l`などが斜め線へ化けるため禁止する
- `--max-x`、`--max-y`を指定した場合、生成座標が範囲外ならエラーにする
- `--auto-scale-to-fit`を指定した場合、生成座標が`--max-x`/`--max-y`内へ収まるように`--size`を自動縮小する
- ペンアップ移動は`G0`、描画移動は`G1`、ペンダウンは`M3`、ペンアップは`M5`で表す
- 濁点、半濁点、小さい文字などの短い線分を削除しない
- 線分簡略化、字形補正、SVG変換、vpype連携、G2/G3円弧補間は行わない
- 未対応文字は警告を出し、既定では代替四角形、`--missing-glyph skip`ではスキップする
- 直前の物理位置と同じ座標へのペンアップ`G0`は出力しない。ファームウェア側plannerのゼロ長XY拒否を避けるためであり、短い描画線分は削除しない
- 生成したG-codeの実機品質はペン先径、紙質、ペン上下dwell、feed、機械剛性で調整する

`tools/serial_tool/serial_send.py`は`--gcode`でG-codeファイルを直接送信できる。空行、`;`開始コメント行、`%`行は送信せず、その他の行を1行1コマンドとして扱う。
描画前の暫定bring-up手順は`--preamble-csv tools/serial_tool/examples/gcode_preamble.csv`で前置できる。標準preambleは`SELFTEST`、`ZERO`、`ALARM_CLEAR`、`LIMIT_STATUS`、`G28`、`POS`を送り、alarm解除、limit状態、homing完了、`HOMED=YES`を確認する。正式版ではこの前置手順をホスト側G-code送信からファームウェア側Job Lifecycleへ移す。
`--gcode`で読み込んだ行には、コマンド種別ごとの既定expectを付ける。`G0/G1`は`ACK_XY target=`、`G4`は`DWELL P=`、`M3/M5`は`PEN DOWN`/`PEN UP`などを待つ。
`--gcode --queue-mode --stream-gcode-motion`では、G-codeファイル由来の`G0/G1`に限り、`ACK QUEUED`を確認した時点で次の行を送る。`ACK_XY target=`は後から流れる完了ログとして扱い、後続コマンドの完了判定には使わない。
stream対象の`G0/G1`では、serial idle待ち、成功時の行別`TIMING START/END`、`--echo`表示、ACK表示を抑制し、CommandQueueへ連続XYを投入しやすくする。
このstream modeでも`M3/M5`、`G4`、`G28`、`M114`、`G20/G21/G90/G91`は従来通り完了ログまたはmodalログを待つ。`ERROR:`、`NACK`、`REJECT:`、`ALARM=YES`、`machine is alarmed`は停止条件とする。
期待するqueue ACKまたは完了ログが最大待ち時間内に出ない場合、Serial Toolは`timeout after ... waiting for ...`を表示し、その時点までに受信したSerialログを`timeout partial serial log`として出力する。これはファームウェア拒否ではなくホスト側待ち時間切れとして区別する。
Serial Toolは`NACK`、`REJECT:`、`ALARM=YES`、`machine is alarmed`、`ERROR:`を受信した場合、その行を失敗扱いにして、既定では後続行を送信しない。
`--gcode --job-lifecycle`では、G-code本文の前に`JOB_BEGIN`、最後に`JOB_END`を送る。G-code本文中にfailureを検出した場合は、後続行を送らず`JOB_ABORT`を送る。

---

## 22. 受け入れ条件

初期版は以下を満たすこと。

- buildできる
- `CONFIG`でCore2ピン割り付けが表示される
- `SELFTEST`が通る
- `XY`でCoreXY変換が正しく表示される
- `TMC_INIT`が実装されている、またはplaceholderとして明確に存在する
- FastAccelStepperがStepperBackend内に閉じている
- TMC2209 UARTがTMC2209Manager内に閉じている
- Core 0/1の役割がTaskConfigまたはdocsに明記されている
- `main.cpp`にCoreXY式やFastAccelStepper詳細が直書きされていない
- `SIMULATION_MODE=1`が初期値である
- Core2内蔵LCDにmode、位置、motor、homing、pen、safety、limit、TMC状態が表示される
- 外付けNEOPIXELを`LED` / `LED_PIXEL` / `LED_OFF`コマンドで制御できる
- `MELODY`が通常motionと排他実行され、終了または中断後にTMC通常profileへ復元される
- 台形加減速profileとtimed segmentによるA/B同期XY移動が実行できる
- `center_shapes.csv`が実機で最後まで完走し、最終`POS`で`ALARM=NO`、`LIMIT_X=OPEN`、`LIMIT_Y=OPEN`を確認できる
- 動き出し・動き終わりの歪み調査用に、同一中心へ5個の正方形を重ねて描く反時計回り版`concentric_squares_check.csv`と時計回り版`concentric_squares_clockwise_check.csv`をSerial Toolから実行できる
- 通常版完走後の速度依存性確認用に、feedを省略して`DEFAULT_FEED_MM_MIN`で実行する`concentric_squares_high_speed_check.csv`をSerial Toolから実行できる
- `diagnostic_ab_timed_square_draw.csv`で、PENDOWNした四角描画を`AB_TIMED`直接実行経路で行い、通常XY描画CSVと比較できる
- `G0/G1/G4/G20/G21/G28/G90/G91/M3/M5/M114`の最小G-codeをSerialから受け、既存の安全なmotion/pen/status/homing経路へ接続できる
