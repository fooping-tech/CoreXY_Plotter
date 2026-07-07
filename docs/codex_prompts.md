# Codex用プロンプト(アーカイブ)

旧`PLANS.md` §13から移動。各Phase実装時にCodexへ渡したプロンプトの記録。
実装完了済みPhaseの資料であり、現在の仕様の正は`SPEC.md`である。


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
