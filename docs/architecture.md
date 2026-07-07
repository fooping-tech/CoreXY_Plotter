# アーキテクチャ(as-built)

本書は「完成形」ではなく**現状**のモジュール構成・タスク実態・queue接続を記述する。
将来の完成形はAGENTS.md §7/§9にあり、本書末尾で差分を明記する。
コマンド仕様は[command_reference.md](command_reference.md)、機能仕様は`SPEC.md`を参照。

最終更新: 2026-07-08(リファクタリングRF1完了時点)

## タスク構成(現状)

| タスク | Core | 優先度 | 実際にやっていること |
|---|---:|---:|---|
| `uiTask` | 0 | 1 | LCD描画、Touch/Button入力。**加えてLED lifecycle全体**(`NeoPixelController::begin()`、`LedCommandQueue` drain、`LedPatternEngine::tick()`)を駆動する |
| `commandTask` | 0 | 2 | Serial 1行読み取り→`CommandDispatcher::parse`→キュー投入。motion系はqueue満杯でも破棄せず待つ |
| `logTask` | 0 | 1 | `LogQueue`→`Serial.println`。drop計数があれば`WARN: LogQueue dropped N messages`を報告 |
| `motionTask` | 1 | 5 | コマンドdispatch(テーブル駆動)、**timed segmentのbackend投入と完了待ちを含むmotion実行全体**、safety poll |
| `stepperFeedTask` | 1 | 6 | **placeholder(100ms sleepのみ)**。AGENTS §7の「FastAccelStepper投入」は実際にはmotionTask内の`TimedSegmentExecutor`が行う |
| `tmcTask` | 1 | 3 | **placeholder(1s sleepのみ)**。TMC初期化は`TMC_INIT`/motion前の自動initとしてmotionTask内で実行される |
| `safetyTask` | 1 | 4 | 100ms周期でlimit poll、alarm→LED状態反映、`job_active`/`motion_active`同期、status publish |

## motion系モジュール(RF1分割後)

`src/tasks/MotionTask.cpp`(約400行)はコマンド受信・dispatch・方針関数(abort/alarm/motion active)に限定し、
実行は以下の抽出モジュールが担う。所有関係: planner群・queue群は`XYMotionPlanner`のメンバ、
実行エンジンは`TimedSegmentExecutor`で、いずれもmotionTask内の関数local staticとして所有される。

```text
motionTask (dispatchテーブル)
  -> XYMotionPlanner        block生成→lookahead収集→junction/trapezoid計画→実行
       所有: TrapezoidPlanner, JunctionPlanner, SegmentGenerator,
             PlannerQueue(16), SegmentQueue(512)
  -> TimedSegmentExecutor   timed segmentのbackend投入retry・完了待ち・位置推定・
                            位置信頼性喪失時のalarm
  -> JobLifecycleHandler    JOB_BEGIN自動homing、JOB_END(pen up→park→ジングル)
  -> MotionDiagnostics      AB_TIMED診断経路、TEST_A/B単独モータ診断
  -> GcodeCommandTranslator GcodeInterpreterのログ整形グルー+modal reset
  -> MotionSyncTracker      backend/MachineState同期とdrift検出(純ロジック、nativeテストあり)
```

方針関数(stopForAbort/setMotionActive/enterAlarm/コマンド受信)はMotionTask側に置き、
各モジュールへ関数ポインタのフックとして注入する。抽出モジュールはbackend/safety/machineを
参照渡しで受け取り、グローバル直接参照を増やさない(例外: ログ用の`logMessage`/`postLedStatus`)。

閉じ込めルール(維持):

- FastAccelStepper依存は`StepperBackendFastAccel`のみ
- TMC UART依存は`TMC2209Manager`のみ
- CoreXY変換(順・逆)は`CoreXYKinematics`のみ

## LED系モジュール(RF1.4分割後)

```text
commandTask / motionTask / safetyTask --LedCommandQueue--> uiTask
  -> LedPatternEngine   コマンド解釈・状態機械。応答はコールバック(uiTaskがlogMessageへ接続)
       -> LedStatusConfig.h  LedStatus→pattern/color/速度のconstexprテーブル
       -> LedRenderer        生アニメーション描画の純関数群
       -> NeoPixelController FastLED出力
```

## キュー接続(現状)

| キュー | 方向 | 型 | 備考 |
|---|---|---|---|
| `CommandQueue`(16) | Core 0 → motionTask | `CommandMessage` | motion系は満杯でも破棄せず投入待ち |
| `StatusQueue`(1) | Core 1 → uiTask | `StatusMessage` | `xQueueOverwrite`で最新のみ保持 |
| `LogQueue`(24) | 全タスク → logTask | `LogMessage` | 満杯時はdrop計数(silent dropしない) |
| `LedCommandQueue`(12) | 各タスク → uiTask | `LedCommand` | drop時は`WARN`ログ(postLedStatus) |
| `PlannerQueue`(16) | motionTask内部 | `MotionBlock` | XYMotionPlannerが所有 |
| `SegmentQueue`(512) | motionTask内部 | `MotionSegment` | 同上。stepperFeedTaskへは渡していない |

## AGENTS.md §7/§9「完成形」との差分

1. **stepperFeedTaskは未使用。** §7は「FastAccelStepperへの投入、実行管理」を最高優先度taskへ
   割り当てるが、現状はmotionTaskが`TimedSegmentExecutor`経由で同期的に投入・完了待ちする。
   移行にはCore 1内のtask間同期設計が必要で、挙動不変リファクタの範囲を超えるため
   設計課題として保留(PLANS_REFACTORING.md RR2)。
2. **tmcTaskは未使用。** TMC初期化・自動initはmotionTask内。低頻度診断も未実装。
3. **`MotionController`という単一クラスは存在しない。** §9のMotionController相当の責務は
   motionTaskのdispatch+XYMotionPlanner+TimedSegmentExecutorに分かれている。
4. **LED lifecycleはuiTaskが兼務。** taskを分ける場合はCore 0負荷の再評価が必要。
5. `AppContext.h`のグローバルextern hub構造は当面維持(新モジュールは参照渡しで受け取る)。

## ホストツール構成

```text
tools/common/plotter_gcode.py   G-code emitter・トークナイザ・feed既定値(共通)
tools/serial_tool/serial_send.py  CSV/G-code送信、ACK待ち、timeout推定(pyserial依存はここに閉じる)
tools/qr_tool/ tools/text_tool/   G-code生成CLI(commonのGcodeEmitterを使用)
tools/webui/server.py             HTTP層+subprocess管理(serial_send.pyへ委譲)
tools/webui/gcode_processing.py   SVG/画像→G-code変換パイプライン
tools/webui/webui_settings.py     送信設定の正規化とCLI引数構築
```
