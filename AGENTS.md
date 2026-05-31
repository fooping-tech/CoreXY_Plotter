# AGENTS.md

適用範囲: このファイルはリポジトリ全体に適用する。

このリポジトリは、M5Stack Core2を用いたCoreXYペンプロッタ用ファームウェアである。  
目的は、最初から巨大なCNCファームウェアを作ることではなく、後からplanner、look-ahead、junction deviation、TMC2209 UART診断、G-code対応を追加できる構造を作ることである。

---

## 1. プロジェクトの目的

このファームウェアは、最終的に以下を実現する。

- M5Stack Core2でペンプロッタを制御する
- CoreXY機構をA/Bモータで駆動する
- TMC2209をSTEP/DIRで駆動する
- TMC2209のUART設定を共通バスで行う
- 2個のTMC2209はUARTアドレス違いで制御する
- LCD/Touch/Buttonで状態表示や操作を行う
- Serialから独自コマンドを受ける
- 将来的にG-codeを受ける
- MachineStateを明確に保持する
- CoreXY変換を独立モジュールで行う
- SafetyManagerでsoft limit、limit switch、alarmを扱う
- MotionBlock、PlannerQueue、TrapezoidPlanner、JunctionPlannerを後から追加できるようにする
- FastAccelStepperはSTEP/DIR出力バックエンドとして使う

---

## 2. 最重要方針

このプロジェクトでは、**動くコードよりも、後で壊れない構造を優先する**。

```text
今は小さく作る。
でも後で伸ばせる構造にする。
```

---

## 3. 対象ボード

初期対象ボードは **M5Stack Core2** とする。

Core2はESP32を搭載しており、2Core構成である。  
本プロジェクトでは以下のように役割分担する。

| Core | 役割 |
|---:|---|
| Core 0 | UI、LCD、Touch、Button、Serial入力、ログ出力 |
| Core 1 | Motion、Safety、Stepper、TMC2209 UART、Pen制御 |

重要ルール:

- Core 0からモータを直接動かさない
- Core 1からLCDを直接描画しない
- UI処理とモーション処理を混ぜない
- FastAccelStepperはStepperBackend内に閉じ込める
- TMC2209 UARTはTMC2209Manager内に閉じ込める
- CoreXY変換はCoreXYKinematics内に閉じ込める

---

## 4. M5Stack Core2 ピン制約

以下のピン制約を守ること。

| GPIO | 扱い |
|---:|---|
| GPIO21 / GPIO22 | 内部I2C。AXP192、RTC、IMU、Touch等で使用。STEP/DIR/ENに使わない |
| GPIO1 / GPIO3 | USB Serial。STEP/DIR/ENに使わない |
| GPIO34 / GPIO35 / GPIO36 / GPIO38 | 入力専用系。出力に使わない |
| GPIO0 / GPIO2 | boot strap系。原則としてmotion出力に使わない |
| GPIO32 / GPIO33 | PORT-A。外部I2Cを使わない場合はGPIO/PWMとして使用可 |
| GPIO14 / GPIO13 | PORT-C。TMC2209 UART用として使用 |
| GPIO25 / GPIO26 / GPIO27 / GPIO19 / GPIO23 | M-BUS経由でモータ制御に使用 |

---

## 5. 採用ピン割り付け

初期ピン割り付けは以下で固定する。

| 用途 | 信号 | GPIO | 備考 |
|---|---|---:|---|
| Aモータ | `MOTOR_A_STEP_PIN` | 25 | CoreXY A = X + Y |
| Aモータ | `MOTOR_A_DIR_PIN` | 26 | CoreXY A = X + Y |
| Bモータ | `MOTOR_B_STEP_PIN` | 27 | CoreXY B = X - Y |
| Bモータ | `MOTOR_B_DIR_PIN` | 19 | CoreXY B = X - Y |
| A/B共通 | `MOTOR_EN_PIN` | 23 | Low active想定 |
| TMC UART | `TMC_UART_TX_PIN` | 14 | ESP32 TX → TMC2209 PDN_UART |
| TMC UART | `TMC_UART_RX_PIN` | 13 | ESP32 RX ← TMC2209 PDN_UART |
| Xリミット | `X_LIMIT_PIN` | 36 | 入力専用。外付けpull-up推奨 |
| Yリミット | `Y_LIMIT_PIN` | 35 | 入力専用。外付けpull-up推奨 |
| ペン | `PEN_SERVO_PIN` | 32 | Servo PWM |
| 予備 | `USER_IO_PIN` | 33 | 将来予備 |

---

## 6. TMC2209 UART方針

TMC2209は2個を同一UARTバスに接続し、UARTアドレスを変えて制御する。

```text
Core2 GPIO14 TX ---- 1kΩ ----+---- TMC2209 A PDN_UART
                             |
                             +---- TMC2209 B PDN_UART
                             |
Core2 GPIO13 RX -------------+
```

アドレス割り付け:

| ドライバ | UARTアドレス | 役割 |
|---|---:|---|
| TMC2209 A | 0 | CoreXY A = X + Y |
| TMC2209 B | 1 | CoreXY B = X - Y |

注意:

- UARTは設定・診断用であり、モータをリアルタイムに動かす信号ではない
- 実際のモータ駆動はSTEP/DIRで行う
- TMC2209のアドレス設定は使用モジュールのMS1/MS2やジャンパ仕様に従う
- モーション中にUARTを高頻度で読まない
- SG_RESULT等の診断は必要時のみ有効化する

---

## 7. タスク構成

FreeRTOSタスクは以下を基本とする。

| タスク | Core | 優先度目安 | 役割 |
|---|---:|---:|---|
| `uiTask` | 0 | 1 | LCD、Touch、Button、状態表示 |
| `commandTask` | 0 | 2 | Serial入力、コマンド解析、CommandQueue投入 |
| `logTask` | 0 | 1 | LogQueueからSerial/LCDへ出力 |
| `motionTask` | 1 | 5 | CommandQueue処理、Safety、MotionBlock生成 |
| `stepperFeedTask` | 1 | 6 | FastAccelStepperへの投入、実行管理 |
| `tmcTask` | 1 | 3 | TMC2209初期化、電流、microstep、chop設定、低頻度診断 |
| `safetyTask` | 1 | 4 | limit switch、E-stop、alarm管理 |

優先順位の基本:

```text
stepperFeedTask > motionTask > safetyTask > tmcTask > commandTask > uiTask
```

ただし、STEPパルスをタスクで直接出してはいけない。  
STEPパルス生成はFastAccelStepperに任せる。

---

## 8. Core間通信

Core間はキューでつなぐ。

| キュー | 方向 | 中身 | 目的 |
|---|---|---|---|
| `CommandQueue` | Core 0 → Core 1 | `CommandMessage` | UI/Serialからの操作指示 |
| `StatusQueue` | Core 1 → Core 0 | `StatusMessage` | LCD表示用状態 |
| `LogQueue` | Core 1 → Core 0 | `LogMessage` | ログ出力 |
| `MotionQueue` | Core 1内部 | `MotionBlock` | 将来のplanner用 |
| `SegmentQueue` | Core 1内部 | `MotionSegment` | 将来のtimed segment用 |

禁止:

- Core 0からFastAccelStepperを呼ばない
- Core 0からTMC2209Managerを直接操作しない
- Core 1からM5.Lcdを直接触らない
- Core 1から大量のSerial.printをしない

---

## 9. ソフトウェア階層

将来の完成形は以下。

```text
CommandInput
  -> CommandDispatcher
  -> CommandQueue
  -> MotionController
  -> SafetyManager
  -> MotionBlockBuilder
  -> PlannerQueue
  -> TrapezoidPlanner
  -> JunctionPlanner
  -> CoreXYKinematics
  -> SegmentGenerator
  -> StepperBackendFastAccel
  -> FastAccelStepper
  -> TMC2209 STEP/DIR Driver
```

並列に以下が存在する。

```text
TMC2209Manager
  -> Serial2 UART
  -> TMC2209 A/B
```

```text
PenController
  -> Servo / Solenoid
```

```text
Diagnostics
  -> StatusQueue / LogQueue
  -> Core 0 UI / Serial
```

---

## 10. モジュール責務

### `CommandInput`

- Serialから1行取得
- 将来SD/Web/G-code入力に拡張可能にする

### `CommandDispatcher`

- 入力文字列を`CommandMessage`へ変換
- モータは動かさない
- CoreXY変換もしない

### `MachineState`

- 現在X/Y位置
- A/Bステップ位置
- feed
- enabled
- homed
- pen状態
- alarm状態
- TMC ready状態

### `SafetyManager`

- soft limit確認
- limit switch確認
- alarm管理
- E-stop placeholder
- homing前移動制限の将来対応

### `CoreXYKinematics`

- XY差分をA/B差分へ変換する
- CoreXY式はこのモジュール以外に書かない

### `MotionBlock`

- 将来のplanner単位
- 1本の移動線分を表す

### `PlannerQueue`

- MotionBlockを溜める
- 将来look-aheadの対象になる

### `TrapezoidPlanner`

- 将来の台形加減速
- 初期はplaceholderでよい

### `JunctionPlanner`

- 将来のlook-ahead、junction speed、junction deviation
- 初期はplaceholderでよい

### `SegmentGenerator`

- 将来のA/B同期timed segment生成
- 初期はplaceholderでよい

### `StepperBackendFastAccel`

- FastAccelStepperを隠蔽する
- STEP/DIR/ENを扱う
- A/Bモータ単独テストを提供する
- 初期はbring-up用に独立A/B moveを許容する

### `TMC2209Manager`

- Serial2を初期化する
- TMC2209 A/Bをアドレス違いで扱う
- 電流、microstep、chop modeを設定する
- 低頻度診断を行う

### `PenController`

- ペン上げ/下げ
- サーボ角度制御
- 将来M3/M5に接続する

### `Diagnostics`

- POS
- CONFIG
- SELFTEST
- TMC_STATUS
- 状態表示
- ログ整形

---

## 11. CoreXY変換ルール

CoreXY変換は`CoreXYKinematics`だけで行う。

```cpp
dx_mm = target_x_mm - current_x_mm;
dy_mm = target_y_mm - current_y_mm;

da_mm = dx_mm + dy_mm;
db_mm = dx_mm - dy_mm;

a_steps = round(da_mm * steps_per_mm);
b_steps = round(db_mm * steps_per_mm);
```

方向確認:

```text
+X      -> A positive, B positive
+Y      -> A positive, B negative
+X +Y   -> A positive, B approximately zero
+X -Y   -> A approximately zero, B positive
```

---

## 12. FastAccelStepper使用ルール

FastAccelStepperは`StepperBackendFastAccel`以外から直接呼ばない。

許可:

```text
MotionTask
  -> StepperBackend
  -> StepperBackendFastAccel
  -> FastAccelStepper
```

禁止:

```text
main.cpp
  -> FastAccelStepper
```

初期の`moveABSteps()`はbring-up用途として許可する。  
ただし、以下のコメントを残すこと。

```cpp
// Bring-up only:
// Independent A/B move() calls do not guarantee strict XY interpolation.
// Future implementation must replace this path with MotionBlock,
// PlannerQueue, SegmentGenerator, and timed A/B segment execution.
```

---

## 13. 初期Serialコマンド

初期コマンド:

```text
HELP
CONFIG
POS
ENABLE
DISABLE
ZERO
TEST_A <steps>
TEST_B <steps>
XY <x_mm> <y_mm> <feed_mm_min>
PENUP
PENDOWN
SELFTEST
TMC_INIT
TMC_STATUS
```

将来G-code対応:

```text
G0 X Y F
G1 X Y F
G20
G21
G28
G90
G91
M3
M5
M114
```

初期段階ではG-code parserは実装しない。

---

## 14. 禁止事項

以下は禁止する。

- `main.cpp`にCoreXY変換式を書く
- `main.cpp`からFastAccelStepperを直接呼ぶ
- `main.cpp`からTMC2209 UARTを直接操作する
- parserからモータを直接動かす
- Core 0からモータを直接動かす
- Core 1からLCDを直接描画する
- `delayMicroseconds()`でSTEPパルスを生成する
- time-critical処理内でSerial.printする
- look-aheadをStepperBackend内に実装する
- junction deviationをStepperBackend内に実装する
- TMC UART設定をplanner内に混ぜる
- ペン制御をstepper backendに混ぜる

---

## 15. Build/Test方針

変更後は以下を行う。

```bash
pio run
```

可能なら`SELFTEST`を実行する。

最初の確認順:

```text
CONFIG
POS
SELFTEST
ZERO
XY 10 0 600
ZERO
XY 0 10 600
ZERO
XY 10 10 600
TMC_INIT
TMC_STATUS
```

---

## 16. このプロジェクトで重視すること

- 単位を変数名に書く
- 層を分ける
- Core間通信はキューで行う
- Core2のピン制約を守る
- TMC2209 UARTをモーションから分離する
- UIとmotionを分離する
- 最初はSIMULATION_MODEで検証する
- 将来の挿入場所を塞がない
