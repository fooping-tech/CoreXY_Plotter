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
- look-ahead
- junction deviation
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
| A/B共通EN | `MOTOR_EN_PIN` | 23 | Low active |
| TMC UART TX | `TMC_UART_TX_PIN` | 14 | Serial2 TX |
| TMC UART RX | `TMC_UART_RX_PIN` | 13 | Serial2 RX |
| Xリミット | `X_LIMIT_PIN` | 36 | 入力専用。外付けpull-up推奨 |
| Yリミット | `Y_LIMIT_PIN` | 35 | 入力専用。外付けpull-up推奨 |
| ペンサーボ | `PEN_SERVO_PIN` | 32 | PWM |
| NEOPIXEL | `NEOPIXEL_PIN` | 33 | 外付けNEOPIXEL。データ入力 |

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
| motor | `ENABLED` / `DISABLED` |
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
| `DEFAULT_FEED_MM_MIN` | `600.0f` |
| `MAX_FEED_MM_MIN` | `1200.0f` |
| `DEFAULT_MOTOR_SPEED_STEPS_S` | `3000` |
| `MAX_MOTOR_SPEED_STEPS_S` | `5000` |
| `DEFAULT_MOTOR_ACCEL_STEPS_S2` | `10000` |
| `X_MIN_MM` | `0.0f` |
| `X_MAX_MM` | `300.0f` |
| `Y_MIN_MM` | `0.0f` |
| `Y_MAX_MM` | `300.0f` |
| `SERIAL_BAUD` | `115200` |
| `TMC_UART_BAUD` | `115200` |
| `DIR_CHANGE_DELAY_US` | `200` |
| `SIMULATION_MODE` | `1` |

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

```cpp
dx_mm = target_x_mm - current_x_mm;
dy_mm = target_y_mm - current_y_mm;

da_mm = dx_mm + dy_mm;
db_mm = dx_mm - dy_mm;

a_steps = round(da_mm * steps_per_mm);
b_steps = round(db_mm * steps_per_mm);
```

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

  bool enabled;
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
| `ENABLE` | モータEnable |
| `DISABLE` | モータDisable |
| `ZERO` | 論理原点リセット。homingではない |
| `TEST_A <steps>` | Aモータ単独テスト |
| `TEST_B <steps>` | Bモータ単独テスト |
| `XY <x_mm> <y_mm> <feed_mm_min>` | XY移動またはsimulation |
| `PENUP` | ペン上げ |
| `PENDOWN` | ペン下げ |
| `SELFTEST` | CoreXY変換等の自己診断 |
| `TMC_INIT` | TMC2209初期化 |
| `TMC_STATUS` | TMC2209状態表示 |
| `LED <r> <g> <b>` | 外付けNEOPIXEL全灯をRGB指定で点灯 |
| `LED_PIXEL <index> <r> <g> <b>` | 指定indexのNEOPIXELをRGB指定で点灯 |
| `LED_OFF` | 外付けNEOPIXELを消灯 |
| `MELODY` | 診断用モータメロディを明示実行 |

---

## 16. `XY`コマンド仕様

処理順:

1. target X/Y/feedをparse
2. SafetyManagerでsoft limit確認
3. feed確認
4. currentからdelta計算
5. CoreXYKinematicsでA/B step算出
6. ログ出力
7. SIMULATION_MODEなら実モータ出力なし
8. Real modeならStepperBackendへ渡す
9. 成功時のみMachineState更新

ログ例:

```text
XY target=(10.000,0.000) current=(0.000,0.000) dx=10.000 dy=0.000 A=800 B=800 F=600.000
SIMULATION_MODE: no motor output
```

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
| current | 600〜800mAから開始 |
| mode | 初期はspreadCycle寄り |
| diagnostics | 低頻度 |

TMC2209ManagerはFastAccelStepperやplannerに依存しない。

実装では`TMCStepper`を使用し、共有`Serial2`上のアドレス`0`と`1`へ個別に
レジスタ設定を書き込む。`TMC_STATUS`では両ドライバの`test_connection()`、
`IFCNT`、microsteps、RMS current、IRUN/IHOLD、chop mode等を確認できる。

### 診断用モータメロディ仕様

`../1stepper_test`の起動メロディを参考に、ステッパモータのSTEP周波数を音符ごとに変更して短いメロディ風の音を鳴らす。

この機能はbring-upと診断専用とする。CoreXY位置ずれを避けるため起動時に自動実行せず、`MELODY`コマンドでのみ明示実行する。

実行ルール:

- `SIMULATION_MODE=1`ではモータを動かさず、予定音符と設定変更をログ表示する
- 実行前にmotionがidle、alarmなし、limit入力inactive、TMC UART readyであることを確認する
- A/Bモータは診断対象を明確にして実行する
- 音符ごとにSTEP周波数と長さを指定し、短い正逆交互移動で移動量を抑える
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

1200mAは参照実装の値であり、使用するモータ、TMC2209モジュール、電源、放熱条件を確認してから確定する。

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
| X | 0〜300 mm |
| Y | 0〜300 mm |

limit switch:

| 信号 | GPIO | active |
|---|---:|---|
| X_LIMIT | 36 | Low |
| Y_LIMIT | 35 | Low |

GPIO35/36は内部pull-upに頼らず、外付けpull-upを前提とする。

---

## 19. Pen仕様

初期ペン制御:

| 信号 | GPIO |
|---|---:|
| PEN_SERVO | 32 |

角度初期値:

| 状態 | 角度 |
|---|---:|
| PEN_UP | 30度 |
| PEN_DOWN | 70度 |

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

## 20. FastAccelStepper仕様

FastAccelStepperは`StepperBackendFastAccel`のみが直接使用する。

初期の`moveABSteps()`はbring-up用であり、厳密なXY補間を保証しない。

将来的には以下へ移行する。

```text
MotionBlock
  -> PlannerQueue
  -> TrapezoidPlanner
  -> SegmentGenerator
  -> StepperBackendFastAccel moveTimed or low-level queue
```

---

## 21. 将来G-code仕様

初期では実装しない。  
将来対応予定:

| G-code | 意味 |
|---|---|
| G0 | rapid move |
| G1 | linear feed move |
| G20 | inch |
| G21 | mm |
| G28 | homing |
| G90 | absolute |
| G91 | relative |
| M3 | pen down |
| M5 | pen up |
| M114 | position report |

F値はmm/minとして扱う。

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
