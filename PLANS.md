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
現在地: Phase 0〜6.9 完了。Phase 8 台形加減速、Phase 9 timed segment実装済み。実機bring-upで脱調対策を調整中
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
| 脱調対策 | 描画用の保守的な速度/加速度/電流/ペン圧設定とcenter shapes実機確認を追加 |
| G-code parser | 未実装予定 |
| look-ahead | 未実装予定 |
| junction deviation | 未実装予定 |

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
| 7 | 最小G-code | G0/G1/G90/G91/G20/G21/M3/M5/M114 | [ ] |
| 8 | 台形加減速 | TrapezoidPlannerを実装 | [x] |
| 9 | timed segment | SegmentGeneratorでA/B同期 | [x] |
| 10 | look-ahead | JunctionPlanner、junction deviation | [ ] |
| 11 | 高級機能 | homing、TMC診断、SD/WebUI、補正系 | [ ] |

現在の実装対象:

```text
Phase 9 timed segment実装後の実機描画安定化
```

Phase 0〜6.9、Phase 8、Phase 9は完了済み。
Phase 7、Phase 10以降は、実装範囲を確認してから着手する。
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

- [x] `XY <x> <y> <feed>`を受けられる
- [x] current XYをログに出す
- [x] target XYをログに出す
- [x] dx/dyをログに出す
- [x] A/B stepをログに出す
- [x] feedをログに出す
- [x] `SIMULATION_MODE=1`ではモータを動かさない
- [x] 成功時のみMachineStateを更新する

## Phase 1 完了条件

- [x] `SELFTEST PASS`
- [x] `ZERO`後 `XY 10 0 600` で A=800 B=800
- [x] `ZERO`後 `XY 0 10 600` で A=800 B=-800
- [x] `ZERO`後 `XY 10 10 600` で A=1600 B=0
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

## Phase 2 完了条件

- [x] `TMC_INIT`が存在する
- [x] `TMC_STATUS`が存在する
- [x] TMC UART処理が`TMC2209Manager`に閉じている
- [x] plannerやStepperBackendにTMC設定が混ざっていない

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
- [x] Touch/Button操作画面は将来拡張として分離する

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
- [x] `LED_STATUS`
- [x] RGB値の0〜255範囲を検証する
- [x] indexが`0 <= index < NEOPIXEL_LED_COUNT`であることを検証する
- [x] parameter値を範囲検証する
- [x] command処理と描画処理を分離し、外部引数は`LedAnimationConfig`へ反映する
- [x] 描画中の`LedAnimationConfig`更新で不整合が起きないようにする
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

Status: 将来。まだ実装しない。

## チェックリスト

- [ ] `GcodeParser`を追加
- [ ] `ParsedGcode`を追加
- [ ] `GcodeInterpreter`を追加
- [ ] `G0 X Y F`
- [ ] `G1 X Y F`
- [ ] `G20`
- [ ] `G21`
- [ ] `G28`
- [ ] `G90`
- [ ] `G91`
- [ ] `M3`
- [ ] `M5`
- [ ] `M114`
- [ ] parserはmotionを直接実行しない
- [ ] interpreterがMachineStateを扱う
- [ ] motionはSafetyManagerを通る
- [ ] F値はmm/minとして扱う

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
- [x] `TMC_NORMAL_RMS_CURRENT_MA`を実機で脱調しない方向へ調整する
- [x] `PEN_DOWN_ANGLE_DEG`をconfig化した上で、紙への押し付けを弱める方向へ調整する
- [ ] TMC電流増加後のモータ/ドライバ温度を連続描画で確認し、必要なら電流を下げる

### 9.1.2 hard limit入力の通常移動中判定

- [x] homing中ではない通常移動で、原点外のlimit activeが一定時間継続した場合だけalarmへ入れる
- [x] 継続時間を`HARD_LIMIT_UNEXPECTED_ALARM_MS`として`PlotterConfig.h`から設定できる
- [x] 瞬間的なlimitノイズでは即alarmにしない
- [x] limitが継続ONの場合は従来通り安全停止できる構造にする
- [ ] 実配線でX/Y limit入力のノイズ量を確認し、必要なら外付けpull-up、配線取り回し、RC filterを追加する

### 9.1.3 center shapes実機確認

- [x] `tools/serial_tool/examples/center_shapes.csv`を追加する
- [x] 紙面中心を`(X_MIN+X_MAX)/2`, `(Y_MIN+Y_MAX)/2`相当の`(27.5, 27.5)`として扱う
- [x] マル、四角、三角、星を中心付近へ描画する
- [x] 図形サイズはsoft limit端から十分余白を残す
- [x] 描画feedは保守的な`300 mm/min`、移動feedは`600 mm/min`から開始する
- [x] `center_shapes.csv`が実機で最後まで完走し、最終`POS`で`ALARM=NO`、`LIMIT_X=OPEN`、`LIMIT_Y=OPEN`を確認する
- [ ] 脱調が再発しないか、同じCSVを複数回連続で実行して確認する

---

# Phase 10: look-ahead / junction deviation

Status: 将来。まだ実装しない。

## チェックリスト

- [ ] `JunctionPlanner`を実装
- [ ] junction speed計算
- [ ] reverse pass
- [ ] forward pass
- [ ] junction deviation
- [ ] classic jerk相当の制限を検討
- [ ] CoreXY motor-space速度制限
- [ ] PlannerQueue内の複数MotionBlockを対象にする
- [ ] StepperBackendに実装しない

---

# Phase 11: 高級機能

Status: 将来。まだ実装しない。

## チェックリスト

### Homing / Limit

- [ ] Phase 6.9完了後のhoming精度改善
- [ ] G28との統合
- [ ] homing後の軸別再homing
- [ ] limit switch debounceの実測調整
- [ ] hard limit停止経路のtimed segment対応
- [ ] homed前移動制限の運用方針確定
- [ ] alarm復帰

### TMC診断

- [ ] driver status取得
- [ ] SG_RESULT取得
- [ ] over temperature警告
- [ ] open load診断
- [ ] UART通信失敗検出

### 入出力

- [ ] SD実行
- [ ] WebUI
- [ ] USB G-code streaming
- [ ] file pause/resume

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
XY 10 0 600
ZERO
XY 0 10 600
ZERO
XY 10 10 600
XY -1 0 600
XY 301 0 600
XY 0 301 600
XY 10 10 0
TMC_STATUS
```

チェック:

- [ ] `SELFTEST PASS`
- [ ] `XY 10 0 600`でA=800 B=800
- [ ] `XY 0 10 600`でA=800 B=-800
- [ ] `XY 10 10 600`でA=1600 B=0
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

- [x] `CONFIG`で`accel=37.500`を確認できる
- [x] `TMC_STATUS`で通常profileの電流設定を確認できる
- [x] `HOME`が完了する
- [x] マル、四角、三角、星の描画コマンドが最後までACKされる
- [x] 最終`POS`で`ALARM=NO`を確認できる
- [x] 最終`POS`で`LIMIT_X=OPEN`、`LIMIT_Y=OPEN`を確認できる
- [ ] 実際の線が目視で大きくずれない
- [ ] 連続実行後もモータ/TMC温度が許容範囲に収まる

## 12.8 Serial Tool待ち時間仕様

チェック:

- [x] `--timeout`は各コマンド応答の最大待ち時間として扱う
- [x] 起動ログ読み捨て時間は`--startup-drain`で指定できる
- [x] `--startup-delay 0 --timeout 60`でも、最初のコマンド送信前に60秒待たない
- [x] `HOME`行はCSV `delay_ms`を短くし、`expect=HOME complete`と長めの`--timeout`で完了待ちできる
- [ ] high-speed / homing系CSVのHOME行を、固定長待ちからexpect主体の短い`delay_ms`へ整理する

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
| R16 | [!] | uploadが`/dev/cu.usbserial-023591AC`で`termios.error: (22, 'Invalid argument')`により失敗 | USB/シリアルドライバ、port占有、接続状態、upload_speed設定を確認して再試行 |
| R17 | [ ] | 通常TMC電流を850mAへ上げたため、モータ/TMC2209/電源の発熱余裕が未確定 | center shapes連続実行後に温度を確認し、熱い場合は800mA以下へ下げる |
| R18 | [ ] | 原点外でX limitが継続ACTIVEになる現象があり、脱調による座標ずれかlimit入力ノイズか未確定 | 低速・低加速度で再現性を確認し、limit配線、pull-up、機械干渉を切り分ける |
| R19 | [ ] | `HARD_LIMIT_UNEXPECTED_ALARM_MS=500`は実機暫定値であり、安全停止遅延とノイズ耐性のバランスが未確定 | 実機でlimitを意図的に押して停止距離を確認し、必要なら値を短くする |

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
M1: 実機homingとhard limit alarmの土台を作る
```

完了条件:

- [ ] Phase 6.9 Homing bring-up complete

未実装のままでよいもの:

- [ ] G-code parser
- [ ] look-ahead
- [ ] junction deviation
- [ ] timed segment
- [ ] WebUI
- [ ] SD execution
- [ ] input shaping

注意:

上記の「未実装のままでよいもの」は、実装しないことが正しい状態である。  
Codexは勝手に実装しないこと。
