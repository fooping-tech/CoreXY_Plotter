# PLANS_REFACTORING.md

# M5Stack Core2 CoreXYペンプロッタ リファクタリング計画・進捗管理

Version: 0.1
Status: Refactoring plan (progress-tracking)
Scope: ファームウェア(C++)、ホストツール(Python/JS)、ドキュメント、テスト/CI、リポジトリ衛生
Relation: 機能実装の計画・進捗は`PLANS.md`が正。本ファイルはリファクタリング専用。両方に関わる変更は両方を更新する。

---

## 0. このファイルの使い方

このファイルは、リファクタリング計画であり、同時に進捗管理表である。
チェック状態の意味は`PLANS.md`と同じ。

```text
[ ] 未着手
[x] 完了
[-] 一部完了、または保留
[!] 問題あり。修正が必要
```

作業後に必ず更新する項目:

- 「1. 現在地」
- 該当RFフェーズのチェックリスト
- 「6. 変更履歴」
- 必要なら「5. リスク・未解決事項」

---

## 1. 現在地

```text
現在地: RF0 未着手。調査完了(2026-07-02)。全指摘はこの時点のコードに基づく。
```

現在の最優先作業:

```text
RF0(安全網の整備)を先に完了させる。テストなしで挙動を変えるリファクタリングを始めない。
```

---

## 2. リファクタリングの原則

1. **挙動不変を基本とする。** 各リファクタリングは外部から見える動作(Serialコマンド応答、G-code解釈、motion挙動、LCD/LED表示)を変えない。挙動が変わる改善は`PLANS.md`側の機能変更として扱う。
2. **安全網を先に張る。** 対象コードにテストが無い場合、可能な範囲で先にnative/hostテストを追加してから動かす(RF0)。
3. **1コミット1リファクタ。** 抽出・移動・改名を機能修正と混ぜない。
4. **検証手順はAGENTS.md §16に従う。** ファームウェア変更ごとに`pio run`(realとsim両env)を通し、可能なら実機で`CONFIG`/`POS`/`SELFTEST`を確認する。実機確認できない場合は未実行と理由を報告する。
5. **motion経路のリファクタリングは実機回帰確認とセットにする。** 特にRF1.1〜RF1.2は、`tools/serial_tool/examples/`の既存check CSV(homing、gcode、job_lifecycle、lookahead)を回帰手順として使う。
6. **AGENTS.mdの階層ルールを維持する。** FastAccelStepperは`StepperBackendFastAccel`、TMC UARTは`TMC2209Manager`、CoreXY変換は`CoreXYKinematics`に閉じたまま動かす(現状この3ルールは守られていることを2026-07-02調査で確認済み)。
7. **ドキュメント同期(AGENTS.md §15)。** リファクタリングで構造・タスク役割・コマンド仕様の記述が実態と変わる場合、`SPEC.md`/`AGENTS.md`/`PLANS.md`を同時更新する。

---

## 3. フェーズ一覧

| Phase | 名前 | 目的 | 依存 | 状態 |
|---:|---|---|---|---|
| RF0 | 安全網(テスト/CI基盤) | リファクタ前に回帰検出手段を整える | なし | [ ] |
| RF1 | ファームウェア構造 | MotionTask分割、重複排除、config集約 | RF0 | [ ] |
| RF2 | ホストツール | 共通モジュール化、テスト統一、依存整理 | RF0 | [ ] |
| RF3 | ドキュメント | 陳腐化修正、PLANS.md分割、リファレンス一元化 | なし(即時可) | [ ] |
| RF4 | リポジトリ衛生 | 生成物削除、.gitignore、placeholder整理 | なし(即時可) | [ ] |

推奨順序: RF3.1(即時修正)とRF4は独立で先行可。コード変更(RF1/RF2)はRF0完了後。

---

# RF0: 安全網(テスト/CI基盤)

## 目的

リファクタリングの回帰を機械的に検出できる状態を作る。

## 背景(2026-07-02調査)

- nativeテストは`test/native/test_motion_drift.cpp`の2項目のみ(CoreXY閉ループstep保存、SegmentGenerator分解合計)。対象は良いが範囲が狭い。
- ランナー`tools/run_native_motion_tests.sh`はPOSIX `c++`前提かつ出力先`/tmp`固定で、Windows環境で動かない。PlatformIOの`pio test`にも接続されていない(`platformio.ini`に`[env:native]`なし)。
- Pythonテストは`tests/`(pytest形式3本)と`tools/webui/`(unittest形式2本)に分散し、フレームワークも場所も不統一。`pytest tests/`ではwebui分が実行されない。
- `server.py`(1182行)と`app.js`(2163行)にはテストが無い。
- CIが存在しない(`.github/workflows`なし)。テスト実行方法はどのREADMEにも書かれていない。

## チェックリスト

### RF0.1 nativeテストのPlatformIO統合

- [ ] `platformio.ini`へ`[env:native]`(platform=native)を追加し、`pio test -e native`で既存2テストが走る
- [ ] `test/native/Arduino.h`スタブを`pio test`構成と両立させる(またはUnityへ移行)
- [ ] `tools/run_native_motion_tests.sh`の`/tmp`固定を廃止し、`pio test`へ委譲またはポータブル化する
- [ ] RF1.1で抽出予定のモジュール(drift追跡、XYブロック生成、no-op判定)を先にテスト可能な範囲でカバーする

### RF0.2 hostテストの統一

- [ ] `tools/webui/test_image_to_svg.py`と`test_svg_to_gcode.py`を`tests/`配下へ移動し、pytest形式へ統一する
- [ ] `conftest.py`または`pyproject.toml`で`sys.path.insert`の重複を整理する
- [ ] `pytest`一発で全hostテストが走ることを確認する

### RF0.3 CIと手順の明文化

- [ ] GitHub Actions workflowを追加する(`pio run`両env、`pio test -e native`、`pytest`)
- [ ] READMEへテスト実行方法(`pio test -e native`、`pytest`)を記載する
- [ ] AGENTS.md §16へnativeテストとpytestを検証手順として追記する

## RF0 完了条件

- [ ] Windows/Linuxの両方で全テストが1コマンドずつで実行できる
- [ ] CIがpush時にbuild+testを実行する

---

# RF1: ファームウェア プログラムリファクタリング

## 目的

`MotionTask.cpp`の肥大化解消、重複コードの排除、設定値のconfig集約、タスク役割と実装の整合。

## RF1.1 MotionTask.cpp分割(最重要)

`src/tasks/MotionTask.cpp`は1075行で、無名namespace内に約40個の関数と6個のファイルstaticなplannerオブジェクトを持ち、`motionTask()`本体は220行のコマンドswitchになっている。以下の責務が混在している(行番号は2026-07-02時点)。

| 責務 | 行 | 抽出先案 |
|---|---|---|
| pipelineシングルトン群(TrapezoidPlanner等6個) | 14–20 | `MotionController`が所有するメンバへ |
| drift追跡・backend位置推定 | 24–36, 86–143 | `MotionSyncTracker`(純ロジック、nativeテスト可) |
| abort/alarm停止シーケンス | 145–159, 185–196 | `enterAlarm()`共通関数(RF1.2b) |
| timed segment実行エンジン | 161–248 | `TimedSegmentExecutor` |
| Job lifecycle接続(auto home、park、終了処理) | 295–338, 685–742 | `JobController`近傍へ |
| G-code変換グルー | 369–392 | `GcodeInterpreter`側へ |
| XYブロック生成・計画・実行 | 401–683 | `XYMotionPlanner`(約280行) |
| AB_TIMED診断経路 | 744–833 | 診断モジュールへ |
| コマンドdispatch switch | 856–1075 | テーブル駆動dispatch |

- [ ] `MotionSyncTracker`を抽出し、nativeテストを追加する
- [ ] `TimedSegmentExecutor`を抽出する
- [ ] `XYMotionPlanner`(buildXYBlock / planQueuedBlocks / executePlannedBlock / handleXYBatch)を抽出する
- [ ] Job lifecycle接続処理を`JobController`側へ寄せる
- [ ] `motionTask()`のswitchをテーブル駆動またはハンドラ関数群へ整理する
- [ ] 分割後もplanner系がファイルstaticグローバルでなく所有関係で持たれている
- [ ] 分割の各段階で`pio run`両envが通り、homing/gcode/job_lifecycle check CSVで回帰確認する

補足(設計判断が必要、勝手に変えない):

- `stepperFeedTask`(`src/tasks/StepperFeedTask.cpp`)と`tmcTask`(`src/tasks/TmcTask.cpp`)は空のplaceholderのままで、AGENTS.md §7が両taskへ割り当てた役割(stepper投入管理、TMC init)は実際には`motionTask`内で実行されている。最高優先度(6)のtaskが何もしていない。**実装をtaskへ移すか、AGENTS.md/SPEC.mdの記述を実態(motionTask内実行)へ合わせるかを決めてから着手する。** 前者はCore 1のtask間同期設計が必要で挙動不変リファクタの範囲を超えるため、初期はドキュメント側の整合(RF3.5)を推奨。

## RF1.2 重複コードの排除

- [ ] (a) `StatusMessage`手組みスナップショットの一本化: `MotionTask.cpp:72–77`と`main.cpp:43–47`が同一の4引数構築。`StatusMessage::capture()`等のファクトリへ
- [ ] (b) alarm突入シーケンスの一本化: backend停止→setAlarm→`alarmed=true`→homed無効化→LED ERROR→ログ、が`MotionTask.cpp:145–159, 185–196, 986–998, 1015–1026`の4箇所に重複。`enterAlarm(reason, LedStatus)`へ
- [ ] (c) `job_active`同期の一本化: `MotionTask.cpp:38–41`と`SafetyTask.cpp:17–18`が同一式。さらに`JobController::isActive()`はRUNNINGを含むため`|| isRunning()`は冗長 → 共通ヘルパ化し冗長条件を除去(挙動不変を確認の上)
- [ ] (d) LED `SET_STATUS`送信の一本化: `MotionTask.cpp:43–51`(drop時WARN)と`SafetyTask.cpp:9–14`(drop無視)で失敗ポリシーも不一致。共通ヘルパで統一
- [ ] (e) `HOME`/`HOME_X`/`HOME_Y`ケース(`MotionTask.cpp:926–967`)の3重複をパラメタ化
- [ ] (f) AB_TIMEDのbackend queue状態ログ5重複(`MotionTask.cpp:780–830`)を`logBackendQueueState()`へ
- [ ] (g) `ACK_XY`/`NACK_XY`ログのフォーマット重複(ACK 3箇所、NACK 8箇所)を`ackXY()`/`nackXY()`ヘルパへ
- [ ] (h) `square()`の2重定義(`TrapezoidPlanner.cpp:10–12`、`JunctionPlanner.cpp:12–14`)と3種のclampイディオム(`UiTask.cpp:330`の自前`clampFloat`、Arduino `constrain`、`static_assert`)を共通mathユーティリティへ

## RF1.3 UiTaskの整理

`src/tasks/UiTask.cpp`(511行)は描画・タッチ入力・レイアウト・motionポリシーが混在している。

- [ ] 描画rectとタッチhit-test rectの二重リテラル(例: `{10, 42, 145, 36}`が`UiTask.cpp:229`と`369`に重複)を、単一の`Rect`レイアウトテーブルへ集約し描画とhit-test両方で参照する
- [ ] 色定数、画面寸法(320/214)、notice timeout(1800ms)等のマジックナンバーを整理する
- [ ] jogの soft limit clamp・base追跡(`queueJog`, `UiTask.cpp:336–365`)のmotionドメインロジックをUI外へ寄せる、または境界をコメントで明確化する
- [ ] `UI_JOG_FEED_MM_MIN`/`UI_JOG_STEP_MM`(`UiTask.cpp:35–36`)を`PlotterConfig.h`へ移す
- [ ] uiTaskがLED lifecycle(`neopixel_controller.begin()`、`led_command_queue` drain、`tick()`)も駆動している構造を、少なくともAGENTS.md/SPECに明文化する(taskを分ける場合はCore 0負荷を再評価)

## RF1.4 LedPatternEngineの分離

`src/LedPatternEngine.cpp`(408行)は (1)コマンド解釈・状態機械 (2)状態→演出マッピング (3)生アニメーション描画、の3責務を持つ。

- [ ] renderer群(`renderPacifica/Fire/Breath/Chase/Progress/Alert/Success`、268–355行)を純関数の`LedRenderer`へ分離する
- [ ] 状態→演出テーブル(`applyStatusConfig`、164–235行)のハードコード値(輝度/色相/速度、transient 2500ms等)を`PlotterConfig.h`のNeoPixelセクションへ移す
- [ ] engine内からの`logMessage`直接呼び出しを応答コールバックまたは戻り値へ置き換え、engineをグローバルI/O非依存にする

## RF1.5 config集約の徹底

`PlotterConfig.h`自体は単位付き命名と`static_assert`検証があり良好。だが各所に直書き定数が残る。

- [ ] `Diagnostics.cpp:104–106`のSELFTEST期待値(800/1600)を`STEPS_PER_MM`から導出する(`STEPS_PER_MM`変更で自壊するのを防ぐ)
- [ ] `SafetyTask.cpp:23`の100msポーリング、`CommandTask.cpp:23,70,86`の10ms/50msを`TaskConfig.h`へ
- [ ] RF1.3/RF1.4のUI・LED定数移動と合わせ、「調整可能定数はconfigへ集約」(PLANS.md §0.2.1)の原則へ再整合する

## RF1.6 その他の構造改善

- [ ] `CoreXYKinematics`へ逆変換API(A/B steps→XY)を追加し、`MotionTask.cpp:97–109`のインライン逆変換 `(delta_a_mm ± delta_b_mm) * 0.5f` を置き換える(CoreXY式の集約ルールを逆方向にも適用)
- [ ] `StatusMessage`/`LogMessage`を`MachineState.h`から`Messages.h`等へ移し、`CommandMessage.h`/`LedTypes.h`と並べる
- [ ] `logMessage`のログ用途とコマンド応答用途("OK:"/"ACK_"/"NACK_"/"ERROR:"文字列プレフィクス)の混在を整理する。最低限プレフィクス文字列を定数化、可能ならseverity/応答種別のenum化(Serialプロトコル互換を壊さないこと)
- [ ] `logMessage`のqueue full時silent drop(`main.cpp:34–36`)のポリシーを決めて明文化する(drop計数など)
- [ ] `AppContext.h`のグローバルextern hub構造は当面維持でよいが、RF1.1で抽出する新モジュールは参照渡しで受け取り、グローバル直接参照を増やさない

## RF1 完了条件

- [ ] `MotionTask.cpp`が400行以下になり、抽出モジュールにnativeテストがある
- [ ] RF1.2の重複8項目が解消されている
- [ ] `pio run`両env、`pio test -e native`、実機`SELFTEST`+主要check CSVが通る

---

# RF2: ホストツール リファクタリング

## 目的

Pythonツール間の重複排除と、巨大単一ファイルの分割。

## 背景(2026-07-02調査)

| ファイル | 行数 | テスト |
|---|---:|---|
| `tools/webui/static/app.js` | 2163 | なし |
| `tools/serial_tool/serial_send.py` | 1257 | `tests/test_serial_send.py` |
| `tools/webui/server.py` | 1182 | なし |
| `tools/qr_tool/qr_to_plot_csv.py` | 618 | `tests/test_qr_to_plot_csv.py` |
| `tools/webui/svg_to_gcode.py` | 558 | `tools/webui/test_svg_to_gcode.py` |
| `tools/webui/image_to_svg.py` | 546 | `tools/webui/test_image_to_svg.py` |
| `tools/text_tool/kst32b_to_gcode.py` | 510 | `tests/test_kst32b_to_gcode.py` |

## RF2.1 共通モジュール`tools/common/`の新設

- [ ] G-code出力ロジックの3重複を統合する: `svg_to_gcode.py:439`、`kst32b_to_gcode.py:227`、`qr_to_plot_csv.py:467`が同じ`G21/G90/M5 … M3 … G1 … M5`構造を各自実装している → 共通のG-code emitterへ
- [ ] `gcode_words()`トークナイザの2重複(`serial_send.py:423`、`server.py:568`)を統合する
- [ ] feed既定値の不一致を解消する: draw feedが`qr_to_plot_csv.py:40`=600、`svg_to_gcode.py:32`=800(`server.py:340`でも再ハードコード)、`serial_send.py:27`=1200と3種類ある。共通configへ集約し、ファームウェア側`PlotterConfig.h`の値との関係(意図的な差か否か)をコメントで明記する
- [ ] 共通化後、既存の生成G-codeサンプルと出力が一致することを回帰確認する(意図的な差異のみ許容)

注意: `server.py`が`serial_send.py`/`qr_to_plot_csv.py`/`kst32b_to_gcode.py`をsubprocessで再利用する構造は良い分離であり、維持する。pyserial依存が`serial_send.py`に閉じている点も維持する。

## RF2.2 大型ファイルの分割

- [ ] `server.py`(1182行)をAPIハンドラ/subprocess実行/G-code処理のモジュールへ分割し、分割単位でテストを追加する
- [ ] `app.js`(2163行)を画面別(Dashboard/Control/Job/Console/Settings/Fun)またはレイヤ別(API client/preview/UI)に分割する
- [ ] `serial_send.py`(1257行)のCLI/プロトコル待ち合わせ/timeout見積りを分離する(挙動が実機運用に直結するため、check CSVでの回帰確認とセットで行う)

## RF2.3 依存関係の整理

- [ ] `tools/webui/requirements.txt`が実依存(pyserial、qrcode)を含まない問題を解消する: root `requirements.txt`または`pyproject.toml`(extras方式)へ統合する
- [ ] バージョンピンの方針を統一する(現状qrcodeのみピン、pyserial/Pillowは未ピン)
- [ ] `tools/webui/README.md`のvenv手順を統合後の手順へ更新する

## RF2 完了条件

- [ ] G-code生成・トークナイザ・feed定数の重複がゼロ
- [ ] 全hostテストが`pytest`一発で通る(RF0.2)
- [ ] WebUIからのQR/テキスト/画像→G-code→送信の一連の動作が手動確認できている

---

# RF3: ドキュメント リファクタリング

## 目的

実態と矛盾する記述の解消、PLANS.mdの分割、参照の一元化。

## RF3.1 即時修正(コード変更不要、先行可)

- [ ] `README.md:76–77`の重複タイトル`# CoreXY_Plotter` ×2行を削除する
- [ ] `AGENTS.md` §13の陳腐化を解消する: 「初期Serialコマンド」12個のみ記載、「G-code parserは実装しない」と記述したままだが、実際は約40コマンド+G-code実装済み → 現状へ更新するか、コマンド一覧はRF3.3のリファレンスへの参照に置き換える
- [ ] `SPEC.md` §15コマンド表へ未記載のLEDコマンド6個(`LED_PATTERN`/`LED_BRIGHTNESS`/`LED_PARAM`/`LED_STATUS`/`LED_AUTO`/`LED_STATUS_SET`)を追加する
- [ ] `G4`の記載不整合(READMEは対応と記載、AGENTS.md §13の将来G-code一覧に無い)を解消する
- [ ] `SPEC.md` §2「非目的」にWebUIが残っている点を実態(Host WebUI実装済み)へ更新する

## RF3.2 PLANS.mdの分割(2340行 → 約1590行)

進捗計画と実行記録が混在している。以下を分割する(分割後、AGENTS.md §15の更新先指定も合わせて修正)。

- [ ] §12 手動テスト手順(1588–1980行、約393行)→ `docs/manual_tests.md`へ移し、`tools/serial_tool/docs/*.md`(18ファイル)との重複を解消する(どちらかを正とし、他方はリンクにする)
- [ ] §13 Codex用プロンプト(1981–2159行)→ `docs/codex_prompts.md`へ移す、または役目を終えたものとして削除する
- [ ] §14 リスク・未解決事項(2160–2205行)→ `docs/risks.md`へ移す(またはissue管理へ)
- [ ] §15 変更履歴(2206–2293行)→ `CHANGELOG.md`へ移す
- [ ] 分割後のPLANS.mdはPhase計画+チェックリスト+マイルストーンに限定する

## RF3.3 コマンドリファレンスの一元化

コマンド仕様がSPEC §15/§16/§21、READMEのbring-upブロック、AGENTS §13に分散し、完全かつ最新の一覧がどこにも無い。

- [ ] `docs/command_reference.md`を新設し、全Serialコマンド+対応G-codeを引数・応答(ACK/NACK/OK形式)・前提条件(homed/TMC ready/job中可否)付きで一覧化する
- [ ] SPEC/README/AGENTSのコマンド記述は要約+リンクへ置き換える
- [ ] `CommandDispatcher.cpp`のコマンド一覧と突き合わせて欠落ゼロを確認する

## RF3.4 WebUI文書の統合

WebUIの画面構成・操作ルールが`SPEC.md` §20、`docs/webui_product_design.md`、`docs/webui_wireframe.md`、`PLANS.md` §11.1の4箇所に重複記載されている。

- [ ] 正とする文書を1つ決め(推奨: SPEC §20を仕様の正、docs/はデザイン経緯資料として位置づけ)、他はリンク+差分のみへ整理する
- [ ] 画面一覧(Dashboard/Manual Control/Job/Console/Settings)の三重記載を解消する

## RF3.5 as-builtアーキテクチャ文書

- [ ] `docs/architecture.md`を新設し、現状のモジュール構成(タスク実態: timed segment投入はmotionTask内、stepperFeedTask/tmcTaskはplaceholder)、queue接続、Core割り付けを「完成形」ではなく「現状」として記述する
- [ ] AGENTS.md §7/§9の「完成形」記述と現状の差分を明記する(RF1.1の設計判断の入力になる)

## RF3 完了条件

- [ ] 実態と矛盾する記述(RF3.1の5点)がゼロ
- [ ] コマンドはリファレンス1箇所を見れば全部わかる
- [ ] PLANS.mdが計画・進捗のみになっている

---

# RF4: リポジトリ衛生

## 目的

生成物・placeholder・依存宣言の整理。

## チェックリスト

- [ ] `tools/qr_tool/20260611_212014.gcode`(タイムスタンプ名のツール実行出力)を削除する
- [ ] `tools/qr_tool/qr_hello.svg`(ツール直下の生成プレビュー)を削除または`examples/`へ移す
- [ ] `.gitignore`へツール出力パターン(タイムスタンプ名`.gcode`等、生成物の置き場所ルール)を追加する。既存の`examples/`配下の意図的なfixture(38ファイル)は維持し、docsから参照されないものだけ棚卸しする
- [ ] `include/README`、`lib/README`、`test/README`(PlatformIO雛形のまま)を削除するか、プロジェクト固有の説明へ書き換える
- [ ] `.github/workflows/`のCI追加(RF0.3と同一作業)

## RF4 完了条件

- [ ] `git ls-files`に用途不明な生成物が無い
- [ ] 新たな生成物コミットが`.gitignore`で予防されている

---

## 4. 優先度まとめ

| 優先 | 項目 | 理由 |
|---|---|---|
| 高 | RF3.1 ドキュメント即時修正 | 安価で、実装との実害ある矛盾を放置しない |
| 高 | RF0 テスト/CI基盤 | 以降の全リファクタの前提 |
| 高 | RF1.2(b) alarm突入の一本化 | 安全経路の4重複はバグ温床 |
| 高 | RF1.1 MotionTask分割 | 最大の保守性ボトルネック |
| 中 | RF2.1 tools/common化 | feed値600/800/1200の不一致は描画品質に影響しうる |
| 中 | RF1.3〜RF1.5 UI/LED/config | 挙動不変で完結、局所的 |
| 中 | RF3.2〜RF3.5 文書分割・一元化 | 中期の保守コスト削減 |
| 低 | RF2.2 server.py/app.js分割 | 効果は大きいがWebUIは変更頻度が高く、機能開発と衝突しやすい |
| 低 | RF4 リポジトリ衛生 | いつでも可、影響小 |

---

## 5. リスク・未解決事項

| ID | 状態 | 内容 | 対応 |
|---|---|---|---|
| RR1 | [ ] | RF1.1のmotion経路分割は、実機でしか出ない脱調・drift・timing問題を再発させるリスクがある | 分割は小さく刻み、各段階でhoming/gcode/job_lifecycle/lookahead check CSVを実機実行する。実機未確認の段階を`[-]`のまま残す |
| RR2 | [ ] | stepperFeedTask/tmcTaskへの実装移動はCore 1内のtask間同期設計が必要で、挙動不変リファクタの範囲を超える | RF1.1では移動せずドキュメント整合(RF3.5)で対応し、移動は`PLANS.md`側の設計課題として起票する |
| RR3 | [ ] | RF2.1のfeed既定値統一は、意図的にツールごとへ調整されていた値を潰す可能性がある | 統一前に各値の由来をPLANS.md変更履歴から確認し、機械条件依存の値はconfig引数として残す |
| RR4 | [ ] | `logMessage`プレフィクスのenum化はSerial ToolやWebUIのexpectパターンを壊しうる | 出力文字列は変えず内部表現のみ変更する。文字列変更が必要な場合はserial_send.py/server.pyのexpect更新とセットで行う |
| RR5 | [ ] | PLANS.md分割はAGENTS.md §15の「PLANS.mdを更新せよ」ルールと外部参照(過去ログ・PR)を壊しうる | 分割時にAGENTS.md §15の更新先を同時改訂し、旧セクション位置にリンクを残す |
| RR6 | [ ] | native test基盤の`Arduino.h`スタブ(21行)はTrapezoidPlanner等へテストを広げると不足する | RF0.1でスタブ拡張方針(必要関数のみ追加 or ArduinoFake等の採用)を決める |

---

## 6. 変更履歴

| 日付 | 変更 | 更新者 |
|---|---|---|
| 2026-07-02 | 初版作成。ファームウェア/ドキュメント/ツールの調査結果(行番号付き)を基にRF0〜RF4を定義 | Codex |
