# PLANS.md

# M5Stack Core2 CoreXYペンプロッタ 実装計画・進捗管理

Version: 0.3  
Status: Progress-tracking plan for Codex execution  
Target: M5Stack Core2 / ESP32 / PlatformIO / Arduino framework  
Stepper backend: FastAccelStepper  
Motor driver: TMC2209 x2, STEP/DIR + UART shared bus

---

## 0. このファイルの使い方

このファイルは、実装計画であり、同時に進捗管理表である。

Codexは作業完了後に、該当するチェックボックスを更新すること。

チェック状態の意味:

```text
[ ] 未着手
[x] 完了
[-] 一部完了、または保留
[!] 問題あり。修正が必要
```

作業後に必ず更新する項目:

- 「1. 現在地」
- 該当Phaseのチェックリスト
- 「15. 変更履歴」
- 必要なら「14. リスク・未解決事項」

禁止:

- 実装していない項目を完了扱いにしない
- 動いただけで設計上の完了条件を満たしていないものを `[x]` にしない
- エラーを隠して進捗を更新しない
- Phase 7以降の機能を勝手に前倒し実装しない

---

## 1. 現在地

現在の想定フェーズ:

```text
現在地: Phase 0〜10 実装済み。Phase 10 look-ahead / junction deviationは実機未確認。実機bring-upで脱調対策を調整中
```

現在の状態:

| 項目 | 状態 |
|---|---|
| 対象ボード | M5Stack Core2 |
| Core割り付け | 実装済み |
| ピン割り付け | 実装済み |
| TMC2209 UART共通バス | `TMCStepper`でA/Bアドレス別レジスタ設定を実装済み、実機確認完了 |
| CoreXY変換 | 実装済み |
| FastAccelStepper backend | 実装済み、実機確認完了 |
| Core2 LCD Status UI | 実装済み、実機表示確認完了 |
| 外付けNEOPIXEL | 実装済み、実機点灯確認完了 |
| Motor Melody Diagnostics | 実装済み、TMC profile実レジスタ書込み経路あり、実機確認完了 |
| Homing bring-up | `HOME`/`HOME_X`/`HOME_Y`、二段階homing、hard limit alarmを実装済み、HOME実機確認完了 |
| 台形加減速 | `TrapezoidPlanner`でTRAPEZOID/TRIANGULAR profileを実装済み |
| timed segment | `SegmentGenerator`とFastAccelStepper `moveTimed()`によるA/B同期実行を実装済み |
| streaming drift対策 | timed segment部分投入対策、絶対step座標化、CommandQueue backpressureを実装済み |
| 脱調対策 | 描画用の保守的な速度/加速度/電流/ペン圧設定とcenter shapes実機確認を追加 |
| 正式描画入力 | G-codeを基本とする方針へ整理。`XY`は診断/bring-up用として維持 |
| G-code parser | Phase 7最小G-codeを実装済み、実機確認は未完了 |
| Job Lifecycle | `JOB_BEGIN`/`JOB_END`/`JOB_ABORT`/`JOB_STATUS`を実装済み、motionを伴う実機確認は未完了 |
| look-ahead | 実装済み、実機未確認 |
| junction deviation | 実装済み、実機未確認 |
| Host WebUI Image to G-code | SVG/PNG/JPEGから仮想G-codeを生成し、preview/save/sendへ接続済み。SVGは小型サンプルで実機描画確認済み。PNG/JPEGは実機品質未確認 |

現在の最優先作業:

```text
実機で脱調しない描画条件を確認し、TMC電流、ペン圧、加速度、limit入力ノイズ耐性を安全側に調整する。
```

---

## 2. 全体フェーズ一覧

| Phase | 名前 | 目的 | 状態 |
|---:|---|---|---|
| 0 | 文書と土台 | AGENTS/SPEC/PLANS、構造、placeholderを作る | [x] |
| 0.5 | M5Stack Core2 board profile | Core2ピン、Core割り付け、UART構成を固定する | [x] |
| 1 | Simulation CoreXY | モータを動かさずA/B変換を検証 | [x] |
| 2 | TMC2209 UART | TMC2209 A/Bを共通UARTで初期化 | [x] |
| 3 | FastAccelStepper単軸 | A/Bモータを低速で個別に動かす | [x] |
| 4 | XY低速移動 | CoreXYとして四角を低速で動かす | [x] |
| 5 | Safety / Diagnostics | soft limit、limit、POS、CONFIG、SELFTEST | [x] |
| 6 | Planner placeholder | MotionBlock、PlannerQueue等を用意 | [x] |
| 6.6 | Core2 LCD Status UI | Core2内蔵LCDに機械状態を表示する | [x] |
| 6.7 | NEOPIXEL Status LED | GPIO33の外付けNEOPIXELを設定灯数で制御する | [x] |
| 6.8 | Motor Melody Diagnostics | STEP周波数とTMC設定を一時変更して診断メロディを鳴らす | [x] |
| 6.9 | Homing bring-up | X/Y原点復帰、hard limit、homed状態を実装する | [x] |
| 7 | 最小G-code | G0/G1/G4/G90/G91/G20/G21/G28/M3/M5/M114 | [x] |
| 8 | 台形加減速 | TrapezoidPlannerを実装 | [x] |
| 9 | timed segment | SegmentGeneratorでA/B同期 | [x] |
| 10 | look-ahead | JunctionPlanner、junction deviation | [-] |
| 10.5 | Job Lifecycle | G-codeジョブの開始/終了処理をファーム側へ移す | [-] |
| 11 | 高級機能 | homing、TMC診断、SD/WebUI、補正系 | [ ] |

現在の実装対象:

```text
G-codeを正式描画入力として実機確認し、timed segment実機描画安定化とPhase 10.5 Job Lifecycle設計を進める
```

Phase 0〜6.9、Phase 8、Phase 9は完了済み。
Phase 10は実装済み、実機確認は未完了。
Phase 7は最小G-code範囲で実装済み。正式描画入力はG-codeを基本とし、`XY`は診断/bring-up用として扱う。
Phase 10.5で、ホスト側`gcode_preamble.csv`へ寄っているジョブ開始/終了処理をファームウェア側へ移す。
Phase 11以降は、実装範囲を確認してから着手する。
ただし、先々の設計を忘れないようチェックリストとして残しておく。

---

# Phase 0: 文書と土台

## 目的

Codexが暴走せずに作業できる土台を作る。

## チェックリスト

### 0.1 ルート文書

- [x] `AGENTS.md` が存在する
- [x] `SPEC.md` が存在する
- [x] `PLANS.md` が存在する
- [x] `README.md` が存在する
- [x] READMEにプロジェクト目的が書かれている
- [x] READMEにビルド方法が書かれている
- [x] READMEにSIMULATION_MODEの説明がある
- [x] READMEに安全注意がある
- [x] READMEに既知の制限がある

### 0.2 includeファイル

- [x] `include/PlotterConfig.h`
- [x] `include/Core2PinMap.h`
- [x] `include/TaskConfig.h`
- [x] `include/MachineState.h`
- [x] `include/CommandTypes.h`
- [x] `include/CommandMessage.h`
- [x] `include/CoreXYKinematics.h`
- [x] `include/MotionBlock.h`
- [x] `include/PlannerQueue.h`
- [x] `include/TrapezoidPlanner.h`
- [x] `include/JunctionPlanner.h`
- [x] `include/SegmentGenerator.h`
- [x] `include/StepperBackend.h`
- [x] `include/StepperBackendFastAccel.h`
- [x] `include/TMC2209Manager.h`
- [x] `include/SafetyManager.h`
- [x] `include/PenController.h`
- [x] `include/Diagnostics.h`

### 0.2.1 configファイル責務

- [x] `include/PlotterConfig.h`を動作パラメータのconfigファイルとして使う
- [x] `include/Core2PinMap.h`をボード固有ピンとUARTアドレスのconfigファイルとして使う
- [x] NEOPIXEL灯数、輝度上限、初期パターン、frame interval、パターン初期値を`PlotterConfig.h`から設定できる
- [x] メロディの有効化、microsteps、RMS current、chop mode、note gapを`PlotterConfig.h`から設定できる
- [x] 将来追加する調整可能な定数を各モジュールへ直書きせず、原則としてconfigファイルへ集約する
- [x] GPIO割り付けを各モジュールへ直書きせず、`Core2PinMap.h`へ集約する
- [x] taskのCore割り付け、優先度、stack等を`TaskConfig.h`へ集約する
- [x] 初期実装ではconfigをコンパイル時設定とし、実行時設定保存は将来拡張として分離する

### 0.3 srcファイル

- [x] `src/main.cpp`
- [x] `src/CommandDispatcher.cpp`
- [x] `src/CoreXYKinematics.cpp`
- [x] `src/PlannerQueue.cpp`
- [x] `src/TrapezoidPlanner.cpp`
- [x] `src/JunctionPlanner.cpp`
- [x] `src/SegmentGenerator.cpp`
- [x] `src/StepperBackendFastAccel.cpp`
- [x] `src/TMC2209Manager.cpp`
- [x] `src/SafetyManager.cpp`
- [x] `src/PenController.cpp`
- [x] `src/Diagnostics.cpp`
- [x] `src/tasks/UiTask.cpp`
- [x] `src/tasks/CommandTask.cpp`
- [x] `src/tasks/MotionTask.cpp`
- [x] `src/tasks/StepperFeedTask.cpp`
- [x] `src/tasks/TmcTask.cpp`
- [x] `src/tasks/LogTask.cpp`

### 0.4 設計ルール

- [x] `main.cpp`が巨大化していない
- [x] `main.cpp`にCoreXY変換式がない
- [x] `main.cpp`にFastAccelStepper詳細がない
- [x] `main.cpp`にTMC2209 UART詳細がない
- [x] placeholderには将来の役割コメントがある
- [x] `SIMULATION_MODE=1`が初期値
- [x] buildできる、またはboard未設定以外のエラーがない

## Phase 0 完了条件

- [x] 上記チェックがすべて完了
- [x] `pio run` が通る、または未設定理由がREADMEに明記されている

---

# Phase 0.5: M5Stack Core2 Board Profile

## 目的

M5Stack Core2前提のピン、Core割り付け、UART構成を固定する。

## チェックリスト

### 0.5.1 ピン定義

- [x] `BOARD_M5STACK_CORE2` が定義されている
- [x] `MOTOR_A_STEP_PIN = 25`
- [x] `MOTOR_A_DIR_PIN = 26`
- [x] `MOTOR_B_STEP_PIN = 27`
- [x] `MOTOR_B_DIR_PIN = 19`
- [x] `MOTOR_EN_PIN = 23`
- [x] `TMC_UART_TX_PIN = 14`
- [x] `TMC_UART_RX_PIN = 13`
- [x] `X_LIMIT_PIN = 36`
- [x] `Y_LIMIT_PIN = 35`
- [x] `PEN_SERVO_PIN = 32`
- [x] `USER_IO_PIN = 33`

### 0.5.2 TMC2209アドレス

- [x] `TMC_A_UART_ADDRESS = 0`
- [x] `TMC_B_UART_ADDRESS = 1`
- [x] `TMC_UART_BAUD = 115200`
- [x] UART共通バス構成がコメントで説明されている
- [x] TX側1kΩ直列抵抗の推奨がコメントにある

### 0.5.3 使用禁止ピンの明記

- [x] GPIO21/22は内部I2Cとして使用禁止と明記
- [x] GPIO1/3はUSB Serialとして使用禁止と明記
- [x] GPIO34/35/36/38は入力専用系として明記
- [x] GPIO0/2はboot strapとして注意記載
- [x] GPIO35/36はリミット入力専用扱いになっている

### 0.5.4 Core割り付け

- [x] `CORE_UI = 0`
- [x] `CORE_MOTION = 1`
- [x] `PRIORITY_UI = 1`
- [x] `PRIORITY_COMMAND = 2`
- [x] `PRIORITY_TMC = 3`
- [x] `PRIORITY_SAFETY = 4`
- [x] `PRIORITY_MOTION = 5`
- [x] `PRIORITY_STEPPER_FEED = 6`

### 0.5.5 CONFIG表示

`CONFIG`で以下を表示する。

- [x] `BOARD=M5STACK_CORE2`
- [x] A/B STEP/DIR/EN
- [x] TMC UART TX/RX
- [x] TMC A/B address
- [x] X/Y limit pins
- [x] Pen pin
- [x] Core割り付け
- [x] task priority
- [x] `SIMULATION_MODE`

## Phase 0.5 完了条件

- [x] `CONFIG`でピン割り付けが確認できる
- [x] Core 0/1の割り付けが確認できる
- [x] 使用禁止ピンがコードコメントとdocsに明記されている

---

# Phase 1: Simulation CoreXY

## 目的

モータを動かさず、CoreXY変換を検証する。

## チェックリスト

### 1.1 `CoreXYKinematics`

- [x] `CoreXYDelta`構造体がある
- [x] `xyDeltaToABSteps()`がある
- [x] `xyMoveToABSteps()`がある
- [x] CoreXY変換式がこのモジュールにだけ存在する
- [x] `STEPS_PER_MM`を使ってstep変換している
- [x] Serial依存がない
- [x] FastAccelStepper依存がない
- [x] MachineState直接依存がない

### 1.2 `SELFTEST`

以下を検証する。

- [x] current=(0,0), target=(10,0) -> A=800, B=800
- [x] current=(0,0), target=(0,10) -> A=800, B=-800
- [x] current=(0,0), target=(10,10) -> A=1600, B=0
- [x] current=(10,0), target=(10,10) -> A=800, B=-800
- [x] current=(10,10), target=(0,0) -> A=-1600, B=0
- [x] 成功時に `SELFTEST PASS` を出す
- [x] 失敗時に理由を出す

### 1.3 `XY` simulation

- [x] `XY <x> <y> [feed]`を受けられる
- [x] feed省略時は`DEFAULT_FEED_MM_MIN`を使う
- [x] current XYをログに出す
- [x] target XYをログに出す
- [x] dx/dyをログに出す
- [x] A/B stepをログに出す
- [x] feedをログに出す
- [x] `SIMULATION_MODE=1`ではモータを動かさない
- [x] 成功時のみMachineStateを更新する

## Phase 1 完了条件

- [x] `SELFTEST PASS`
- [x] `ZERO`後 `XY 10 0` で A=800 B=800
- [x] `ZERO`後 `XY 0 10` で A=800 B=-800
- [x] `ZERO`後 `XY 10 10` で A=1600 B=0
- [x] `SIMULATION_MODE=1`でFastAccelStepper move APIを呼んでいない

---

# Phase 2: TMC2209 UART

## 目的

TMC2209 A/Bを共通UARTで初期化できるようにする。

## チェックリスト

### 2.1 `TMC2209Manager`

- [x] `TMC2209Manager.h`が存在する
- [x] `TMC2209Manager.cpp`が存在する
- [x] Serial2初期化処理がある
- [x] A/Bアドレス管理がある
- [x] 電流設定のplaceholderまたは実装がある
- [x] microstep設定のplaceholderまたは実装がある
- [x] chop mode設定のplaceholderまたは実装がある
- [x] status取得のplaceholderまたは実装がある
- [x] plannerに依存していない
- [x] StepperBackendに依存していない

### 2.2 UART初期化

- [x] `Serial2.begin(115200, SERIAL_8N1, TMC_UART_RX_PIN, TMC_UART_TX_PIN)`相当がある
- [x] TX=GPIO14
- [x] RX=GPIO13
- [x] A address=0
- [x] B address=1

### 2.3 コマンド

- [x] `TMC_INIT`
- [x] `TMC_STATUS`
- [x] simulation時の表示がある
- [x] real時の初期化処理がある、または明確なplaceholderがある

### 2.4 Motion前TMC ready保証

- [x] `XY`実行前にTMC未readyなら`TMC_INIT`相当を自動実行する
- [x] G-code由来`G0`/`G1`実行前にTMC未readyなら`TMC_INIT`相当を自動実行する
- [x] `HOME`/`HOME_X`/`HOME_Y`実行前にTMC未readyなら`TMC_INIT`相当を自動実行する
- [x] `AB_TIMED`実行前にTMC未readyなら`TMC_INIT`相当を自動実行する
- [x] 自動`TMC_INIT`失敗時は該当motionを実行せず拒否する
- [x] 実機でTMC未初期化のままUI jogするとA/B microstep差により左右移動量がずれる問題を確認し、自動初期化で解消した

## Phase 2 完了条件

- [x] `TMC_INIT`が存在する
- [x] `TMC_STATUS`が存在する
- [x] TMC UART処理が`TMC2209Manager`に閉じている
- [x] plannerやStepperBackendにTMC設定が混ざっていない
- [x] motion系コマンドはTMC readyを前提にし、未readyならMotionTask側で自動初期化する

---

# Phase 3: FastAccelStepper単軸

## 目的

FastAccelStepperでA/Bモータを個別に動かす。

## チェックリスト

### 3.1 `StepperBackendFastAccel`

- [x] `begin()`
- [x] `enable()`
- [x] `disable()`
- [x] `moveASteps(int32_t steps)`
- [x] `moveBSteps(int32_t steps)`
- [x] `moveABSteps(int32_t a_steps, int32_t b_steps, float feed_mm_min)`
- [x] `isRunning()`
- [x] `waitUntilIdle()`
- [x] FastAccelStepperのincludeがこのモジュールに閉じている
- [x] `main.cpp`にFastAccelStepper詳細がない

### 3.2 ピン初期化

- [x] A_STEP=25
- [x] A_DIR=26
- [x] B_STEP=27
- [x] B_DIR=19
- [x] EN=23
- [x] EN low active
- [x] direction delayが設定されている

### 3.3 コマンド

- [x] `ENABLE`
- [x] `DISABLE`
- [x] `TEST_A <steps>`
- [x] `TEST_B <steps>`

### 3.4 simulation

- [x] `SIMULATION_MODE=1`ではmove APIを呼ばない
- [x] simulation時に予定stepを表示する

## Phase 3 完了条件

- [x] `TEST_A 200`でAだけ動く、またはsimulation表示される
- [x] `TEST_B 200`でBだけ動く、またはsimulation表示される
- [x] FastAccelStepperがbackendに閉じている
- [x] bring-up用である旨のコメントがある

---

# Phase 4: CoreXY低速移動

## 目的

低速でCoreXYとして四角移動を確認する。

## チェックリスト

### 4.1 `XY` real mode

- [x] `SIMULATION_MODE=0`で実モータ移動できる
- [x] feedから速度計算している
- [x] 速度上限をclampしている
- [x] 加速度初期値を使っている
- [x] 成功時のみMachineState更新
- [x] 失敗時にMachineStateを更新しない

### 4.2 方向確認

- [x] +XでA/B同方向
- [x] +YでA/B逆方向
- [x] +X+Yで片側が主に動く
- [x] +X-Yで反対側が主に動く
- [x] 方向反転設定で調整できる

### 4.3 四角テスト

```text
ZERO
XY 10 0 300
XY 10 10 300
XY 0 10 300
XY 0 0 300
```

- [x] 上記テストが実行できる
- [x] 低速で安全に動く
- [x] 異常時に止められる

## Phase 4 完了条件

- [x] CoreXYとして期待方向に動く
- [x] 四角移動が低速で実行できる
- [x] ログが十分に出る
- [x] 厳密補間ではなくbring-up実装であることが明記されている

---

# Phase 5: Safety / Diagnostics

## 目的

危険な動作を防ぎ、状態を見える化する。

## チェックリスト

### 5.1 SafetyManager

- [x] soft limit確認
- [x] feed validation
- [x] X_LIMIT読み取り
- [x] Y_LIMIT読み取り
- [x] alarm flag
- [x] E-stop placeholder
- [x] homing前移動制限placeholder

### 5.2 soft limit

- [x] X < 0を拒否
- [x] X > 300を拒否
- [x] Y < 0を拒否
- [x] Y > 300を拒否
- [x] 範囲外時に理由を表示

### 5.3 feed validation

- [x] feed <= 0を拒否
- [x] feed > MAXを拒否またはclamp
- [x] clampする場合はログ表示

### 5.4 Diagnostics

- [x] `HELP`
- [x] `CONFIG`
- [x] `POS`
- [x] `SELFTEST`
- [x] `TMC_STATUS`
- [x] `LIMIT_STATUS`またはCONFIG/POS内でlimit表示

## Phase 5 完了条件

- [x] 範囲外XYを拒否する
- [x] feed 0を拒否する
- [x] `POS`が位置と状態を表示する
- [x] `CONFIG`がピンとCore割り付けを表示する
- [x] limit入力の読み取り関数がある

---

# Phase 6: Planner Placeholder

## 目的

将来のplanner挿入場所を確保する。

## チェックリスト

### 6.1 MotionBlock

- [x] `MotionBlock`構造体がある
- [x] start XYを持つ
- [x] target XYを持つ
- [x] dx/dyを持つ
- [x] lengthを持つ
- [x] nominal speedを持つ
- [x] entry/exit speedを持つ
- [x] A/B stepsを持つ
- [x] pen stateを持つ

### 6.2 PlannerQueue

- [x] 固定長リングバッファ
- [x] `enqueue()`
- [x] `dequeue()`
- [x] `peekNext()`
- [x] `isEmpty()`
- [x] `isFull()`
- [x] `count()`
- [x] 動的確保を使わない

### 6.3 TrapezoidPlanner

- [x] placeholderがある
- [x] 将来の役割コメントがある
- [x] StepperBackendに依存していない

### 6.4 JunctionPlanner

- [x] placeholderがある
- [x] look-ahead用コメントがある
- [x] junction deviation用コメントがある
- [x] StepperBackendに依存していない

### 6.5 SegmentGenerator

- [x] placeholderがある
- [x] `MotionSegment`がある
- [x] A/B同期timed segment用コメントがある

## Phase 6 完了条件

- [x] 型とファイルが存在する
- [x] 役割コメントがある
- [x] まだlook-aheadは実装していない
- [x] まだjunction deviationは実装していない
- [x] まだtimed segment実行はしていない

---

# Phase 6.6: Core2 LCD Status UI

## 目的

Core2内蔵LCDに機械状態を表示し、Serial接続なしでもbring-upと安全状態を確認できるようにする。

## チェックリスト

### 6.6.1 LCD初期化

- [x] Core2内蔵LCDを初期化する
- [x] LCD初期化と描画処理をCore 0の`uiTask`に閉じ込める
- [x] Core 1からLCD APIを直接呼ばない
- [x] LCD初期化失敗時もmotion制御を不必要にブロックしない

### 6.6.2 StatusQueue

- [x] Core 1 → Core 0の`StatusQueue`を実装する
- [x] 表示用の`StatusMessage`を定義する
- [x] `StatusMessage`に状態スナップショットを含める
- [x] 状態変化時にStatusQueueへ通知する
- [x] 低頻度の定期更新を行う
- [x] queue満杯時にmotion処理をブロックしない

### 6.6.3 ステータス画面

- [x] mode: `SIMULATION` / `REAL`
- [x] position: `X` / `Y` [mm]
- [x] motor: `ENABLED` / `DISABLED`
- [x] homing: `HOMED` / `NOT HOMED`
- [x] pen: `UP` / `DOWN`
- [x] safety: `READY` / `ALARM`
- [x] limit: X/Y入力状態
- [x] TMC: `READY` / `NOT READY`

### 6.6.4 更新処理

- [x] 状態変化時に画面を更新する
- [x] 低頻度の定期更新で表示を再同期する
- [x] LCD描画でmotion、safety、stepper処理をブロックしない
- [x] 変更箇所のみ更新する、またはちらつきを抑える描画方式にする
- [x] `M5Canvas`へ描画してから`pushSprite()`することで、全画面再描画時のちらつきを抑える
- [x] canvas確保失敗時は直接LCD描画へフォールバックする

### 6.6.5 Touch/Button操作UI

- [x] 下部タブで`Status` / `Control` / `Detail`ページを切り替える
- [x] 左右フリックでページ送りする
- [x] Core2物理A/Cボタンで前後ページへ移動する
- [x] `Status`ページに大きな安全状態、X/Y、pen、TMC、home状態を表示する
- [x] `Control`ページに`HOME`、`ALARM_CLEAR`、上下左右jog、`PENUP`、`PENDOWN`を配置する
- [x] `Detail`ページにhoming、feed、A/B step、limit debounced/raw状態を表示する
- [x] UI操作は既存`CommandMessage`を`command_queue`へ投入し、MotionTaskの安全経路を通す
- [x] `HOME`はhoming中でなければUIから実行できる
- [x] `ALARM_CLEAR`はalarm中だけUIから実行できる
- [x] jogとpen上下はhome完了済み、alarmなし、homing中でない時だけ有効にする
- [x] jogは現在座標から±1mmの絶対`XY`へ変換し、soft limit内へclampする
- [x] queue満杯、soft limit、home未完了などの拒否理由を短いnoticeとして表示する

## Phase 6.6 完了条件

- [x] Core2内蔵LCDに全ステータス項目が表示される
- [x] Serial未接続でもposition、alarm、limit状態を確認できる
- [x] Core 0だけがLCDを描画する
- [x] Core 1からの状態は`StatusQueue`経由で表示される
- [x] LCD更新中もmotion処理が不必要に停止しない
- [x] `pio run`が通る

---

# Phase 6.7: NEOPIXEL Status LED

## 目的

GPIO33へ接続した外付けNEOPIXELを設定灯数で制御し、bring-upと状態表示に利用できるようにする。

## チェックリスト

### 6.7.1 ピンと設定

- [x] `NEOPIXEL_PIN = 33`
- [x] `NEOPIXEL_LED_COUNT`を`PlotterConfig.h`で設定できる
- [x] `NEOPIXEL_BRIGHTNESS_MAX`を`PlotterConfig.h`で設定できる
- [x] 初期状態を消灯にする
- [x] 低輝度の上限値を設定する
- [x] GPIO33を他用途と重複使用しない

### 6.7.2 FastLED出力層

- [x] `platformio.ini`の`lib_deps`へFastLEDを追加する
- [x] `include/NeoPixelController.h`
- [x] `src/NeoPixelController.cpp`
- [x] `begin()`
- [x] `setAllRgb(uint8_t r, uint8_t g, uint8_t b)`
- [x] `setPixelRgb(uint16_t index, uint8_t r, uint8_t g, uint8_t b)`
- [x] `show()`
- [x] `off()`
- [x] FastLEDの`CRGB`バッファを灯数に応じて管理する
- [x] `FastLED.addLeds()`、`FastLED.setBrightness()`、`FastLED.show()`をこの層に閉じ込める
- [x] FastLEDライブラリ依存をNEOPIXEL関連モジュールの外へ漏らさない
- [x] Core 1からNEOPIXEL APIを直接呼ばない

### 6.7.3 パターンエンジン

- [x] `include/LedPatternEngine.h`
- [x] `src/LedPatternEngine.cpp`
- [x] パターン共通interfaceとして`render(now_ms, leds, led_count, config)`相当を定義する
- [x] `OFF`
- [x] `SOLID`
- [x] `PACIFICA`
- [x] `FIRE`
- [x] `BREATH`
- [x] `CHASE`
- [x] `PROGRESS`
- [x] `ALERT`
- [x] `SUCCESS`
- [x] FastLEDのPacifica例を参考に`PACIFICA`を実装する
- [x] FastLEDのFire2012例を参考に`FIRE`を実装する
- [x] パターン追加時に`NeoPixelController`を変更せず追加できる構造にする
- [x] `LedAnimationConfig`にbrightness、hue、saturation、speed、intensity、cooling、sparking等を保持する
- [x] 未使用parameterはパターンごとに無視できる
- [x] palette選択と将来のpalette追加をパターン層で扱えるようにする

### 6.7.4 非ブロッキング更新

- [x] Core 0の`uiTask`または専用低優先度taskから`tick(now_ms)`を呼ぶ
- [x] `NEOPIXEL_FRAME_INTERVAL_MS`で描画頻度を制限する
- [x] 1 frameごとにpattern render後、必要時だけ`show()`する
- [x] `delay()`や`FastLED.delay()`でtaskを待機させない
- [x] LED更新でmotion、safety、stepper処理をブロックしない
- [x] 灯数とframe rateを増やした場合のCore 0負荷を確認する

### 6.7.5 外部設定とコマンド

- [x] UI入力 → LED描画処理の`LedCommandQueue`または明確な同期機構を用意する
- [x] `LED <r> <g> <b>`
- [x] `LED_PIXEL <index> <r> <g> <b>`
- [x] `LED_OFF`
- [x] `LED_PATTERN <OFF|SOLID|PACIFICA|FIRE>`
- [x] `LED_BRIGHTNESS <0..NEOPIXEL_BRIGHTNESS_MAX>`
- [x] `LED_PARAM <BRIGHTNESS|HUE|SATURATION|SPEED|INTENSITY|COOLING|SPARKING> <value>`
- [x] `LED_AUTO <0|1>`
- [x] `LED_STATUS_SET <IDLE|HOMING|DRAWING_PEN_UP|DRAWING_PEN_DOWN|PROCESSING|PAUSED|COMPLETED|WARNING|ERROR>`
- [x] `LED_STATUS`
- [x] RGB値の0〜255範囲を検証する
- [x] indexが`0 <= index < NEOPIXEL_LED_COUNT`であることを検証する
- [x] parameter値を範囲検証する
- [x] command処理と描画処理を分離し、外部引数は`LedAnimationConfig`へ反映する
- [x] 描画中の`LedAnimationConfig`更新で不整合が起きないようにする
- [x] 手動表示と自動状態表示を`auto_status_enabled`で分離する
- [x] 自動状態表示で`ERROR > WARNING > PAUSED > HOMING > DRAWING_PEN_DOWN > DRAWING_PEN_UP > PROCESSING > COMPLETED > IDLE`の優先順位を守る
- [x] `COMPLETED`/`WARNING`演出を短時間保持してから`IDLE`へ自動復帰する
- [x] Core 1側状態変化は`LedCommandQueue`経由でCore 0側LED engineへ渡す
- [x] Homing、Pen Up/Down、Job開始/完了/Abort、Alarm/Errorの主要状態から`LedStatus`を更新する
- [x] 将来Touch/Button、WebUI、Serial以外の入力元から同じ設定APIを呼べる構造にする
- [x] LED更新でmotion処理をブロックしない

## Phase 6.7 完了条件

- [x] `LED 255 0 0`で赤色に点灯する
- [x] `LED_PIXEL 0 0 255 0`で指定LEDだけを緑色にできる
- [x] `LED_PATTERN PACIFICA`でPacificaパターンを選択できる
- [x] `LED_PATTERN FIRE`でFireパターンを選択できる
- [x] brightness、hue、speed等の外部引数が対応パターンへ反映される
- [x] `LED_OFF`で消灯する
- [x] Core 0のUI側だけがNEOPIXELを更新する
- [x] アニメーション更新中もmotion処理が不必要に停止しない
- [x] `pio run`が通る

---

# Phase 6.8: Motor Melody Diagnostics

## 目的

`../1stepper_test`の起動メロディを参考に、STEP周波数、TMC2209のRMS電流、microstep、chop modeを一時変更する診断用モータメロディを追加する。

## チェックリスト

### 6.8.1 `MotorMelodyController`

- [x] `include/MotorMelodyController.h`
- [x] `src/MotorMelodyController.cpp`
- [x] 音符ごとのSTEP周波数と長さをテーブルで定義する
- [x] A/Bの診断対象モータを明示する
- [x] 短い正逆交互移動で移動量を抑える
- [x] MachineStateの論理X/Y位置を更新しない
- [x] 起動時に自動再生しない

### 6.8.2 STEP出力

- [x] STEP周波数変更は`StepperBackendFastAccel`経由で行う
- [x] FastAccelStepperの詳細を`StepperBackendFastAccel`外へ漏らさない
- [x] `delayMicroseconds()`でSTEPパルスを直接生成しない
- [x] 通常motionとメロディを排他実行する
- [x] limitまたはalarmで中断できる

### 6.8.3 TMC診断用profile

- [x] 通常profileを保存する
- [x] メロディ用microsteps初期案を`2`にする
- [x] メロディ用RMS current初期案を`1200mA以下`にする
- [x] メロディ用chop modeを`spreadCycle`にする
- [x] note gap初期案を`25ms`にする
- [x] メロディ用profileを`PlotterConfig.h`から設定できる
- [x] profile変更を`TMC2209Manager`に閉じ込める
- [x] 完了、中断、alarm、limit検出時に通常profileへ復元する
- [x] TMC UART未ready時は実行を拒否する

### 6.8.4 コマンドとsimulation

- [x] `MELODY`
- [x] `SIMULATION_MODE=1`では実モータを動かさない
- [x] simulation時に予定音符とprofile変更をログ表示する
- [x] motion実行中は`MELODY`を拒否する

## Phase 6.8 完了条件

- [x] `MELODY`で短い診断メロディが鳴る
- [x] メロディ中だけSTEP周波数、電流、microstep、chop modeが変更される
- [x] 正常終了後に通常profileへ復元される
- [x] limitまたはalarm中断後にも通常profileへ復元される
- [x] 通常motionと同時実行されない
- [x] `pio run`が通る

---

# Phase 6.9: Homing bring-up

Status: 計画追加。まだ実装しない。

## 目的

X/Yリミットスイッチを使って機械原点へ復帰し、`MachineState`のhomed状態とhard limit alarmを安全に扱えるようにする。

G-codeの`G28`を実装する前に、Serialコマンドで原点復帰を検証する。  
このPhaseでは、planner、look-ahead、timed segmentはまだ実装しない。

`../1stepper_test`のhoming実装を参照要件とする。  
特に、速いseekで一度リミットへ突き当て、backoffでスイッチを離し、遅いseekでもう一度リミットをONにして、その2回目のON位置を原点として採用する。

## 基本方針

- homingはCore 1のmotion側だけで実行する
- Core 0は`CommandQueue`へhoming commandを投入するだけにする
- Core 0からStepperBackend、SafetyManager、TMC2209Managerを直接操作しない
- X/Y移動は必ず`CoreXYKinematics`経由でA/B stepへ変換する
- homing中だけsoft limitを一時的に専用扱いにする
- homing対象軸以外のlimit入力がactiveになった場合はalarmにする
- `ZERO`は論理原点リセットのままとし、homing扱いにしない
- 起動時に自動homingしない
- motor power off、microstep変更、steps/mm変更など位置保持を信用できなくなる操作後は`homed=false`に戻す

## `../1stepper_test`由来の必須動作要件

- [x] homing開始時に対象軸の`homed`をfalseにする
- [x] `SeekFast`でリミット方向へ速く移動する
- [x] `SeekFast`中にlimit debounced ONを検出したら即座に`Backoff`へ移行する
- [x] 起動時点またはhoming開始時点でlimitがONの場合も、まず`Backoff`へ移行する
- [x] `Backoff`ではリミットと反対方向へ移動する
- [x] `Backoff`中にlimit debounced OFFを検出したら`SeekSlow`へ移行する
- [x] `Backoff`が`HOMING_BACKOFF_MM`に達してもlimitがOFFにならない場合はalarm/errorにする
- [x] `SeekSlow`でリミット方向へ低速移動する
- [x] `SeekSlow`中にlimit debounced ONを検出したら`SetZero`へ移行する
- [x] `SetZero`では2回目のlimit ON位置を機械原点としてX/Y位置とA/B step位置を設定する
- [x] homing完了時のlimit状態はONであることを期待値にする
- [x] `SeekFast`と`SeekSlow`それぞれでmax travel超過を検出し、超過時はalarm/errorにする
- [x] homing完了前の通常XY移動を禁止する
- [x] homing完了後もsoft limit外の通常XY移動を拒否する
- [x] 通常移動中にlimit ONを検出した場合は安全停止してalarm/errorにする
- [x] limit入力はraw値とdebounced値の両方を診断表示できる
- [x] debounce時間をconfig化する
- [x] homing state遷移とlimit状態をSerial/LogQueueへ出力する
- [x] homing完了ログは原点座標、A/B step、limit状態を含める
- [x] homing errorログはstate、reason、現在位置、A/B step、limit raw/debouncedを含める

## チェックリスト

### 6.9.1 config

- [x] `HOMING_ENABLED`を`PlotterConfig.h`で設定できる
- [x] `HOMING_X_DIR`を設定できる
- [x] `HOMING_Y_DIR`を設定できる
- [x] `HOMING_SEEK_FEED_MM_MIN`を設定できる
- [x] `HOMING_SLOW_FEED_MM_MIN`を設定できる
- [x] `HOMING_BACKOFF_MM`を設定できる
- [x] `HOMING_MAX_TRAVEL_X_MM`を設定できる
- [x] `HOMING_MAX_TRAVEL_Y_MM`を設定できる
- [x] `HOMING_SET_X_MM`を設定できる
- [x] `HOMING_SET_Y_MM`を設定できる
- [x] `HOMING_LIMIT_DEBOUNCE_MS`を設定できる
- [x] `HOMING_REQUIRE_HOMED_FOR_XY_MOVE`を設定できる
- [x] limit入力のactive polarityを設定またはコメントで明記する
- [x] limit入力に外付けpull-upを使う前提をコメントで明記する

### 6.9.2 command

- [x] `CommandType::Home`
- [x] `CommandType::HomeX`
- [x] `CommandType::HomeY`
- [x] `HOME`
- [x] `HOME_X`
- [x] `HOME_Y`
- [x] `HOME_STATUS`
- [x] `LIMIT_STATUS`
- [x] `ALARM_CLEAR`または同等のalarm復帰command
- [x] `HELP`へhoming commandを追加する
- [x] `G28`はPhase 7で`HOME`へ接続する計画として残す
- [x] parserはhomingを直接実行せず、CommandQueueへ投入する

### 6.9.3 `HomingController`

- [x] `include/HomingController.h`
- [x] `src/HomingController.cpp`
- [x] homing stateを保持する
- [x] `Idle`
- [x] `SeekFastX`
- [x] `BackoffX`
- [x] `SeekSlowX`
- [x] `SetXZero`
- [x] `SeekFastY`
- [x] `BackoffY`
- [x] `SeekSlowY`
- [x] `SetYZero`
- [x] `Complete`
- [x] `Alarm`
- [x] Xだけ、Yだけ、X/Y連続のhomingを実行できる
- [x] `SeekFast -> Backoff -> SeekSlow -> SetZero`の2段階homingを行う
- [x] `SeekFast`でmax travel超過時にalarmを立てる
- [x] `SeekSlow`でmax travel超過時にalarmを立てる
- [x] limitが最初からactiveの場合はbackoffしてからslow seekする
- [x] backoff距離内にlimitがOFFにならない場合はalarmを立てる
- [x] 2回目のlimit ON位置だけを原点採用する
- [x] homing完了時だけMachineStateの位置を設定する
- [x] X/Y両方完了時だけ`MachineState::homed = true`にする
- [x] homing中は通常motion、MELODY、pen動作と排他にする
- [x] homing開始時にMachineStateのhomedをfalseへ戻す

### 6.9.4 SafetyManager連携

- [x] homing中かどうかをSafetyManagerへ渡す
- [x] homing対象軸のlimit activeは停止条件として扱う
- [x] homing対象外軸のlimit activeはalarmとして扱う
- [x] homing以外の通常移動中にlimit activeになったらalarmにする
- [x] alarm中はhomingを含むmotion commandを拒否する
- [x] alarm復帰用commandを計画する
- [x] homed前の通常XY移動を許可するか禁止するかをconfigで決める
- [x] homed前移動を禁止する場合でもhoming commandは許可する
- [-] motor power off後はhomedをfalseにする
- [-] TMC microstep変更後はhomedをfalseにする
- [x] steps/mm相当の設定変更後はhomedをfalseにする

### 6.9.5 StepperBackend連携

- [-] homing用の低速移動APIをStepperBackendに追加する
- [x] FastAccelStepper詳細は`StepperBackendFastAccel`内に閉じ込める
- [x] STEPパルスをtaskや`delayMicroseconds()`で直接生成しない
- [x] homing move中にlimit状態を定期確認できる構造にする
- [x] limit検出時に安全に停止できる構造にする
- [x] bring-up段階では短いincremental moveの反復を許容する
- [x] incremental move反復では、各反復前後でlimit debounced状態を確認する
- [x] 将来のtimed segment化で置き換えられるコメントを残す

### 6.9.6 CoreXY方向確認

- [x] X homingはXY空間のX方向移動として実行する
- [x] Y homingはXY空間のY方向移動として実行する
- [x] X homing中のA/B方向が想定通りであることを確認する
- [x] Y homing中のA/B方向が想定通りであることを確認する
- [x] `MOTOR_A_DIR_INVERT` / `MOTOR_B_DIR_INVERT`調整後もhoming方向がconfigで追従できる

### 6.9.7 Diagnostics / UI

- [x] `POS`にhomed状態を表示する
- [x] `LIMIT_STATUS`または`POS`でlimit状態を確認できる
- [x] `HOMING_STATUS`または`POS`でhoming stateを確認できる
- [x] limit raw/debouncedを両方表示できる
- [x] homing no-step / wait / error reasonを表示できる
- [x] LCDにhoming中状態を表示する
- [x] homing alarm理由をLogQueueへ出す
- [x] homing完了時に設定されたX/Y座標をログ出力する
- [x] 実機確認では`Backoff limit=ON`、`SeekSlow limit=OFF`、`SetZero limit=ON`、`Complete limit=ON`相当の遷移ログを確認する

## Phase 6.9 完了条件

- [x] `HOME_X`でX limitまで移動し、backoff後に低速再検出できる
- [x] `HOME_Y`でY limitまで移動し、backoff後に低速再検出できる
- [x] `HOME`でX/Yを順番にhomingできる
- [x] 2回目の低速seekでlimit ONになった位置だけを原点として採用する
- [x] homing完了時のlimit debounced状態がONである
- [x] homing後に`POS`が原点座標と`HOMED=YES`を表示する
- [ ] max travel超過時にalarmになる
- [ ] backoffしてもlimitがOFFにならない場合にalarmになる
- [ ] homing対象外limit activeでalarmになる
- [ ] 通常移動中のlimit activeでalarmになる
- [ ] alarm中に通常motionが拒否される
- [x] `ZERO`がhoming扱いにならない
- [x] `pio run`が通る
- [x] `pio run --target upload`後、実機で`HOME_X`、`HOME_Y`、`HOME`を確認する

---

# Phase 7: 最小G-code

Status: 実装済み。正式描画入力はG-codeを基本とする。`G0/G1`は内部実装として既存`XY`経路へ、`G28`は既存`HOME`経路へ接続する。実機確認は未完了。

運用方針:

- 通常描画ジョブの外部インターフェースはG-codeを基本にする
- `XY`、`TEST_A`、`TEST_B`、`AB_TIMED`、`MELODY`は診断/bring-up用として維持する
- Text Tool、QR Tool、将来のSD/Web/USB streamingはG-code出力/入力へ寄せる
- `gcode_preamble.csv`は正式Job Lifecycle実装までの暫定bring-up手順として扱う

## チェックリスト

- [x] `GcodeParser`を追加
- [x] `ParsedGcode`を追加
- [x] `GcodeInterpreter`を追加
- [x] `G0 X Y F`
- [x] `G1 X Y F`
- [x] `G20`
- [x] `G21`
- [x] `G28`
- [x] `G90`
- [x] `G91`
- [x] `M3`
- [x] `M5`
- [x] `M114`
- [x] parserはmotionを直接実行しない
- [x] interpreterがMachineStateを扱う
- [x] motionはSafetyManagerを通る
- [x] F値はmm/minとして扱う
- [x] 正式描画入力をG-code基本、`XY`を診断/bring-up用とする運用方針をSPEC/PLANSへ反映
- [x] `tools/serial_tool/examples/gcode_check.csv`を追加
- [x] `tools/serial_tool/docs/gcode-check.md`を追加
- [ ] 実機で`gcode_check.csv`を実行し、`G28`、単位切替、相対移動、pen、`M114`を確認する

---

# Phase 8: 台形加減速

Status: 実装済み。Phase 9のtimed segmentまでは既存backend経路へ渡す。

## チェックリスト

- [x] `TrapezoidPlanner`を実装
- [x] acceleration phase
- [x] cruise phase
- [x] deceleration phase
- [x] short move triangular profile
- [x] max velocity制限
- [x] max acceleration制限
- [x] MotionBlockに計画結果を保持
- [x] StepperBackendに加減速ロジックを入れない

---

# Phase 9: timed segment

Status: 実装済み。MotionTaskがSegmentQueueを補充し、FastAccelStepper `moveTimed()`でA/B timed segmentを実行する。

## チェックリスト

- [x] `SegmentGenerator`を実装
- [x] DDAまたは同等のA/B同期
- [x] `MotionSegment`生成
- [x] `SegmentQueue`実装
- [x] FastAccelStepper `moveTimed`検討
- [x] 低レベルqueue検討
- [x] SegmentGeneratorとStepperBackendの責務分離

## 9.1 実機描画安定化 / 脱調対策

Phase 9のtimed segment実装後、中心図形描画で一部脱調および原点外でのX limit active alarmが発生した。
追加仕様として、描画負荷を下げる保守的な初期設定と、limit入力ノイズで即停止しない安全判定を導入する。

### 9.1.1 Motion / TMC / Pen設定

- [x] `DEFAULT_MOTOR_ACCEL_STEPS_S2`を実機描画用に保守的な値へ下げる
- [x] `DEFAULT_ACCEL_MM_S2`の`CONFIG`表示とcheck CSV期待値を追従させる
- [x] CoreXY最悪条件`sqrt(2)`と`SPEED_SAFETY`を使って`MAX_FEED_MM_MIN`を`MAX_MOTOR_SPEED_STEPS_S`から導出する
- [x] `DEFAULT_FEED_MM_MIN`、`DEFAULT_MOTOR_SPEED_STEPS_S`、`DEFAULT_MOTOR_ACCEL_STEPS_S2`を独立設定ではなく導出値にする
- [x] timed segment生成時にA/B各segmentのstep周波数が`MAX_MOTOR_SPEED_STEPS_S`を超えないことを確認する
- [x] `TMC_NORMAL_RMS_CURRENT_MA`を実機で脱調しない方向へ調整する
- [x] `PEN_DOWN_ANGLE_DEG`をconfig化した上で、紙への押し付けを弱める方向へ調整する
- [ ] TMC電流増加後のモータ/ドライバ温度を連続描画で確認し、必要なら電流を下げる

### 9.1.2 hard limit入力の通常移動中判定

- [x] homing中ではない通常移動で、原点外のlimit activeが一定時間継続した場合だけalarmへ入れる
- [x] 継続時間を`HARD_LIMIT_UNEXPECTED_ALARM_MS`として`PlotterConfig.h`から設定できる
- [x] 瞬間的なlimitノイズでは即alarmにしない
- [x] timed segment実行中にFastAccelStepperの現在A/B stepからMachineStateのX/Y概算位置を更新し、ブロック完了前にhard limit判定できるようにする
- [x] `HARD_LIMIT_UNEXPECTED_ALARM_MS`を20msへ短縮し、limit ON継続時の停止遅れを減らす
- [x] homing直後の通常移動で、原点limitがONかつ離れる方向へ動く場合は`NORMAL_MOVE_LIMIT_RELEASE_MM`だけlimit release猶予を与える
- [x] release猶予距離を超えてもlimitがOFFにならない場合は`X/Y home limit did not release`でalarm停止する
- [x] limitが継続ONの場合は従来通り安全停止できる構造にする
- [ ] 実配線でX/Y limit入力のノイズ量を確認し、必要なら外付けpull-up、配線取り回し、RC filterを追加する

### 9.1.3 center shapes実機確認

- [x] `tools/serial_tool/examples/center_shapes.csv`を追加する
- [x] 紙面中心を`(X_MIN+X_MAX)/2`, `(Y_MIN+Y_MAX)/2`相当の`(27.5, 27.5)`として扱う
- [x] マル、四角、三角、星を中心付近へ描画する
- [x] 図形サイズはsoft limit端から十分余白を残す
- [x] 通常描画CSVはfeedを省略し、ファームウェア側の`DEFAULT_FEED_MM_MIN`から開始する
- [x] `center_shapes.csv`が実機で最後まで完走し、最終`POS`で`ALARM=NO`、`LIMIT_X=OPEN`、`LIMIT_Y=OPEN`を確認する
- [ ] 脱調が再発しないか、同じCSVを複数回連続で実行して確認する

### 9.1.4 動き出し・動き終わり歪み調査

- [x] `tools/serial_tool/examples/concentric_squares_check.csv`を追加する
- [x] 逆方向確認用に`tools/serial_tool/examples/concentric_squares_clockwise_check.csv`を追加する
- [x] 速度依存性確認用に`tools/serial_tool/examples/concentric_squares_high_speed_check.csv`を追加する
- [x] `concentric_squares_high_speed_check.csv`は`delay_ms=0`、`expect`空欄とし、`serial_send.py --queue-mode`でCommandQueue満杯時にbackpressureする
- [x] 同じ中心`(27.5, 27.5)`に辺長違いの正方形を5個重ねて描く
- [x] 水平/垂直辺だけで構成し、各辺の始点・終点の歪みを目視確認しやすくする
- [x] 手順書`tools/serial_tool/docs/concentric-squares-check.md`を追加する
- [ ] 実機で実行し、各正方形の閉じ位置、角の丸まり、終点オーバーシュートを確認する
- [ ] 高速版を実機で実行し、通常版と比べて角の丸まり、終点オーバーシュート、閉じズレ、脱調、温度上昇が増えるか確認する
- [ ] 高速版を`--queue-mode`で実行し、`CommandQueue full`が再送で回復し、PENUP/PENDOWN順序が崩れないことを確認する
- [ ] 歪みが再現する辺、描画方向、サイズ依存性を記録し、速度/加速度/ペン圧/ベルト張り/ガタのどれを優先調整するか決める

### 9.1.5 AB_TIMED診断 / backend直接timed実行切り分け

目的:

`AB_TIMED`を診断専用コマンドとして追加し、通常の`XY`描画経路と、A/Bモータを直接`moveTimed()`系へ渡す経路を比較する。
これにより、角ズレ、歪み、閉じズレの原因が`TrapezoidPlanner`、`SegmentGenerator`、`SegmentQueue`側か、`StepperBackendFastAccel`/FastAccelStepper側かを切り分ける。

ベースCSV:

- `tools/serial_tool/examples/concentric_squares_clockwise_check.csv`
- `tools/serial_tool/examples/concentric_squares_check.csv`

チェック:

- [x] `AB_TIMED <a_steps> <b_steps> <duration_us>`を追加する
- [x] `AB_TIMED`は`XY`、`CoreXYKinematics`、`TrapezoidPlanner`、`SegmentGenerator`、`SegmentQueue`をバイパスする
- [x] `a_steps=0`または`b_steps=0`の片側timed moveを許可する
- [x] `duration_us`が短すぎる場合は`NACK_AB_TIMED reason=duration_too_short`を返す
- [x] alarm中は`NACK_AB_TIMED reason=alarm`で実行しない
- [x] queue前後の`micros()`、queue結果、A/B running状態、A/B queue entriesをログする
- [x] `tools/serial_tool/examples/diagnostic_ab_timed_square_draw.csv`を追加する
- [x] CSVは`PENDOWN`して実際に線を描く
- [x] 小さい四角、大きい四角、同じサイズ2回連続、時計回り/反時計回りを含める
- [ ] 実機で通常XY版CSVとAB_TIMED版CSVを同じ紙面条件で比較する
- [ ] AB_TIMEDの閉じズレ、方向依存、サイズ依存を記録する

## 9.2 stream G-code motion座標ドリフト対策

G-codeを高速streamして連続描画する場合の座標ドリフト要因を潰す。

### 9.2.1 FastAccelStepper timed segment部分投入対策

- [x] A側`moveTimed()`成功後にB側が`RETRY`した場合、B側だけを`vTaskDelay(1)`を挟んでリトライする
- [x] B側`ERROR`またはリトライtimeout時はbackendを停止する
- [x] 呼び出し側はbackend現在stepから`MachineState`を再同期し、homedを無効化してalarmへ入れる
- [x] `MOVE_TIMED_OK`、`MOVE_TIMED_EMPTY`、`DirPin2msPauseAdded`を`QUEUED`扱いする既存分類を維持する
- [x] SIMULATION_MODEとint16 step制限を維持する

### 9.2.2 絶対step座標化

- [x] `CoreXYKinematics::xyPositionToABSteps()`を追加し、`round((x+y)*steps_per_mm)`、`round((x-y)*steps_per_mm)`で絶対A/B stepを算出する
- [x] `MotionBlock`へ`target_a_steps`/`target_b_steps`を追加する
- [x] `buildXYBlock()`はplanned A/B絶対stepからの差分として`a_steps`/`b_steps`を作る
- [x] look-ahead batch中に`planned_x_mm`/`planned_y_mm`と並行して`planned_a_steps`/`planned_b_steps`を伝搬する
- [x] block成功後の`MachineState.a_steps/b_steps`は差分加算ではなくblockの絶対targetへ代入する
- [x] no-op判定を`a_steps == 0 && b_steps == 0`へ変更し、mm長だけで微小stepを捨てない
- [x] HOME、ZERO、JOB_BEGINでdrift検出用backend基準を揃える
- [x] XY batch完了時にMachineStateとbackendのジョブ開始基準相対A/B差分を比較し、不一致なら`WARN: DRIFT ...`を出す

### 9.2.3 CommandQueue満杯時の無音ドロップ防止

- [x] G-code/XY等のmotion関連コマンドはCommandQueue満杯でも破棄せず、投入成功まで待つ
- [x] ABORT/JOB_ABORTの即時停止要求flag設定は維持する
- [x] input line overflow後は次の改行まで読み捨て、行後半を別コマンドとして扱わない

### 9.2.4 回帰テスト・確認

- [x] ホストnativeテスト`test/native/test_motion_drift.cpp`を追加する
- [x] ランダム微小閉路で絶対step差分の累積A/Bが0へ戻ることを確認する
- [x] `SegmentGenerator`出力segment合計が`MotionBlock.a_steps/b_steps`に厳密一致することを確認する
- [x] `tools/run_native_motion_tests.sh`でnativeテストを実行できる
- [x] `platformio.ini`へ`m5stack-core2-sim`環境を追加し、SIMULATION_MODE buildを明示実行できる
- [x] `pio run`、`pio run -e m5stack-core2-sim`、`pio run -e m5stack-core2 --target upload`が成功
- [x] 実機Serialで`CONFIG`、`POS`、`SELFTEST`、`TMC_INIT`、`TMC_STATUS`が成功
- [ ] 実G-code stream jobで長時間描画し、`WARN: DRIFT`が出ないことと閉じ位置を確認する

---

# Phase 10: look-ahead / junction deviation

Status: 実装済み。実機確認は未完了。

## チェックリスト

- [x] `JunctionPlanner`を実装
- [x] junction speed計算
- [x] reverse pass
- [x] forward pass
- [x] junction deviation
- [x] classic jerk相当の制限を検討
- [x] CoreXY motor-space速度制限
- [x] PlannerQueue内の複数MotionBlockを対象にする
- [x] StepperBackendに実装しない
- [x] `LOOKAHEAD_BATCH_COLLECT_MS`で連続XYを短いバッチへまとめる
- [x] `CONFIG`にjunction deviation、classic jerk、batch collect時間、PlannerQueue容量を表示する
- [x] `tools/serial_tool/examples/lookahead_check.csv`を追加する
- [x] `tools/serial_tool/docs/lookahead-check.md`を追加する
- [ ] 実機で`lookahead_check.csv`を`--queue-mode`で実行し、`LOOKAHEAD blocks>1`を確認する
- [ ] 実機で角の丸まり、閉じズレ、脱調、温度上昇が増えないか確認する

---

# Phase 10.5: Job Lifecycle / G-code起動終了処理

Status: 実装済み。`JOB_BEGIN`はTMC未ready時に`TMC_INIT`相当を自動実行し、`JOB_BEGIN_AUTO_HOME=true`なら未homed時にHOME相当を自動実行する。成功時のhomed検証結果はジョブ中のG-code由来XY移動ゲートに使う。`JOB_END`はpen up後に`X=5mm, Y=Y_MAX_MM-5mm`へ退避し、A/B両モータの短い8-bit風和音ジングルを鳴らす。`JOB_BEGIN_AUTO_HOME`追加後の`pio run`、upload、`CONFIG`/`SELFTEST`/`TMC_STATUS`確認は成功。motionを伴う`job_lifecycle_check.csv`実機確認と、auto_home=trueの実機確認は未完了。

## 目的

正式描画入力をG-code基本にするため、G-codeファイル本文やホスト側`gcode_preamble.csv`に起動処理・終了処理を毎回書かせない。
ファームウェア側にジョブ開始/終了の状態機械を持たせ、bring-up用コマンドと正式ジョブ経路を分離する。

## 方針

- 電源投入時は安全なidle状態へ初期化するだけにし、自動homing、自動移動、自動メロディは行わない
- bring-upでは従来通り`SELFTEST`、`TMC_INIT`、`ZERO`、`ALARM_CLEAR`、`HOME`、`PENUP`を明示実行する
- 正式ジョブでは、G-code本文の前後に`SELFTEST`、`TMC_INIT`、`ALARM_CLEAR`、`LIMIT_STATUS`、`ZERO`、`CONFIG`を入れない
- ジョブ開始時の安全確認、ジョブ終了時のpen up、退避移動、終了ジングル、queue drainはファームウェア側で行う
- `G0/G1/M3/M5/G4/G28/M114`など描画意味を持つG-codeはそのまま維持する
- `XY`、`TEST_A`、`TEST_B`、`AB_TIMED`、`MELODY`は診断/bring-up用のままにする

## 初期実装範囲

最初はSerial経由の明示コマンドでJob Lifecycleを確認する。
SD/Web/USB streaming統合はPhase 11以降に回す。

追加候補コマンド:

| コマンド | 内容 |
|---|---|
| `JOB_BEGIN` | 正式G-codeジョブ開始。開始前確認を行い、成功時だけjob runningへ遷移 |
| `JOB_END` | 正式G-codeジョブ終了。pen up、退避移動、終了ジングル、queue drain、状態出力を行う |
| `JOB_ABORT` | ジョブ文脈つき中断。既存`ABORT`と同じ停止経路を使い、job状態、job result、last errorをabortedへ遷移 |
| `JOB_STATUS` | job状態、開始前確認結果、最後の終了理由を表示 |

将来、`serial_send.py --gcode`は、正式運用では`JOB_BEGIN`を送ってからG-code本文を送り、最後に`JOB_END`を送る。
bring-upでは引き続き`--preamble-csv`を使ってよい。

`ABORT`と`JOB_ABORT`の使い分け:

| コマンド | 位置付け | 主な用途 | 処理 |
|---|---|---|---|
| `ABORT` | 低レベル即時停止 | bring-up、手動操作、homing中、危険時の停止 | motion/homing/stepper停止、alarm遷移、homed無効化 |
| `JOB_ABORT` | Job Lifecycle上の中断 | G-codeジョブ送信ツール、正式ジョブ中断 | `ABORT`と同じ停止経路を呼び、追加でjob状態を`ABORTED`、job result/last errorを中断扱いにする |

`JOB_ABORT`は`ABORT`の代替ではなく、ジョブ管理層から見た中断ラッパーとする。
ジョブ中に手動`ABORT`が来た場合も、停止経路は即時に実行し、JobControllerはjob状態を`ABORTED`へ寄せる。
ジョブ外で`JOB_ABORT`が来た場合は、初期実装では`JOB_ABORT rejected reason=no_active_job`を返し、低レベル停止は行わない。

## 実装配置

`JOB_BEGIN`/`JOB_END`の本体はCore 1のmotion層に置く。
Core 0の`CommandTask`、`CommandDispatcher`、`main.cpp`、`GcodeParser`には処理本体を書かない。

配置方針:

| ファイル | 責務 |
|---|---|
| `include/JobController.h` | Job state、開始/終了結果、開始前確認API、診断コマンド拒否判定を宣言 |
| `src/JobController.cpp` | `JOB_BEGIN`/`JOB_END`の状態遷移、安全確認、pen up、ログ整形の中心実装 |
| `include/CommandTypes.h` | `JOB_BEGIN`、`JOB_END`、`JOB_ABORT`、`JOB_STATUS`のenum追加 |
| `src/CommandDispatcher.cpp` | 文字列を`CommandMessage`へ変換するだけ。Job処理はしない |
| `include/AppContext.h` / `src/main.cpp` | `JobController job_controller`のextern/実体定義と初期化 |
| `src/tasks/MotionTask.cpp` | `CommandType::JOB_*`を受け、MotionTask内ローカルqueueやG-code modal状態と`JobController`を接続 |
| `src/Diagnostics.cpp` | `JOB_STATUS`または`CONFIG`向けの表示を追加 |
| `tools/serial_tool/*` | job lifecycle付きG-code送信と検査CSV/手順書 |

`MotionTask.cpp`に残す処理:

- `CommandQueue`から`JOB_BEGIN`/`JOB_END`を受けるswitch case
- `planner_queue`、`segment_queue`、`pending_command`などMotionTask内ローカル状態の空確認
- `GcodeInterpreter`のmodal状態をjob開始時既定値へ戻す呼び出し
- stream済みG-codeが残る場合のflush方針に沿ったqueue整理
- `JobController`へ渡すpreflight結果の組み立て

`JobController.cpp`へ移す処理:

- job state machine
- `JOB_BEGIN`の開始前確認順序
- `JOB_END`の終了処理順序
- `JOB_ABORT`時のjob状態遷移
- `RUNNING`中に許可/拒否するコマンド種別判定
- `JOB_BEGIN OK`、`JOB_END OK`、`JOB_BEGIN rejected reason=...`などのログ文言

`JobController`はFastAccelStepperやTMCStepperを直接触らない。
既存の抽象層である`StepperBackendFastAccel`、`TMC2209Manager`、`PenController`、`SafetyManager`、`MachineState`を受け取って処理する。
Core 0のUIやLCDには触らず、状態表示は`MachineState`/`StatusQueue`/`Diagnostics`経由にする。

## 状態設計

`MachineState`または専用`JobState`で以下を追跡する。

| 状態 | 意味 |
|---|---|
| `IDLE` | ジョブなし。手動/診断コマンドを受けられる |
| `STARTING` | `JOB_BEGIN`処理中 |
| `RUNNING` | G-codeジョブ実行中 |
| `ENDING` | `JOB_END`処理中 |
| `COMPLETE` | 正常終了直後 |
| `ABORTED` | 中断停止後 |
| `FAILED` | job実行中または終了処理中のalarm、limit、queue異常など |

状態遷移ルール:

- `IDLE`以外で`JOB_BEGIN`された場合は拒否する
- `RUNNING`中の`XY`、`TEST_A`、`TEST_B`、`AB_TIMED`、`MELODY`は拒否する
- `RUNNING`中のG-code motion、pen、dwell、statusは許可する
- alarm発生時は`FAILED`または`ABORTED`へ遷移し、後続motionを拒否する
- `JOB_BEGIN`の開始前確認拒否はjob状態を`IDLE`へ戻し、`last_error`/`result`で拒否理由を残す
- `FAILED`/`ABORTED`に残っていても、alarmが解除済みなら次の`JOB_BEGIN`前に`IDLE`へ復帰できる
- `RUNNING`中に`JOB_ABORT`または`ABORT`を受けた場合は、共通停止経路を使いjob状態を`ABORTED`へ遷移する
- `JOB_END`完了後は`COMPLETE`をログし、次の操作前に`IDLE`へ戻せる

コマンド許可方針:

| Job状態 | 許可 | 拒否 |
|---|---|---|
| `IDLE` | bring-up/診断コマンド、`JOB_BEGIN`、状態表示 | なし。ただしalarm中motionは従来通りSafetyManagerで拒否 |
| `STARTING` | `ABORT`のみ | motion、pen、diagnostics変更系、追加`JOB_BEGIN` |
| `RUNNING` | `GCODE`、G-code由来`XY`、`DWELL`、`PEN_UP`、`PEN_DOWN`、`POS`、`M114`相当、`JOB_END`、`JOB_ABORT`、`ABORT` | 手入力`XY`、`TEST_A/B`、`AB_TIMED`、`MELODY`、`ZERO`、`ALARM_CLEAR`、`TMC_INIT`、`HOME` |
| `ENDING` | `ABORT`のみ | motion、pen、diagnostics変更系、追加`JOB_END` |
| `COMPLETE` | `JOB_STATUS`、`POS`、次の`JOB_BEGIN`へ戻すためのidle遷移 | motion開始は一度`IDLE`へ戻してから |
| `FAILED`/`ABORTED` | `JOB_STATUS`、`POS`、復旧用`ZERO -> ALARM_CLEAR -> HOME` | alarm中の新規`JOB_BEGIN`。alarm解除済みなら次の`JOB_BEGIN`前に`IDLE`復帰可 |

G-code由来の`XY`と手入力`XY`は区別する。
`GcodeInterpreter`から変換された`XY`には、`CommandMessage`内の既存情報または追加フラグで`source=GCODE`を保持する。
初期実装でフラグ追加が重い場合は、`MotionTask`でG-code翻訳直後に直接`handleXYBatch()`へ渡す経路のみを`RUNNING`中許可し、Serial由来の裸`XY`は拒否する。

## `JOB_BEGIN`処理順

1. `JobController`が`IDLE`であることを確認する
2. `MotionTask`内の`pending_command`が空であることを確認する
3. `planner_queue`、`segment_queue`が空であることを確認する
4. `stepper_backend.isRunning()`がfalseであることを確認する
5. `safety_manager.poll()`でlimit raw/debouncedとalarm状態を更新する
6. alarmがないことを確認する
7. hard limitが通常移動禁止状態でactiveになっていないことを確認する
8. TMC readyを確認する
9. TMC未readyなら`TMC_INIT`相当を自動実行する。失敗した場合は`tmc_not_ready`で拒否する
10. homed状態を確認する
11. `JOB_BEGIN_AUTO_HOME=false`で未homedなら`not_homed`で拒否し、job状態は`IDLE`へ戻す
12. `JOB_BEGIN_AUTO_HOME=true`で未homedなら、`MotionTask`が`HOME`相当を自動実行する。失敗した場合は`auto_home_failed`で拒否する
13. `JOB_BEGIN`でhomed確認済みであることを`JobController`に保持し、`RUNNING`/`ENDING`中のG-code由来XY移動のhomedゲートに使う
14. penを上げ、`machine_state.pen_down=false`へ更新する
15. `GcodeInterpreter`のmodal状態を`G21`、`G90`、既定feedへ初期化する
16. job counters、last error、job resultを初期化する
17. job状態を`RUNNING`へ遷移し、`JOB_BEGIN OK`をログする

`JOB_BEGIN_AUTO_HOME`の初期値は安全側の`false`とする。
`true`にする場合は、limit switch、homing方向、E-stop可能な作業状態を実機で確認してから有効化する。

## `JOB_END`処理順

1. `JobController`が`RUNNING`であることを確認する
2. `MotionTask`が受信済みmotionを最後まで処理した状態であることを確認する
3. `planner_queue`、`segment_queue`、`pending_command`が空であることを確認する
4. `stepper_backend.isRunning()`がfalseになるまで待つ
5. penを上げ、`machine_state.pen_down=false`へ更新する
6. `JOB_END_PARK_ENABLED`なら、既存XY/planner/timed segment経路で`JOB_END_PARK_X_MM=5.0`、`JOB_END_PARK_Y_MM=Y_MAX_MM-5.0`へ移動する
7. `JOB_END_JINGLE_ENABLED`なら、A/B両モータを使う短いオリジナル8-bit風和音ジングルを鳴らす
8. `safety_manager.poll()`で終了時limit/alarm状態を更新する
9. MachineState、limit、alarm、TMC ready、job resultをログする
10. job状態を`COMPLETE`へ遷移し、`JOB_END OK`をログする
11. 次の`JOB_BEGIN`を受ける前に`IDLE`へ戻す。初期実装では`JOB_END OK`ログ後に自動idle遷移する

終了処理中にalarmまたはABORTが入った場合は、pen upを試行したうえで`FAILED`または`ABORTED`へ遷移する。
退避移動または終了ジングルに失敗した場合は`JOB_END failed reason=park_failed`または`JOB_END failed reason=jingle_failed`を出し、job状態を`FAILED`へ遷移する。

`JOB_END`はG-code streamの最後にホストから明示送信する。
ファームウェアがG-codeファイル終端を直接知るわけではないため、Serial運用では「最後の行を送ったあとに`JOB_END`を送る」ことを正式手順とする。

## Serial Tool計画

- [x] `tools/serial_tool/examples/job_lifecycle_check.csv`を追加する
- [x] `tools/serial_tool/docs/job-lifecycle-check.md`を追加する
- [x] `tools/serial_tool/README.md`のCheck一覧へリンクを追加する
- [x] `serial_send.py --gcode --job-lifecycle`を追加し、`JOB_BEGIN`と`JOB_END`を自動で前後送信できるようにする
- [x] `--preamble-csv`はbring-up/暫定確認用として残す
- [x] `JOB_BEGIN`失敗時はG-code本文を送らない
- [x] G-code本文中のfailure検出時は後続行を送らず、`JOB_ABORT`または`ABORT`へつなぐ

## チェックリスト

- [x] `JobState`または同等の状態保持を追加する
- [x] `CommandType::JOB_BEGIN`、`JOB_END`、`JOB_ABORT`、`JOB_STATUS`を追加する
- [x] `CommandDispatcher`はjob commandをparseするだけで、motionを直接実行しない
- [x] `JobController`を新設し、Job state machineと開始/終了処理を`MotionTask`から分離する
- [x] `MotionTask`側でJob Lifecycleを処理する
- [x] `MotionTask`にはqueue空確認、G-code modal reset、`JobController`呼び出しだけを残す
- [x] Core 0からstepper、TMC、penを直接操作しない
- [x] Core 1からLCDを直接描画しない
- [x] `JOB_BEGIN`でalarm、TMC ready、homed、pen up、queue emptyを確認する
- [x] `JOB_BEGIN`でTMC未readyなら`TMC_INIT`相当を自動実行し、失敗時だけ拒否する
- [x] `JOB_BEGIN`で未homedなら初期実装では拒否する
- [x] `JOB_BEGIN_AUTO_HOME`を追加し、true時は未homedの`JOB_BEGIN`でHOME相当を自動実行する
- [x] `JOB_BEGIN`の開始前拒否は`FAILED`に残さず`IDLE`へ戻し、次の`JOB_BEGIN`を再試行可能にする
- [x] `JOB_BEGIN`で確認したhomed状態をJobControllerに保持し、ジョブ中のG-code由来XY移動のhomed判定に使う
- [x] `JOB_BEGIN`でG-code modal状態をmm/absoluteなど既定値へ初期化する
- [x] `JOB_END`でpen up、退避移動、終了ジングル、queue drain、status/log出力を行う
- [x] `JOB_END_PARK_X_MM=5.0`、`JOB_END_PARK_Y_MM=Y_MAX_MM-5.0`をconfig化しsoft limit内にstatic_assertする
- [x] 終了ジングルはA/B両モータの和音として実装し、既存曲そのものではない短い8-bit風オリジナル音列にする
- [x] `RUNNING`中の診断コマンドを拒否する
- [x] `RUNNING`中はG-code由来`XY`だけを許可し、Serial手入力の裸`XY`は拒否する
- [x] `JOB_ABORT`は既存`ABORT`と同じ停止経路を使い、追加でjob状態/result/last errorを`ABORTED`へ遷移する
- [x] `RUNNING`中に手動`ABORT`を受けた場合も、JobControllerがjob状態を`ABORTED`へ寄せる
- [x] ジョブ外の`JOB_ABORT`は`no_active_job`として拒否し、低レベル停止は行わない
- [x] alarm、limit、ABORT発生時にjob状態が失敗系へ遷移する
- [x] `CONFIG`または`JOB_STATUS`でjob状態を確認できる
- [x] `SPEC.md`へ確定したコマンド名、状態、応答ログを反映する
- [x] `PLANS.md`のリスクと実機確認結果を更新する
- [x] `pio run`を実行する
- [x] `pio run --target upload`を実行する
- [ ] 実機で`JOB_BEGIN -> 短いG-code -> JOB_END`を確認する
- [ ] 実機で未homed時の`JOB_BEGIN`拒否を確認する
- [ ] 実機でalarm中の`JOB_BEGIN`拒否を確認する
- [ ] 実機でG-code中断時に`JOB_ABORT`または`ABORT`で安全停止することを確認する

## 未解決判断

- [x] `JOB_BEGIN`でTMC未ready時は自動`TMC_INIT`する。失敗時だけ拒否する
- [x] `JOB_BEGIN`で未homed時に自動`HOME`するか、拒否するかは`JOB_BEGIN_AUTO_HOME`で切り替える。既定は安全側のfalse
- [x] `JOB_END`はpen up後に`X=5mm, Y=Y_MAX_MM-5mm`へ退避移動し、その後に終了ジングルを鳴らす
- [x] `COMPLETE`からの復帰は、初期実装では`JOB_END OK`ログ後に自動で`IDLE`へ戻す
- [ ] stream済みG-codeがalarm後にCommandQueueへ残る場合、Job Lifecycle側でflushする範囲を決める

---

# Phase 11: 高級機能

Status: 一部着手。Host WebUIはPC/Raspberry Pi側で動かし、M5Stack Core2本体にはWeb serverを載せない。2026-06-13時点の棚卸しで、Phase 7/10.5/11.1で完了済みの項目を親チェックリストへ反映した。

## チェックリスト

### Homing / Limit

- [-] Phase 6.9完了後のhoming精度改善
- [x] G28との統合
- [x] homing後の軸別再homing
- [ ] limit switch debounceの実測調整
- [ ] hard limit停止経路のtimed segment対応
- [x] homed前移動制限の運用方針確定
- [-] alarm復帰

棚卸しメモ:

- `G28`はPhase 7で既存`HOME`経路へ接続済み。
- `HOME_X`/`HOME_Y`による軸別homingは実装済みで、Phase 6.9の実機確認も完了済み。
- homing精度改善は、二段階homing、長距離moveをlimit条件で停止する方式、backend現在stepからのMachineState再同期まで完了済み。ただしdebounce実測、switch戻り距離、連続運用での再現性は未完了のため一部完了扱いとする。
- homed前移動制限は`HOMING_REQUIRE_HOMED_FOR_XY_MOVE`で通常XYを禁止し、`JOB_BEGIN`成功時のG-code由来移動だけ`JobController`のhomed grantで許可する運用に確定済み。
- alarm復帰は`ZERO -> ALARM_CLEAR -> HOME`の手順と`ALARM_CLEAR`実装まで完了済み。ただし脱調後・座標ずれ後の実機復旧確認が未完了のため一部完了扱いとする。

### TMC診断

- [x] driver status取得
- [ ] SG_RESULT取得
- [ ] over temperature警告
- [ ] open load診断
- [x] UART通信失敗検出

棚卸しメモ:

- `TMC_STATUS`で`test_connection()`、`IFCNT`、microsteps、rms current、`irun`、`ihold`、`iholddelay`、spreadCycle、`toff`を出力済み。
- UART通信失敗は`test_connection()`のA/B connection値と`ready`へ反映済み。
- `SG_RESULT`、over temperature、open loadは専用レジスタ診断・警告ログ未実装。

### 入出力

- [ ] SD実行
- [-] WebUI
- [-] USB G-code streaming
- [ ] file pause/resume
- [-] Phase 10.5 Job LifecycleをSD/Web/USB streamingへ接続する

棚卸しメモ:

- WebUIはHost WebUI MVP、G-code preview、Image to G-code、`serial_send.py`経由の送信、`JOB_ABORT`まで実装済み。PC起動確認とPNG/JPEG実機描画品質確認が未完了のため一部完了扱いとする。
- USB G-code streamingは`serial_send.py --gcode --queue-mode --stream-gcode-motion`でホスト側先行投入を実装済み。長時間・高密度stream描画の実機安定性確認が未完了のため一部完了扱いとする。
- Job Lifecycle接続は`serial_send.py --gcode --job-lifecycle`とWebUI既定optionでWeb/USB経路へ接続済み。SD経路未実装、motionを伴う`JOB_BEGIN -> G-code -> JOB_END`実機確認未完了のため一部完了扱いとする。

## Phase 11.1: Host WebUI

Status: MVP実装中。初期実装はPCからUSB SerialでM5Stack Core2へ送るHost WebUIとする。

### 目的

PC画面で状態、ログ、G-code preview、ジョブ送信を扱えるようにする。送信処理は既存`tools/serial_tool/serial_send.py`を再利用し、WebUI側でSerial送信・ACK待ち・queue retry・Job Lifecycleを再実装しない。

### 11.1.1 Product / UX 方針

- [x] 黒基調で、状態と危険操作が明確に分かるUIにする
- [x] 画面上部に接続状態とmachine stateを常時表示する
- [x] `READY` / `ALARM` / `NEED HOME` / `HOMING`を大きく表示する
- [x] `ALARM`時は赤を主アクセントにし、通常操作より復旧操作を優先表示する
- [x] HOME未完了、状態不明、Serial切断時はjog、pen、job startをdisabledにする
- [x] Job実行中はmanual jogをdisabledにし、`JOB_ABORT`は常にアクセス可能にする

### 11.1.2 画面

- [x] Dashboard: 接続port、state、X/Y、pen、homed、limit、TMC、直近log
- [x] Manual Control: `HOME`、`ALARM_CLEAR`、`PENUP`、`PENDOWN`、上下左右jog、jog step
- [x] Job: G-code file選択、preview、bounds、warning、send、abort
- [x] Console: firmware log、送信command、ACK/NACK/ERROR、手動command入力
- [x] Settings: serial port、baudrate、startup delay、queue mode、stream motion mode

### 11.1.3 Host bridge

- [x] Host bridge serverを追加する
- [x] serial port一覧を取得する
- [x] Browserへ状態とログを配信する
- [x] `serial_send.py`をsubprocessで呼び出す
- [x] WebUIからのG-code送信では既定で`--gcode`、`--queue-mode`、`--stream-gcode-motion`、`--job-lifecycle`を使う
- [x] `serial_send.py`のstdout/stderrをBrowserへstreamする
- [x] `NACK`、`REJECT:`、`ERROR:`、`ALARM=YES`をfailure表示に分類する
- [x] Job失敗時の`JOB_ABORT`送信は`serial_send.py --job-lifecycle`へ委譲する

### 11.1.4 G-code preview

- [x] `G0`/`G1`のXY直線をpreviewする
- [x] `G20`/`G21`、`G90`/`G91`を解釈する
- [x] `M3`/`M5`またはpen相当commandでpen down pathとtravelを色分けする
- [x] `G28`はhome markerとして表示し、pathには含めない
- [x] `G4`はdwell markerとして表示し、pathには含めない
- [x] soft limit矩形を表示する
- [x] file boundsを表示する
- [x] soft limit外segmentをwarning表示する
- [x] 未対応G-codeをwarning一覧へ出す

### 11.1.4.1 Image to G-code

- [x] Job画面下部へ折りたたみ可能な`Image to G-code` panelを追加する
- [x] SVG/PNG/JPEGファイル選択とSVG文字列貼り付けに対応する
- [x] `POST /api/gcode/from-svg`を追加する
- [x] `POST /api/gcode/from-image`を追加する
- [x] PNG/JPEGは直接G-code化せず、plotter-friendly SVGへtraceしてから共通SVG to G-code経路へ通す
- [x] Line Art / Outline Trace、Auto/Otsu / Manual threshold、invert、skeletonize、max segments設定を追加する
- [x] 中間SVGをresponseへ含め、WebUIからdownloadできる導線を追加する
- [x] Image変換中のupload、trace、SVG to G-code、layout追加の進捗と失敗理由をWebUIへ表示する
- [x] `path`、`polyline`、`polygon`、`line`、`rect`、`circle`、`ellipse`をstroke列へ変換する
- [x] `translate`、`scale` transformを処理し、未対応SVG機能はwarningを返す
- [x] bounding box fit、Y反転、短すぎるstroke削除、stroke順最適化を行う
- [x] 生成G-codeを既存layoutへ仮想`.gcode`として追加し、preview、Save G-code、Send Job導線を再利用する
- [x] 変換器の単体テストを追加する
- [x] SVG生成G-codeの実機描画確認
- [ ] PNG/JPEG生成G-codeの実機描画品質確認

### 11.1.5 初期完了条件

- [ ] WebUIをPCで起動できる
- [x] Serial portを選択できる
- [x] G-code previewが表示される
- [x] WebUIから既存`serial_send.py`経由でG-code jobを送れる
- [x] Job中のlog、ACK/NACK/ERRORが画面へstreamされる
- [x] `JOB_ABORT`をWebUIから実行できる
- [x] `PLANS.md`と`SPEC.md`が実装内容に追従している

### 補正

- [ ] input shaping
- [ ] skew correction
- [ ] backlash compensation
- [ ] steps/mm calibration
- [ ] pen offset calibration

### 操作性

- [ ] feed override
- [ ] jog command
- [ ] pause/resume
- [ ] emergency stop UI

---

# 12. 手動テスト手順

## 12.1 Simulation

```text
HELP
CONFIG
POS
SELFTEST
ZERO
XY 10 0
ZERO
XY 0 10
ZERO
XY 10 10
XY -1 0
XY 301 0
XY 0 301
XY 10 10 0
TMC_STATUS
```

チェック:

- [ ] `SELFTEST PASS`
- [ ] `XY 10 0`でA=800 B=800
- [ ] `XY 0 10`でA=800 B=-800
- [ ] `XY 10 10`でA=1600 B=0
- [ ] soft limit違反が拒否される
- [ ] feed 0が拒否される
- [ ] simulationではモータが動かない

## 12.2 TMC UART

```text
CONFIG
TMC_INIT
TMC_STATUS
```

チェック:

- [ ] Serial2がTX=14/RX=13で初期化される
- [ ] A address=0
- [ ] B address=1
- [ ] TMC_STATUSが読める、またはplaceholderログが出る

## 12.3 実機モータ

```text
CONFIG
TMC_INIT
TMC_STATUS
ENABLE
TEST_A 200
TEST_A -200
TEST_B 200
TEST_B -200
ZERO
XY 10 0 300
XY 10 10 300
XY 0 10 300
XY 0 0 300
DISABLE
```

チェック:

- [x] Aだけ動く
- [x] Bだけ動く
- [x] +XでA/B同方向
- [x] +YでA/B逆方向
- [x] 異音なし
- [x] 発熱が異常でない
- [x] 脱調しない
- [x] リミット異常なし

## 12.4 NEOPIXEL

```text
LED 255 0 0
LED 0 255 0
LED 0 0 255
LED_PIXEL 0 255 255 255
LED_PATTERN PACIFICA
LED_PARAM BRIGHTNESS 32
LED_PARAM HUE 96
LED_PARAM SPEED 20
LED_PATTERN FIRE
LED_PARAM COOLING 55
LED_PARAM SPARKING 120
LED_STATUS
LED_OFF
```

チェック:

- [x] 赤、緑、青が順に点灯する
- [x] `LED_PIXEL`で指定indexだけを点灯できる
- [x] 範囲外indexが拒否される
- [x] `PACIFICA`と`FIRE`を選択して切り替えられる
- [x] brightness、hue、speed、cooling、sparkingが対応パターンへ反映される
- [x] `LED_STATUS`で選択中patternとparameterを確認できる
- [x] `LED_OFF`で消灯する
- [x] LED更新中もmotion処理が不必要に停止しない

## 12.5 診断用モータメロディ

```text
CONFIG
TMC_INIT
TMC_STATUS
ENABLE
MELODY
TMC_STATUS
DISABLE
```

チェック:

- [x] motion idle時だけメロディを実行できる
- [x] メロディ中だけSTEP周波数、電流、microstep、chop modeが変更される
- [x] 正常終了後に通常TMC profileへ復元される
- [x] limitまたはalarm中断後にも通常TMC profileへ復元される
- [x] 論理X/Y位置が変化しない
- [x] 異常発熱がない

## 12.6 Homing bring-up

```text
CONFIG
POS
HOME_X
POS
HOME_Y
POS
ZERO
POS
HOME
POS
XY 10 0 300
XY 10 10 300
```

チェック:

- [ ] `CONFIG`でhoming設定値を確認できる
- [ ] `LIMIT_STATUS`または`POS`でX/Y limit入力を確認できる
- [ ] `HOME_X`でX limit方向へのhoming sequenceを開始する
- [ ] `HOME_X`で最初に速いseekでX limit ONを検出する
- [ ] X limit検出後にbackoffして低速再検出する
- [ ] Xのbackoff中にX limitがOFFになる
- [ ] Xの低速seekで2回目のX limit ONを検出し、その位置をX原点にする
- [ ] `HOME_Y`でY limit方向へのhoming sequenceを開始する
- [ ] `HOME_Y`で最初に速いseekでY limit ONを検出する
- [ ] Y limit検出後にbackoffして低速再検出する
- [ ] Yのbackoff中にY limitがOFFになる
- [ ] Yの低速seekで2回目のY limit ONを検出し、その位置をY原点にする
- [ ] `HOME`でX/Yを順番にhomingする
- [ ] homing後に`POS`が原点座標と`HOMED=YES`を表示する
- [ ] homing完了時にlimit debouncedがONである
- [ ] `ZERO`は論理原点リセットであり、homed状態を勝手にtrueにしない
- [ ] limitが最初からONでも、backoffしてから低速seekへ進む
- [ ] backoff距離内にlimitがOFFにならない場合はalarmになる
- [ ] homing対象外limit activeでalarmになる
- [ ] max travel超過でalarmになる
- [ ] alarm中に通常XY移動が拒否される
- [ ] homing後の`XY 10 0 300`と`XY 10 10 300`が期待方向に動く

## 12.7 中心図形描画 / 脱調確認

```text
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/center_shapes.csv \
  --startup-delay 4 \
  --startup-drain 1 \
  --timeout 8 \
  --echo
```

チェック:

- [x] `CONFIG`で`accel=100.000`を確認できる
- [x] `TMC_STATUS`で通常profileの電流設定を確認できる
- [x] `HOME`が完了する
- [x] マル、四角、三角、星の描画コマンドが最後までACKされる
- [x] 最終`POS`で`ALARM=NO`を確認できる
- [x] 最終`POS`で`LIMIT_X=OPEN`、`LIMIT_Y=OPEN`を確認できる
- [ ] 実際の線が目視で大きくずれない
- [ ] 連続実行後もモータ/TMC温度が許容範囲に収まる

## 12.7.1 同心正方形描画 / 動き出し・動き終わり歪み確認

```text
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/concentric_squares_check.csv \
  --startup-delay 4 \
  --startup-drain 1 \
  --timeout 12 \
  --echo
```

時計回り:

```text
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/concentric_squares_clockwise_check.csv \
  --startup-delay 4 \
  --startup-drain 1 \
  --timeout 12 \
  --echo
```

高速版:

```text
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/concentric_squares_high_speed_check.csv \
  --startup-delay 4 \
  --startup-drain 1 \
  --timeout 12 \
  --queue-mode \
  --echo
```

チェック:

- [ ] `HOME`が完了する
- [ ] 5個の正方形描画コマンドが最後までACKされる
- [ ] 各正方形の始点角に欠け、丸まり、ペン引っかかりがない
- [ ] 各正方形の終点角に伸び、ずれ、オーバーシュートがない
- [ ] 水平辺と垂直辺で歪みの出方に差があるか確認する
- [ ] 反時計回りと時計回りで歪み位置が入れ替わるか確認する
- [ ] 通常版と高速版で歪み、閉じズレ、脱調、温度上昇に差があるか確認する
- [ ] 高速版は`--queue-mode`で実行し、`CommandQueue full`が継続せず最後まで送信できる
- [ ] サイズ違いで歪みの出方に差があるか確認する
- [ ] 最終`POS`で`ALARM=NO`、`LIMIT_X=OPEN`、`LIMIT_Y=OPEN`を確認できる

## 12.7.2 AB_TIMED四角描画 / backend直接timed実行確認

目的:

通常XY描画CSVと`AB_TIMED`描画CSVを比較し、歪み原因がplanner/segment生成側か、backend/FastAccelStepper側かを切り分ける。

通常XY比較対象:

- `tools/serial_tool/examples/concentric_squares_clockwise_check.csv`
- `tools/serial_tool/examples/concentric_squares_check.csv`
- `tools/serial_tool/examples/concentric_squares_high_speed_check.csv`

AB_TIMED診断CSV:

- `tools/serial_tool/examples/diagnostic_ab_timed_square_draw.csv`

実行:

```text
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/diagnostic_ab_timed_square_draw.csv \
  --startup-delay 4 \
  --startup-drain 1 \
  --timeout 8 \
  --echo
```

判定基準:

- [ ] 通常XY描画では歪むが、AB_TIMED描画では四角が閉じる場合、`TrapezoidPlanner`、`SegmentGenerator`、`SegmentQueue`、またはXYコマンド処理側を疑う
- [ ] AB_TIMED描画でも歪む場合、`StepperBackendFastAccel`、FastAccelStepper `moveTimed()`使用方法、A/B同期開始、`duration_us`指定を疑う
- [ ] AB_TIMEDで小さい四角だけ悪化する場合、短距離`moveTimed()`、`duration_us`最小値、step数丸め、A/B step配分を疑う
- [ ] AB_TIMEDで大きい四角は正常、小さい四角はズレる場合、台形加速以前に短時間timed moveの扱いを疑う
- [ ] 時計回りと反時計回りでズレ方向が変わる場合、A/B符号、方向反転、または片側モータの開始/停止タイミング差を疑う
- [ ] 通常XYもAB_TIMEDも同じように歪む場合、backend以下の問題が濃厚。ソフト観点では`StepperBackendFastAccel`とFastAccelStepper設定を重点確認する

## 12.7.3 Look-ahead / junction deviation確認

Phase 10の連続XYバッチとjunction速度を確認する。

```text
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/lookahead_check.csv \
  --startup-delay 4 \
  --startup-drain 1 \
  --timeout 8 \
  --queue-mode \
  --echo
```

チェック:

- [ ] `CONFIG`で`LOOKAHEAD junction_deviation=`を確認できる
- [ ] 連続XYで`LOOKAHEAD blocks=`が2以上になる
- [ ] `XY batch=`ログに`entry=`と`exit=`が出る
- [ ] 各XYが`TRAPEZOID`、`SEGMENTS count=`、`ACK_XY`まで進む
- [ ] 角で停止しないが、角の丸まりが許容範囲に収まる
- [ ] 閉じズレ、脱調、温度上昇がPhase 9単独時より悪化しない

## 12.8 Serial Tool待ち時間仕様

チェック:

- [x] `--timeout`は各コマンド応答の最大待ち時間として扱う
- [x] `--timeout`の既定値は30秒とする
- [x] timeout時は、その時点までに受信したSerialログを表示する
- [x] 起動ログ読み捨て時間は`--startup-drain`で指定できる
- [x] `--startup-delay 0 --timeout 60`でも、最初のコマンド送信前に60秒待たない
- [x] `HOME`行はCSV `delay_ms`を短くし、`expect=HOME complete`と長めの`--timeout`で完了待ちできる
- [x] high-speed / homing系CSVのHOME行を、固定長待ちからexpect主体の短い`delay_ms`へ整理する
- [x] 各CSV行の実行開始時に、最初のコマンド開始を0とした`TIMING START`を表示する
- [x] 各CSV行の実行終了時に、相対時刻、行内経過時間、status付きの`TIMING END`を表示する
- [x] Serial ToolがCSV `XY`、G-code `G0/G1`、`G4`の実行時間を概算し、`推定motion時間 + --motion-timeout-margin`でtimeoutを自動延長する
- [x] stream motion先行投入時は、累積した推定motion時間を次の非stream行のtimeoutへ足す

## 12.9 脱調後の再homing復旧順序

チェック:

- [x] HOMEを扱うCSVでは`ALARM_CLEAR`の前に`ZERO`を入れる
- [x] `ZERO`で脱調後の古い論理座標とhomed状態を破棄してからalarmを解除する
- [x] `ZERO -> ALARM_CLEAR -> HOME`の順にして、原点外limit active alarmが再発しにくい復旧順序にする
- [x] HOME開始時またはSeekFast中に対象limit raw/debouncedのどちらかがONなら、seek方向へ押し込まず即Backoffする
- [x] Backoff完了判定は対象limit raw/debouncedの両方がOFFになってからSeekSlowへ進む
- [x] Homingのfast seek/backoff/slow seekは短い固定距離move反復ではなく、長距離moveをlimit条件で停止する方式にする
- [x] Homing停止後のMachineStateはFastAccelStepperのA/B現在ステップ差分から更新する
- [ ] 実機で脱調後または意図的な座標ずれ後に、`ZERO -> ALARM_CLEAR -> HOME`で復旧できることを確認する

## 12.10 Serial Tool中断時のABORT仕様

`serial_send.py`を`Ctrl-C`で止めた場合、Python側だけが終了してファームウェア側のmotion/homingが残ると、次回コマンドが戻らないように見える。
追加仕様として、中断時はserial portを閉じる前に`ABORT`を送信し、ファームウェアは実行中motion/homingを停止してalarmへ遷移する。

チェック:

- [x] Serial command `ABORT`を追加する
- [x] `ABORT`はcommandTaskで即時停止要求flagを立てる
- [x] motion/timed segment実行中に停止要求flagをpollしてbackendを停止する
- [x] homing実行中に停止要求flagをpollしてbackendを停止する
- [x] `ABORT`後はalarm状態にし、homed状態を無効化する
- [x] `serial_send.py`の`Ctrl-C`時に`ABORT`を送ってからserial portを閉じる
- [ ] 実機で長いXY移動中またはHOME中に`Ctrl-C`し、次回serial commandが応答することを確認する

## 12.10.1 ゼロ距離XY/G-code移動のno-op扱い

maze G-codeの先頭などで、homing後の現在位置と同じ`G0 X0 Y0`が送られる場合がある。
この移動はplannerへ渡す必要がなく、ゼロ距離MotionBlockをJunctionPlannerへ投入すると拒否されるため、ファームウェア側でno-opとしてACKする。

チェック:

- [x] ゼロ距離`XY`/`G0`/`G1`はplanner/segmentへ投入しない
- [x] no-opでもfeedは更新し、`ACK_XY target=(...) A=0 B=0 F=...`を返す
- [x] 実機でmaze G-code先頭の`G0 X0 Y0 F8000`が`NACK_XY reason=planner`にならないことを確認する

## 12.11 KST32B Text Tool / 日本語G-code生成

目的:

KST32Bストロークフォントデータをホスト側で読み、日本語文字列を既存プロッタ用の最小G-codeへ変換する。Inkscape GUI、Hershey Text、SVG変換、vpype連携には依存しない。

チェック:

- [x] `tools/text_tool/kst32b_to_gcode.py`を追加する
- [x] `--font`でKST32B.TXTを指定できる
- [x] `--text`と`--input-file`を排他入力にする
- [x] `--x`、`--y`、`--size`、`--char-spacing`、`--line-spacing`、`--feed`、`--rapid-feed`、`--dwell-ms`、`--flip-y`、`--max-x`、`--max-y`、`--auto-scale-to-fit`、`-o/--output`を実装する
- [x] CSF/1のX/Y move、draw、next-X命令をデコードし、30x32格子をmmへスケーリングする
- [x] ペンアップ移動を`G0`、描画移動を`G1`、ペンダウンを`M3`、ペンアップを`M5`、dwellを`G4 P<ms>`で出力する
- [x] 改行を扱い、次行へ進める
- [x] 未対応文字は警告を出し、既定で代替四角形、`--missing-glyph skip`でスキップできる
- [x] 短い線分を削除せず、線分簡略化や字形変更を行わない
- [x] サンプル入力`text_robo.txt`、`text_konnichiwa.txt`、`text_dakuten.txt`を追加する
- [x] サンプルG-codeを`tools/text_tool/examples/gcode/`へ追加する
- [x] `G4 P<ms>`をファームウェアの最小G-codeとして追加し、Text Tool既定出力をそのまま送れるようにする
- [x] `serial_send.py --gcode`で生成G-codeを直接送信できるようにする
- [x] `serial_send.py --preamble-csv`で描画前のalarm clear、limit確認、homing確認CSVを前置できるようにする
- [x] `--gcode`行に既定expectを付け、`NACK`、`REJECT:`、alarm、`ERROR:`受信時に停止する
- [x] Text Toolで直前位置と同じ座標へのペンアップ`G0`を省略し、plannerのゼロ長XY拒否を避ける
- [x] Text Toolで`--max-x`/`--max-y`範囲検査と`--auto-scale-to-fit`自動縮小を追加し、サンプルG-codeを55x55mm範囲内へ再生成する
- [x] `serial_send.py --stream-gcode-motion`を追加し、G-code由来の`G0/G1`を`ACK QUEUED`確認で先行投入できるようにする
- [x] stream対象の`G0/G1`は`ACK QUEUED`検出後のserial idle待ちを省き、行別`TIMING`、`--echo`、ACK表示を抑制して送信遅延を減らす
- [x] stream modeでも`M3/M5`、`G4`、`G28`、`M114`、modal G-codeは従来通り完了ログを待つ
- [x] `serial_send.py --stream-xy-motion`を追加し、CSV由来の`XY`も`ACK QUEUED`確認で先行投入できるようにする
- [x] stream対象のCSV `XY`は`ACK QUEUED`検出後のserial idle待ちを省き、行別`TIMING`、`--echo`、ACK表示を抑制して送信遅延を減らす
- [x] CSF/1のX move命令を現在Y上の即時ペンアップ移動として扱い、`高`の上点やASCII `l`が斜め線へ化けないよう修正する
- [ ] 生成G-codeを実機へ送信し、濁点、半濁点、小さい文字、dwell、feedを調整する

---

# 13. Codex用プロンプト

## Prompt 1: Core2土台

```text
AGENTS.md、SPEC.md、PLANS.mdを読んでください。

Phase 0 と Phase 0.5 を実装してください。

完了したらPLANS.mdの該当チェックボックスを更新してください。

目的:
- M5Stack Core2前提のピン定義を作る
- Core2PinMap.hを作る
- TaskConfig.hを作る
- CONFIGでピンとCore割り付けを表示できるようにする

まだ以下は実装しないでください。
- G-code parser
- look-ahead
- junction deviation
- timed segment
- WebUI
```

## Prompt 2: Simulation CoreXY

```text
AGENTS.md、SPEC.md、PLANS.mdを読んでください。

Phase 1を実装してください。

CoreXYKinematicsを実装し、SELFTESTとXY simulationを追加してください。
SIMULATION_MODE=1ではモータを絶対に動かさないでください。

完了したらPLANS.mdの該当チェックボックスを更新してください。
```

## Prompt 3: TMC UART

```text
AGENTS.md、SPEC.md、PLANS.mdを読んでください。

Phase 2を実装してください。

TMC2209Managerを作成し、Serial2 GPIO14/13を使ってTMC2209 A/BをUARTアドレス0/1で扱う構造を作ってください。
TMC_INITとTMC_STATUSを追加してください。

モータ駆動はまだ行わないでください。

完了したらPLANS.mdの該当チェックボックスを更新してください。
```

## Prompt 4: FastAccelStepper bring-up

```text
AGENTS.md、SPEC.md、PLANS.mdを読んでください。

Phase 3を実装してください。

StepperBackendFastAccelを作成し、A/BモータをFastAccelStepperで個別に動かすTEST_A/TEST_Bを追加してください。
FastAccelStepperの詳細はStepperBackendFastAccelに閉じ込めてください。

完了したらPLANS.mdの該当チェックボックスを更新してください。
```

## Prompt 5: XY低速移動

```text
AGENTS.md、SPEC.md、PLANS.mdを読んでください。

Phase 4とPhase 5を実装してください。

XYコマンドでCoreXY低速移動できるようにしてください。
SafetyManager、Diagnostics、POS、CONFIG、limit入力読み取りを追加してください。

まだplanner queue実行やlook-aheadは実装しないでください。

完了したらPLANS.mdの該当チェックボックスを更新してください。
```

## Prompt 6: Planner placeholder

```text
AGENTS.md、SPEC.md、PLANS.mdを読んでください。

Phase 6を実装してください。

MotionBlock、PlannerQueue、TrapezoidPlanner、JunctionPlanner、SegmentGeneratorのplaceholderを作ってください。
将来の挿入場所が分かるコメントを入れてください。

まだ実際のlook-ahead、junction deviation、timed segmentは実装しないでください。

完了したらPLANS.mdの該当チェックボックスを更新してください。
```

## Prompt 7: Core2 LCD Status UI

```text
AGENTS.md、SPEC.md、PLANS.mdを読んでください。

Phase 6.6を実装してください。

Core2内蔵LCDをCore 0のuiTaskから更新してください。
Core 1の機械状態はStatusQueueで受け取り、mode、position、motor、homing、pen、safety、limit、TMC状態を表示してください。
LCD更新でmotion、safety、stepper処理をブロックしないでください。

Touch/Button操作画面はまだ実装しないでください。

完了したらPLANS.mdの該当チェックボックスを更新してください。
```

## Prompt 8: NEOPIXEL Status LED

```text
AGENTS.md、SPEC.md、PLANS.mdを読んでください。

Phase 6.7を実装してください。

GPIO33へ接続した外付けNEOPIXELをNeoPixelControllerで制御してください。
FastLEDを使用し、LED数、輝度上限、初期パターン、frame interval、パターン初期値はPlotterConfig.hから設定できるようにしてください。
ハードウェア出力をNeoPixelController、描画生成をLedPatternEngine、外部設定をLedAnimationConfigに分離してください。
OFF、SOLID、PACIFICA、FIREを選択できるようにし、PACIFICAはFastLEDのPacifica例、FIREはFastLEDのFire2012例を参考にしてください。
LED <r> <g> <b>、LED_PIXEL <index> <r> <g> <b>、LED_OFF、LED_PATTERN、LED_BRIGHTNESS、LED_PARAM、LED_STATUSを追加してください。
brightness、hue、saturation、speed、intensity、cooling、sparking等を外部引数として変更できるようにしてください。
Core 1からNEOPIXEL APIを直接呼ばず、LED更新でmotion処理をブロックしないでください。
delay()やFastLED.delay()でアニメーション更新を待機しないでください。

完了したらPLANS.mdの該当チェックボックスを更新してください。
```

## Prompt 9: Motor Melody Diagnostics

```text
AGENTS.md、SPEC.md、PLANS.mdと../1stepper_testの起動メロディ実装を読んでください。

Phase 6.8を実装してください。

MELODYコマンドでのみ実行する診断用モータメロディを追加してください。
STEP周波数変更はStepperBackendFastAccel経由、TMC2209の電流、microstep、chop mode変更はTMC2209Manager経由に限定してください。
メロディ用profileはPlotterConfig.hから設定できるようにしてください。
delayMicroseconds()でSTEPパルスを直接生成しないでください。
通常motionとは排他実行し、正常終了、中断、alarm、limit検出のすべてで通常TMC profileへ復元してください。

完了したらPLANS.mdの該当チェックボックスを更新してください。
```

## Prompt 10: Homing bring-up

```text
AGENTS.md、SPEC.md、PLANS.mdを読んでください。

Phase 6.9を実装してください。

目的:
- HOME_X、HOME_Y、HOMEを追加する
- X/Y limit入力を使って2段階homingを行う
- 1回目は速いseekでlimit ONを検出する
- backoffでlimit OFFまで戻る
- 2回目は遅いseekでlimit ONを検出し、その位置を原点にする
- homing完了時だけMachineStateの位置とhomed状態を更新する
- homing中、通常motion、MELODY、pen動作を排他にする
- hard limit、max travel超過、対象外limit activeでalarmにする

制約:
- Core 0からモータを直接動かさないでください
- CoreXY変換はCoreXYKinematicsだけで行ってください
- FastAccelStepperはStepperBackendFastAccel内に閉じ込めてください
- STEPパルスをdelayMicroseconds()で直接生成しないでください
- G-code parserとG28はまだ実装しないでください
- look-ahead、junction deviation、timed segmentはまだ実装しないでください
- ../1stepper_testのSeekFast -> Backoff -> SeekSlow -> SetZeroの動作を要件として反映してください
- limit raw/debounced、homing state、error reasonを診断ログに出してください

完了したらPLANS.mdの該当チェックボックスを更新してください。
```

---

# 14. リスク・未解決事項

| ID | 状態 | 内容 | 対応 |
|---|---|---|---|
| R1 | [ ] | 使用するTMC2209モジュールのUARTアドレス設定方法が未確認 | 実物のMS1/MS2/ジャンパ仕様を確認 |
| R2 | [ ] | Core2のM-BUS配線方法が未確定 | 配線図を作成 |
| R3 | [ ] | GPIO35/36の外付けpull-up値が未確定 | 10kΩを初期案として検討 |
| R4 | [ ] | サーボ電源をCore2から取るか外部電源にするか未確定 | 外部5V推奨 |
| R5 | [ ] | FastAccelStepperとM5Unifiedの同時利用での負荷確認が未実施 | Core分離とsimulationで検証 |
| R6 | [ ] | Core2単体で最終性能が足りるか未確定 | 将来外部MCU分離可能な構造を維持 |
| R7 | [ ] | bring-up用`moveABSteps()`実行中はコマンド処理が待機する | 実機bring-up後、hard limit停止とtimed segment実装時に停止経路を追加 |
| R8 | [ ] | 使用するNEOPIXEL LEDの灯数、電圧、信号レベル、配線、電源容量が未確定 | GPIO33接続を初期案とし、実部品と灯数に合わせて確認 |
| R9 | [ ] | メロディ用1200mAがモータ、TMC2209モジュール、電源、放熱条件に適合するか未確認 | 低電流から段階的に確認し、上限を確定 |
| R10 | [ ] | NEOPIXEL灯数、frame rate、FastLED `show()`時間によるCore 0負荷が未確認 | 実灯数で負荷を測定し、frame intervalと輝度を調整 |
| R11 | [x] | TMC2209 profile変更はログ経路までで、実レジスタ書込みが未実装 | `TMCStepper`を用い、`TMC2209Manager`内へA/Bアドレス別レジスタ書込みを追加 |
| R12 | [ ] | limit switchの機械配置、active polarity、bounce量が未確定 | 実配線で`LIMIT_STATUS`を確認し、必要ならdebounceとpolarity設定を追加 |
| R13 | [ ] | homing中の安全停止は現状backendの実行管理能力に依存する | bring-upでは短いincremental move反復で確認し、Phase 9以降でtimed segment停止へ置き換える |
| R14 | [ ] | homed前の通常XY移動を許可するか運用方針が未確定 | 初期はconfigで切替可能にし、実機bring-up後に既定値を決める |
| R15 | [ ] | 2回目の低速seek後にlimit debounced ONで安定するまでの待ち時間が未確定 | `../1stepper_test`同様にraw/debouncedをログ化し、必要ならsettle待ちまたはdebounce値を調整 |
| R16 | [x] | uploadが`/dev/cu.usbserial-023591AC`で`termios.error: (22, 'Invalid argument')`により失敗 | 2026-06-07に同portで`pio run --target upload`成功 |
| R17 | [ ] | 通常TMC電流を850mAへ上げたため、モータ/TMC2209/電源の発熱余裕が未確定 | center shapes連続実行後に温度を確認し、熱い場合は800mA以下へ下げる |
| R18 | [ ] | 原点外でX limitが継続ACTIVEになる現象があり、脱調による座標ずれかlimit入力ノイズか未確定 | 低速・低加速度で再現性を確認し、limit配線、pull-up、機械干渉を切り分ける |
| R19 | [ ] | `HARD_LIMIT_UNEXPECTED_ALARM_MS`は20msへ短縮済みだが、安全停止距離とlimit入力ノイズ耐性のバランスが未確定 | 実機でlimitを意図的に押して停止距離を確認し、誤検出が出る場合は配線、pull-up、debounce、値を再調整する |
| R20 | [ ] | 動き出し・動き終わりの歪み原因が、加減速設定、ペン圧、ベルト張り、機械ガタ、ステップ抜けのどれか未確定 | 反時計回り/時計回りの同心正方形CSVで方向、サイズ、始点/終点依存性を切り分ける |
| R21 | [ ] | AB_TIMEDでも歪む場合、`StepperBackendFastAccel`の`moveTimed()`投入、A/B同期開始、duration指定、queue容量見積もりのどれが支配的か未確定 | `diagnostic_ab_timed_square_draw.csv`で小/大/連続/方向違いの結果を比較し、backendログと照合する |
| R22 | [ ] | Phase 10のjunction deviation値、classic jerk上限、batch収集時間が実機で未調整 | `lookahead_check.csv`を`--queue-mode`で実行し、`LOOKAHEAD blocks>1`、角の丸まり、閉じズレ、脱調、温度を確認して調整する |
| R23 | [ ] | Phase 7最小G-codeはbuild確認のみで、実機での`G28`、相対移動、inch換算、pen動作確認が未完了 | `gcode_check.csv`を実行し、`ACK_XY`、`POS`、pen、limit/homing状態を確認する |
| R24 | [ ] | 2026-06-07時点でCore2 USB serial portが見えず、`pio run --target upload`がBluetooth port自動検出で失敗 | Core2をUSB接続し、`/dev/cu.usbserial-*`等のportを指定してuploadとSerial Monitor確認を実行する |
| R25 | [ ] | KST32B Text Toolの生成G-codeは実KST32Bデータで生成確認済みだが、実機での文字潰れ、dwell、feed、soft limit余裕が未確認 | 小さい文字や濁点を含むサンプルを低速から送信し、`--size`、`--dwell-ms`、feedを調整する |
| R26 | [ ] | Text Tool生成G-code実機送信で、homing後もlimitがACTIVEのまま残り、X方向戻りストロークで`NACK_XY`後にhard-limit alarmへ入った | homing後のlimit解放距離、switch機械位置、配線ノイズ、`HARD_LIMIT_UNEXPECTED_ALARM_MS`、描画開始位置を確認する |
| R27 | [ ] | `--stream-gcode-motion`はG-code由来の`G0/G1`を先行投入するため、実機でのlook-ahead改善、CommandQueue full再送、後続pen/dwell応答とのログ混在耐性は未確認 | `--gcode --queue-mode --stream-gcode-motion`で短い線分の日本語サンプルを低速から送信し、停止感、alarm、queue retry、pen timingを確認する |
| R28 | [ ] | hard limit停止中にstream済みの後続G-codeがCommandQueueへ残る場合、alarmで移動は拒否されるがログ上は後続行の`NACK_XY`が続く可能性がある | hard limit発生時のserial logを確認し、必要ならalarm発生時にCommandQueueをflushする設計を追加する |
| R29 | [ ] | `NORMAL_MOVE_LIMIT_RELEASE_MM=8mm`はhoming直後のlimit release猶予として暫定値であり、実際のswitch戻り距離と高速G0時の停止余裕が未確認 | G28直後の最初のG0で`LIMIT_RELEASE_ALLOW`が出て、limitがOFFへ戻ることを低速から確認する。戻らない場合はswitch機構、配線、release距離を調整する |
| R30 | [ ] | `--stream-gcode-motion`送信高速化後も、文字データに`M3/M5/G4`が多い場合はストローク間で停止する。これはmotionではなくpen/dwell由来の停止である | Text Toolの`--dwell-ms`を20ms、0msなどで比較し、ペン実機で線抜けが出ない最小値を決める |
| R31 | [ ] | `--stream-xy-motion`はCSV由来の`XY`を先行投入するため、実機でのlook-ahead改善、CommandQueue full再送、非XY行との応答混在耐性は未確認 | `--csv ... --queue-mode --stream-xy-motion`で連続XY CSVを低速から送信し、`LOOKAHEAD blocks>1`、停止感、alarm、queue retryを確認する |
| R32 | [-] | Job Lifecycleは実装済みだが、motionを伴う`JOB_BEGIN -> G-code -> JOB_END`実機確認が未完了。`JOB_BEGIN OK homed=YES`後の最初のG-code移動で`machine is not homed`拒否が出たため、JOB_BEGIN時のhomed検証結果をジョブ中の移動ゲートにも使うよう修正した | `job_lifecycle_check.csv`を低速・E-stop可能な状態で実行し、job状態、pen up、G-code由来XY許可、手入力XY拒否、JOB_END park/jingleを確認する |
| R33 | [ ] | `XY`を診断/bring-up用へ位置付けたが、既存CSVとQR Toolには`XY`出力が残る | 正式運用用ツールはG-code出力へ寄せ、`XY` CSVは診断手順としてREADME/手順書で明記する |
| R34 | [-] | `JOB_BEGIN_AUTO_HOME`で未homed時の自動HOMEを切り替えられるが、true時の実機安全確認は未完了 | limit switch方向、E-stop可能状態、HOME失敗時の`auto_home_failed`、成功時の`JOB_BEGIN OK`を低速で確認してから正式運用でtrueにする |
| R35 | [ ] | `JOB_END`退避移動とA/B両モータ終了ジングルはbuild/upload済みだが、実機での脱調、音量、TMC温度、退避位置の機械干渉が未確認 | `job_lifecycle_check.csv`を低速・E-stop可能な状態で実行し、退避位置、ジングル音量、モータ/TMC温度を確認する |
| R36 | [ ] | `JOB_BEGIN`のTMC自動初期化と`JOB_BEGIN_AUTO_HOME`はbuild/upload後のSerial再確認が未完了 | `JOB_BEGIN_AUTO_HOME=false`で未homedなら`not_homed`拒否、trueで未homedなら`JOB_BEGIN AUTO_HOME start`からHOME実行へ進むことを安全状態で確認する |
| R37 | [ ] | stream G-code drift対策はbuild、upload、SELFTEST、native closed-loopテストまで完了したが、実際の長時間stream描画での閉じ位置と`WARN: DRIFT`未発生は未確認 | `--gcode --queue-mode --stream-gcode-motion --job-lifecycle`で長い微小線分ジョブを低速から実行し、DRIFTログ、閉じ位置、脱調、pen timingを確認する |
| R38 | [ ] | timed segment部分投入失敗時は位置信頼性喪失としてalarm停止するが、意図的にFastAccelStepper queueを詰めた再現試験は未実施 | queue余裕が少ない高密度segment条件またはテスト用fault injectionを用意し、部分投入失敗時のstop、再同期、homed無効化ログを確認する |
| R39 | [ ] | WebUI Image to G-codeのPNG/JPEG traceはホスト側単体テストのみで、実機での描画品質、線分密度、ペン/紙条件に対する最適設定が未確認 | Line Art/Outline Trace、threshold、skeletonize、max segmentsを小さい画像から確認し、必要ならOpenCV/scikit-imageベースのtraceへ差し替える |

---

# 15. 変更履歴

| 日付 | 変更 | 更新者 |
|---|---|---|
| 2026-05-29 | M5Stack Core2前提、ピン割り付け、Core割り付け、TMC2209 UART共通バスを反映 | ChatGPT |
| 2026-05-29 | 進捗管理用チェックリスト形式に変更。Phase 0〜11までのチェック項目を追加 | ChatGPT |
| 2026-05-31 | Phase 0〜6を実装。simulation/real mode両方のbuildを確認。Phase 4実機確認項目を保留 | Codex |
| 2026-05-31 | Phase 6.6 Core2 LCD Status UIの実装計画を追加 | Codex |
| 2026-05-31 | Phase 6.7 NEOPIXEL Status LEDとPhase 6.8 Motor Melody Diagnosticsの仕様・実装計画を追加 | Codex |
| 2026-05-31 | NEOPIXEL灯数をconfigで変更可能にし、各種設定値をconfigファイルへ集約する計画を追加 | Codex |
| 2026-05-31 | FastLEDのPacifica、Fire等を選択できるNEOPIXELパターンエンジンと外部parameter設定の計画を追加 | Codex |
| 2026-05-31 | Phase 6.6〜6.8を実装。FastLED LED制御、LCD差分更新、診断メロディ経路を追加しsimulation/real mode buildを確認 | Codex |
| 2026-05-31 | `../1stepper_test`を参考にTMCStepperを導入。A/Bアドレス別レジスタ設定、profile切替、UART診断読出しを実装 | Codex |
| 2026-06-03 | Phase 4 / 6.6〜6.8の実機bring-up確認完了を反映。M0完了条件を達成済みに更新 | Codex |
| 2026-06-03 | Phase 6.9 Homing bring-upの実装計画、手動テスト、Codex用プロンプト、リスク項目を追加 | Codex |
| 2026-06-03 | `../1stepper_test`の二段階homing、backoff、低速再検出、debounce診断をPhase 6.9要件へ反映 | Codex |
| 2026-06-04 | Phase 6.9を実装。HOME/HOME_X/HOME_Y、二段階homing、limit raw/debounced診断、homed前移動制限、hard limit alarmを追加し`pio run`成功 | Codex |
| 2026-06-04 | `pio run --target upload`を通常権限と権限付きで実行したが、`/dev/cu.usbserial-023591AC`のtermios設定エラーで失敗 | Codex |
| 2026-06-06 | Phase 8台形加減速とPhase 9 timed segmentを実装し、TrapezoidPlanner、SegmentGenerator、SegmentQueue、FastAccelStepper `moveTimed()`経路を追加 | Codex |
| 2026-06-06 | `PlotterConfig.h`へ日本語コメントを追加し、最大feed 5000mm/min、servo up/down角度config化、high-speed checkを追加 | Codex |
| 2026-06-07 | timed segment実機描画で脱調対策を追加。加速度37.5mm/s^2、TMC通常電流850mA、hard limit継続時間判定、center shapes低速CSVを計画へ反映 | Codex |
| 2026-06-07 | Serial Toolの起動ログ読み捨て時間を`--startup-drain`として`--timeout`から分離し、HOME完了待ちをexpect主体で短縮できる仕様を追加 | Codex |
| 2026-06-07 | HOMEを扱うCSVで`ALARM_CLEAR`前に`ZERO`を入れ、脱調後に古い論理座標を破棄してから再homingする復旧順序へ統一 | Codex |
| 2026-06-07 | HOME開始時のlimit raw ONを即Backoff条件に追加し、debounce未反映中にseek方向へ押し込む短時間移動を防止 | Codex |
| 2026-06-07 | Serial Toolの`Ctrl-C`中断時に`ABORT`を送信し、ファームウェア側でmotion/homingを停止してalarmへ遷移する追加仕様を反映 | Codex |
| 2026-06-07 | Homingを短い固定距離move反復から長距離moveのlimit停止方式へ変更し、fast seek速度設定が実効速度へ反映されやすい構造に更新 | Codex |
| 2026-06-07 | 動き出し・動き終わりの歪み調査用に、同じ中心へ5個の正方形を重ねて描くSerial Tool CSVと手順書を追加 | Codex |
| 2026-06-07 | 同心正方形の時計回り版CSVを追加し、反時計回り版との方向依存比較を手順書へ反映 | Codex |
| 2026-06-07 | 診断専用`AB_TIMED`コマンドと、PENDOWNして四角を描く`diagnostic_ab_timed_square_draw.csv`を追加 | Codex |
| 2026-06-07 | Phase 10 look-ahead / junction deviationを実装。JunctionPlanner、連続XYバッチ、CONFIG表示、lookahead check CSV/手順書を追加 | Codex |
| 2026-06-07 | Phase 10実装後に`pio run`、`pio run --target upload`、Serial Toolの`CONFIG`/`POS`/`SELFTEST`/`TMC_INIT`/`TMC_STATUS`確認が成功 | Codex |
| 2026-06-07 | Serial ToolへCSV各行の`TIMING START`/`TIMING END`ログを追加し、最初のコマンド開始を0とした相対時刻と行内経過時間を表示 | Codex |
| 2026-06-07 | ホスト側`tools/qr_tool`を追加し、QR文字列/URLから`PENUP`/`PENDOWN`/`XY` CSVとハッチングSVGを生成できるようにした | Codex |
| 2026-06-07 | Serial Toolの`--queue-mode`で`HOME`/`HOME_X`/`HOME_Y`の完了ログ待ちを既定30秒にし、QR CSVのhoming timeoutを防止 | Codex |
| 2026-06-07 | QR Toolの描画方式を、横run矩形の外周描画と45度斜線ハッチングに変更 | Codex |
| 2026-06-07 | QR Toolの内部ハッチングを、線ごとのペン上下から連続ジグザグ塗りつぶしに変更 | Codex |
| 2026-06-07 | QR Toolの塗りつぶしを横run単位から上下左右接続成分単位の横方向ジグザグ連続パスへ変更 | Codex |
| 2026-06-07 | Phase 7最小G-codeを実装。`GcodeParser`、`ParsedGcode`、`GcodeInterpreter`、`G0/G1/G20/G21/G28/G90/G91/M3/M5/M114`、gcode check CSV/手順書を追加 | Codex |
| 2026-06-07 | Phase 7実装後に`pio run`成功、`gcode_check.csv --dry-run`成功、`test_serial_send.py`成功。Core2 USB port未検出のためuploadと実機Serial確認は未実行 | Codex |
| 2026-06-07 | KST32B Text Toolを追加。CSF/1デコード、CLI、サンプル入力/生成G-code、README、`G4 P<ms>` dwell対応を追加 | Codex |
| 2026-06-08 | Serial Toolへ`--gcode`入力を追加し、Text Tool生成G-codeを直接送信できるようにした | Codex |
| 2026-06-08 | Serial Toolへ`--preamble-csv`と`gcode_preamble.csv`を追加し、Text Tool生成G-code送信前にalarm clear、limit確認、homing確認を前置できるようにした | Codex |
| 2026-06-08 | Text Tool生成G-code実機ログの`NACK_XY`継続送信問題を受け、Serial ToolでG-code行の既定expectとfirmware failure検出を追加 | Codex |
| 2026-06-08 | Text Tool生成G-code実機ログの`junction planner rejected XY batch`を受け、同一座標へのペンアップ`G0`を省略するよう修正し、サンプルG-codeを再生成 | Codex |
| 2026-06-08 | Text Tool生成G-code実機ログのsoft limit超過を受け、`--max-x`/`--max-y`範囲検査と`--auto-scale-to-fit`を追加し、サンプルG-codeを55x55mm範囲内へ再生成 | Codex |
| 2026-06-08 | Serial Toolへ`--stream-gcode-motion`を追加し、G-code由来の`G0/G1`を完了ACK待ちではなくqueue投入ACKで先行送信できるようにした | Codex |
| 2026-06-08 | 通常XY/timed segment実行中にbackend現在stepからMachineStateのX/Y概算位置を更新し、hard limit継続判定をブロック完了前に効かせるよう修正。`HARD_LIMIT_UNEXPECTED_ALARM_MS`を20msへ短縮 | Codex |
| 2026-06-08 | G28直後の原点limit ON状態から通常移動で離れる場合に、`NORMAL_MOVE_LIMIT_RELEASE_MM`の範囲だけlimit releaseを許容し、戻らない場合はalarm停止するよう修正 | Codex |
| 2026-06-08 | Serial Toolの`--stream-gcode-motion`で、G-code由来`G0/G1`のACK後serial idle待ちと成功時の行別ログを省き、CommandQueueへ連続XYを高速投入しやすくした | Codex |
| 2026-06-08 | Serial Toolへ`--stream-xy-motion`を追加し、CSV由来`XY`もACK後serial idle待ちと成功時の行別ログを省いて先行投入できるようにした | Codex |
| 2026-06-08 | 正式描画入力はG-code基本、`XY`は診断/bring-up用とする運用方針をSPEC/PLANSへ反映。起動/終了処理は将来Job Lifecycleとしてファームウェア側へ移す方針を追加 | Codex |
| 2026-06-08 | Phase 10.5 Job Lifecycle計画を追加。`JOB_BEGIN`/`JOB_END`/`JOB_ABORT`/`JOB_STATUS`、開始前確認、終了時pen up、Serial Tool検査、未解決判断を整理 | Codex |
| 2026-06-08 | `ABORT`は低レベル即時停止、`JOB_ABORT`はJob Lifecycle上の中断ラッパーとして使い分ける方針をPhase 10.5へ追記 | Codex |
| 2026-06-08 | Phase 10.5を実装。`JobController`、`JOB_BEGIN`/`JOB_END`/`JOB_ABORT`/`JOB_STATUS`、G-code source判定、`serial_send.py --job-lifecycle`、job lifecycle check CSV/手順書を追加。`pio run`とupload、`CONFIG`/`POS`/`SELFTEST`/`TMC_INIT`/`TMC_STATUS`確認が成功 | Codex |
| 2026-06-08 | `JOB_END`へpen up、`X=5mm, Y=Y_MAX_MM-5mm`退避、A/B両モータのオリジナル8-bit風和音終了ジングルを追加。`pio run`、upload、基本Serial確認が成功 | Codex |
| 2026-06-08 | `JOB_BEGIN`へTMC未ready時の自動`TMC_INIT`を追加。初期化失敗時のみ`tmc_not_ready`で拒否する方針へ更新。`pio run`とuploadは成功、Serial再確認は権限付き実行の利用制限で未実行 | Codex |
| 2026-06-09 | `JOB_BEGIN OK homed=YES`後のG-code移動が`machine is not homed`で拒否される実機ログを受け、JOB_BEGIN時のhomed検証結果をJobControllerに保持し、ジョブ中のG-code由来XY移動のhomed判定へ使うよう修正 | Codex |
| 2026-06-09 | `JOB_BEGIN_AUTO_HOME` configを追加。既定はfalse。true時は`JOB_BEGIN`で未homedならTMC初期化後にHOME相当を自動実行し、失敗時は`auto_home_failed`で拒否する。`pio run`、upload、`CONFIG`/`SELFTEST`/`TMC_STATUS`確認は成功 | Codex |
| 2026-06-09 | 前回`JOB_BEGIN`拒否後の次回`JOB_BEGIN`が`job_not_idle`で拒否される実機ログを受け、開始前拒否は`IDLE`へ戻し、`FAILED`/`ABORTED`もalarm解除済みなら次回`JOB_BEGIN`前に`IDLE`復帰できるよう修正。`pio run`とuploadは成功。`JOB_BEGIN_AUTO_HOME=true`状態で未homed `JOB_BEGIN`がAUTO_HOMEへ入ることを確認し、安全のため`ABORT`した | Codex |
| 2026-06-09 | ホスト側失敗後にjob状態が`RUNNING`へ残った実機ログを受け、Serial Toolは`--job-lifecycle`の`JOB_BEGIN`/G-code本文失敗時にも`JOB_ABORT`を送るよう修正。ファーム側は`JOB_ABORT`受信時点でmotion abort flagを立て、homing中にも停止要求が届くよう修正。`pio run`、upload、`CONFIG`/`SELFTEST`/`TMC_STATUS`確認は成功 | Codex |
| 2026-06-09 | `JOB_BEGIN_AUTO_HOME=true`でHOME中にSerial Toolが既定timeout 2秒で`JOB_BEGIN OK`未検出と判断し`JOB_ABORT`する実機ログを受け、Serial Toolの`JOB_BEGIN`完了待ちを60秒、`JOB_END`完了待ちを30秒へ延長。`py_compile`確認は成功 | Codex |
| 2026-06-09 | Core2 LCD UIを3ページ構成へ拡張。下部タブ、左右フリック、物理A/Cボタンでページ切替し、Status/Control/Detail表示、UIからの`HOME`、`ALARM_CLEAR`、home完了後のjogとpen上下を追加。ちらつき対策として`M5Canvas` + `pushSprite()`描画へ変更し、`pio run`とuploadは成功 | Codex |
| 2026-06-09 | Host WebUIの設計を開始。PC/Raspberry Pi側WebUIからUSB Serialで制御し、ジョブ送信は既存`tools/serial_tool/serial_send.py`をsubprocess再利用、G-code previewをMVPに含める方針をSPEC/PLANSへ追加。作業ブランチ`codex/webui-serial-preview`を作成 | Codex |
| 2026-06-09 | Host WebUI MVPを追加。`tools/webui/server.py`、静的HTML/CSS/JS、READMEを実装し、Dashboard/Control/Job/Console/Settings、SSE log、G-code preview、`serial_send.py` subprocess job送信を追加。`py_compile`と`node --check`は成功。sandbox制限によりローカルHTTP応答確認は未完了 | Codex |
| 2026-06-10 | UI jogで左右のステップ量が違うように見える実機症状を調査。CoreXY式、A/B pin、motor invert、StepperBackendはmainと一致し、原因はTMC未初期化状態でmotionしていたこと。`XY`、G-code由来`G0/G1`、`HOME`、`AB_TIMED`の前にTMC未readyなら自動`TMC_INIT`するよう修正し、実機で動作改善を確認 | Codex |
| 2026-06-11 | `--stream-gcode-motion`中に`M3`の`PEN DOWN`待ちが既定timeoutで失敗した実機ログを受け、Serial Toolの期待ログ未検出エラーをtimeout到達時は`timeout after ... waiting for ...`と表示するよう修正。ファームウェア拒否とホスト側待ち時間切れを区別する方針をSPEC/READMEへ反映 | Codex |
| 2026-06-11 | look-ahead中にG-code由来`M5`をpendingへ退避した後、XY正常完了時のqueue clearでpendingも消えて`PEN UP`が実行されない実機ログを受け、XY正常完了時はpending commandを保持するよう修正 | Codex |
| 2026-06-12 | maze G-code先頭の`G0 X0 Y0 F8000`がゼロ距離MotionBlockとしてJunctionPlannerに拒否される実機ログを受け、ゼロ距離`XY`/`G0`/`G1`はplannerへ投入せずno-op ACKとして扱う仕様を追加 | Codex |
| 2026-06-12 | Serial ToolがCSV `XY`、G-code `G0/G1`、`G4`から実行時間を概算し、`推定motion時間 + --motion-timeout-margin`でtimeoutを自動延長するよう修正。stream motionの累積推定時間を次の非stream行へ引き継ぐ仕様を追加 | Codex |
| 2026-06-13 | stream G-code motionの座標ドリフト対策を追加。timed segment部分投入時のB側リトライ/失敗時再同期alarm、XY blockの絶対A/B step target化、CommandQueue満杯時のmotion行backpressure、native motion drift test、SIMULATION_MODE build環境を実装。`pio run`、`pio run -e m5stack-core2-sim`、upload、`CONFIG`/`POS`/`SELFTEST`/`TMC_INIT`/`TMC_STATUS`確認が成功 | Codex |
| 2026-06-13 | KST32B Text ToolのCSF/1 X move解釈を修正。X moveを現在Y上の即時ペンアップ移動として扱い、`高`の上点、ASCII `H`/`L`/`l`が斜め線になる問題を修正。回帰テストを追加 | Codex |
| 2026-06-13 | Host WebUIへSVG to G-codeを追加。SVGファイル/文字列入力、`POST /api/gcode/from-svg`、stroke抽出、fit/Y反転/短stroke削除/順序最適化、既存preview/save/send導線への仮想G-code追加、単体テストを実装。実機描画確認は未実施 | Codex |
| 2026-06-13 | `tools/webui/examples/svg_check.svg`を追加し、SVG変換G-codeを実機へ送信。`JOB_BEGIN` auto-home、6 strokes / 49 segmentsの描画、`JOB_END` park/jingleまで成功し、最終状態`HOMED=YES PEN=UP ALARM=NO TMC=READY`を確認 | Codex |
| 2026-06-13 | Host WebUIのユーザー向けSVG to G-codeをImage to G-codeへ統合。`.svg/.png/.jpg/.jpeg` upload、`POST /api/gcode/from-image`、PNG/JPEG→plotter SVG→共通SVG G-code経路、Line Art/Outline Trace設定、中間SVG response/download、Pillow requirements、単体テストを追加。実機PNG/JPEG描画品質確認は未実施 | Codex |
| 2026-06-13 | Image to G-code変換の進捗表示を追加。upload、PNG/JPEG trace、SVG to G-code、layout追加のステップ表示、失敗時のパネル内エラー表示、SSE progress log、ブラウザ接続reset時のサーバ側traceback抑制を実装 | Codex |
| 2026-06-13 | Image to G-codeの既定`max_segments`を4000から12000へ変更し、5363 segments程度のラスタ変換が初期設定で失敗しないようにした。実行中ボタンのステータス文言とアニメーション、失敗時の`FAIL`表示を追加 | Codex |
| 2026-06-13 | PNG/JPEGの既定trace modeをOutline Traceへ変更し、輪郭抽出を境界ピクセル近傍接続からmarching squaresベースへ改善。塗りつぶしイラストが内部skeleton線へ崩れる問題を軽減し、Line Artのskeletonizeは線画専用設定へ整理 | Codex |
| 2026-06-13 | Image to G-codeの中間SVGで元画像bboxの縦横比を保持するよう修正。Trace Detail設定（High/Balanced/Simple）と濃色領域ハッチング設定（threshold/pitch）を追加し、アスペクト比保持とhatchingの単体テストを追加 | Codex |
| 2026-06-13 | Core2 LCDとHost WebUIの状態表示を`ALARM > HOMING > MOVING > RUNNING > NEED HOME > READY`の優先順位へ統一。`MachineState`へ`motion_active`/`job_active`を追加し、timed segment中はREADYではなくMOVING、Job Lifecycle中はRUNNING表示にした | Codex |
| 2026-06-13 | Phase 11親チェックリストを棚卸し。G28統合、軸別homing、homed前移動制限、TMC基本status/UART失敗検出を完了へ更新し、WebUI、USB G-code streaming、Job Lifecycle接続は実機確認残りのため一部完了へ整理 | Codex |
| 2026-06-13 | NeoPixel状態連動表示を追加。`BREATH`/`CHASE`/`PROGRESS`/`ALERT`/`SUCCESS`、`LedStatus`、`LED_AUTO`、`LED_STATUS_SET`、主要motion/job/alarm状態からの自動表示更新、LED check CSV/手順更新を実装 | Codex |
| 2026-06-13 | NeoPixel自動演出を組み込み強化。状態優先順位、`COMPLETED`/`WARNING`の短時間保持と`IDLE`自動復帰、診断用`LED_STATUS_SET`の強制適用を追加 | Codex |
| 2026-06-14 | WebUI Funタブを調整。Mazeを3mm/1mm path幅とS/G線画付きに変更し、Lissajous/Gridへ生成前previewを追加、Fun生成後は既存Job layoutへ遷移するようにした。Webcam Portraitは輪郭線に暗部hatchingを重ねるボールペン線画寄り処理へ変更。READY/IDLE LED自動表示はBREATHからPACIFICAへ変更 | Codex |
| 2026-06-14 | Fun MazeのSVG出力をセル壁ごとの`line`から、接続壁componentをDFS walkした長い`polyline`へ変更。1mm/2mm刻みの壁ごとにpen up/downする問題を軽減し、S/G文字は小さめの独立strokeとして維持 | Codex |
| 2026-06-14 | Fun MazeのS/G文字も個別`line`から連続`polyline`へ変更し、Hard maze変換結果を4 strokes（壁2、S、G）へ削減。SVG to G-codeのstroke間に出ていた重複`M5`も除去 | Codex |

---

# 16. 現在マイルストーン完了条件

現在マイルストーンは以下。

```text
M0: M5Stack Core2用の安全なファームウェア土台を作る
```

完了条件:

- [x] Phase 0 complete
- [x] Phase 0.5 complete
- [x] Phase 1 complete
- [x] Phase 2 structure complete
- [x] Phase 3 simulation or low-speed complete
- [x] Phase 4 low-speed complete
- [x] Phase 5 complete
- [x] Phase 6 placeholder complete
- [x] Phase 6.6 Core2 LCD Status UI complete
- [x] Phase 6.7 NEOPIXEL Status LED complete
- [x] Phase 6.8 Motor Melody Diagnostics complete

次マイルストーンは以下。

```text
M1: 実機homing、timed segment、最小G-codeの土台を作る
```

完了条件:

- [x] Phase 6.9 Homing bring-up complete
- [x] Phase 7 minimal G-code structure complete
- [x] Phase 8 trapezoid planner complete
- [x] Phase 9 timed segment complete
- [-] Phase 10 look-ahead implemented; machine verification pending

未実装のままでよいもの:

- [-] WebUI
- [ ] SD execution
- [ ] input shaping

注意:

Phase 11以降は、実装範囲を確認してから着手する。  
Codexは勝手に実装しないこと。
