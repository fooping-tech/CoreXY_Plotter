# PLANS_REFACTORING.md

# M5Stack Core2 CoreXYペンプロッタ リファクタリング計画・進捗管理

Version: 0.2
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
現在地: RF0〜RF4 完了(2026-07-08)。残件は実機回帰確認(RR1)と、
保留2件(app.js分割、serial_send.py分割 — RF2.2参照)のみ。
```

現在の最優先作業:

```text
実機接続時にhoming/gcode/job_lifecycle/lookahead check CSVで回帰確認し、
RF1の[-]項目を[x]へ更新する。
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
| RF0 | 安全網(テスト/CI基盤) | リファクタ前に回帰検出手段を整える | なし | [x] |
| RF1 | ファームウェア構造 | MotionTask分割、重複排除、config集約 | RF0 | [-] 実機回帰のみ残 |
| RF2 | ホストツール | 共通モジュール化、テスト統一、依存整理 | RF0 | [-] app.js/serial_send分割は保留 |
| RF3 | ドキュメント | 陳腐化修正、PLANS.md分割、リファレンス一元化 | なし(即時可) | [x] |
| RF4 | リポジトリ衛生 | 生成物削除、.gitignore、placeholder整理 | なし(即時可) | [x] |

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

- [x] `platformio.ini`へ`[env:native]`(platform=native)を追加し、`pio test -e native`で既存2テストが走る
- [x] `test/native/Arduino.h`スタブを`pio test`構成と両立させる(Unityへ移行済み。スタブは`-I test/native`で併用)
- [x] `tools/run_native_motion_tests.sh`の`/tmp`固定を廃止し、`pio test`へ委譲またはポータブル化する
- [x] RF1.1で抽出予定のモジュール(drift追跡、XYブロック生成、no-op判定)を先にテスト可能な範囲でカバーする(test_planners 9項目を先行追加、抽出後にtest_motion_sync 5項目を追加)

### RF0.2 hostテストの統一

- [x] `tools/webui/test_image_to_svg.py`と`test_svg_to_gcode.py`を`tests/`配下へ移動し、pytest形式へ統一する
- [x] `conftest.py`または`pyproject.toml`で`sys.path.insert`の重複を整理する(tests/conftest.pyへ一括化)
- [x] `pytest`一発で全hostテストが走ることを確認する(54件)

### RF0.3 CIと手順の明文化

- [x] GitHub Actions workflowを追加する(`pio run`両env、`pio test -e native`、`pytest`)
- [x] READMEへテスト実行方法(`pio test -e native`、`pytest`)を記載する
- [x] AGENTS.md §16へnativeテストとpytestを検証手順として追記する

## RF0 完了条件

- [x] Windows/Linuxの両方で全テストが1コマンドずつで実行できる(Windowsで確認済み。LinuxはCIで実行)
- [x] CIがpush時にbuild+testを実行する(.github/workflows/ci.yml)

---

# RF1: ファームウェア プログラムリファクタリング

## 目的

`MotionTask.cpp`の肥大化解消、重複コードの排除、設定値のconfig集約、タスク役割と実装の整合。

## RF1.1 MotionTask.cpp分割(最重要)

`src/tasks/MotionTask.cpp`は1075行で、無名namespace内に約40個の関数と6個のファイルstaticなplannerオブジェクトを持ち、`motionTask()`本体は220行のコマンドswitchになっている(2026-07-02時点)。

分割結果(2026-07-08、MotionTask.cpp 1075行→400行):

| 責務 | 抽出先(実績) |
|---|---|
| pipelineシングルトン群 | `XYMotionPlanner`のメンバ所有へ(function-local static経由) |
| drift追跡・backend位置推定 | `MotionSyncTracker`(純ロジック、nativeテストあり) |
| abort/alarm停止シーケンス | `enterAlarm()`共通関数(RF1.2b) |
| timed segment実行エンジン | `TimedSegmentExecutor` |
| Job lifecycle接続 | `JobLifecycleHandler`(JobController近傍のグルー) |
| G-code変換グルー | `GcodeCommandTranslator` |
| XYブロック生成・計画・実行 | `XYMotionPlanner` |
| AB_TIMED診断経路+TEST_A/B | `MotionDiagnostics` |
| コマンドdispatch switch | テーブル駆動dispatch(`kCommandHandlers`) |

- [x] `MotionSyncTracker`を抽出し、nativeテストを追加する
- [x] `TimedSegmentExecutor`を抽出する
- [x] `XYMotionPlanner`(buildXYBlock / planQueuedBlocks / executePlannedBlock / handleXYBatch)を抽出する
- [x] Job lifecycle接続処理を`JobController`側へ寄せる(`JobLifecycleHandler`として分離)
- [x] `motionTask()`のswitchをテーブル駆動またはハンドラ関数群へ整理する
- [x] 分割後もplanner系がファイルstaticグローバルでなく所有関係で持たれている
- [-] 分割の各段階で`pio run`両envが通り、homing/gcode/job_lifecycle check CSVで回帰確認する
  (`pio run`両env+native 16テストは各段階で通過。**check CSVの実機回帰は未実行**: 開発機に実機未接続のため。RR1参照)

補足(設計判断): `stepperFeedTask`/`tmcTask`のplaceholder実態は、実装移動ではなくドキュメント整合で対応した(RF3.5 `docs/architecture.md`に現状と完成形の差分を明記。移動はRR2の通り設計課題として保留)。

## RF1.2 重複コードの排除

- [x] (a) `StatusMessage`手組みスナップショットの一本化 → `captureStatus()`(main.cpp)
- [x] (b) alarm突入シーケンスの一本化(4箇所) → `enterAlarm(reason, LedStatus)`
- [x] (c) `job_active`同期の一本化 → `syncJobActiveFlag()`。`isActive()`はRUNNINGを含むため`|| isRunning()`の冗長条件を除去(挙動不変)
- [x] (d) LED `SET_STATUS`送信の一本化 → `postLedStatus()`(drop時WARNポリシーで統一)
- [x] (e) `HOME`/`HOME_X`/`HOME_Y`の3重複 → `handleHomeCommand(name, メンバ関数ポインタ)`
- [x] (f) AB_TIMEDのbackend queue状態ログ5重複 → `logAbTimedState()`(現MotionDiagnostics内)
- [x] (g) `ACK_XY`/`NACK_XY`ログのフォーマット重複 → `ackXY()`/`nackXY()`(現XYMotionPlanner内)
- [x] (h) `square()`の2重定義とclampイディオム → `include/MathUtils.h`(square/clampFloat)。UiTaskの自前clampFloatも置換

## RF1.3 UiTaskの整理

- [x] 描画rectとタッチhit-test rectの二重リテラルを単一の`Rect`レイアウトテーブル(`CONTROL_*_RECT`)へ集約
- [x] 色定数、画面寸法(320/214)、notice timeout(1800ms)等のマジックナンバーを整理する(命名定数化)
- [x] jogのsoft limit clamp・base追跡の境界をコメントで明確化(UI側は先行チェック、最終判定はfirmware motion経路)
- [x] `UI_JOG_FEED_MM_MIN`/`UI_JOG_STEP_MM`を`PlotterConfig.h`へ移す
- [x] uiTaskがLED lifecycleも駆動している構造をSPEC.md §7と`docs/architecture.md`に明文化(task分割はCore 0負荷再評価が必要なため未実施)

## RF1.4 LedPatternEngineの分離

- [x] renderer群を純関数の`LedRenderer`へ分離する
- [x] 状態→演出テーブルのハードコード値を`include/LedStatusConfig.h`のconstexprテーブルへ移し、transient 2500msとIDLE輝度上限を`PlotterConfig.h`のNeoPixelセクションへ
- [x] engine内からの`logMessage`直接呼び出しを`setResponder()`コールバックへ置き換え、engineをグローバルI/O非依存にする(応答文字列は不変)

## RF1.5 config集約の徹底

- [x] `Diagnostics.cpp`のSELFTEST期待値(800/1600)を`STEPS_PER_MM`から導出する
- [x] `SafetyTask.cpp`の100msポーリング、`CommandTask.cpp`の10ms/50msを`TaskConfig.h`へ
- [x] RF1.3/RF1.4のUI・LED定数移動と合わせ、「調整可能定数はconfigへ集約」の原則へ再整合する

## RF1.6 その他の構造改善

- [x] `CoreXYKinematics`へ逆変換API(`abStepsToXYDeltaMm`)を追加し、インライン逆変換を置き換え
- [x] `StatusMessage`/`LogMessage`を`MachineState.h`から`Messages.h`へ移す
- [x] `logMessage`の応答プレフィクス文字列を`Messages.h`でマクロ定数化し、ホスト側expectパターンとの互換要件を明文化(enum化はSerialプロトコル互換維持のため見送り。出力文字列は不変)
- [x] `logMessage`のqueue full時silent dropを廃止: drop計数+logTaskによる`WARN: LogQueue dropped N messages`報告
- [x] RF1.1で抽出した新モジュールは参照渡し+フック注入で受け取り、グローバル直接参照を増やしていない(`AppContext.h`のhub構造自体は維持)

## RF1 完了条件

- [x] `MotionTask.cpp`が400行以下になり(400行)、抽出モジュールにnativeテストがある(MotionSyncTracker)
- [x] RF1.2の重複8項目が解消されている
- [-] `pio run`両env、`pio test -e native`は通過。**実機`SELFTEST`+主要check CSVは未実行**(実機未接続。RR1)

---

# RF2: ホストツール リファクタリング

## 目的

Pythonツール間の重複排除と、巨大単一ファイルの分割。

## RF2.1 共通モジュール`tools/common/`の新設

- [x] G-code出力ロジックの3重複を統合する → `tools/common/plotter_gcode.py`の`GcodeEmitter`(座標/feed書式はツール別注入で既存出力を維持)
- [x] `gcode_words()`トークナイザの2重複を統合する(正規表現はserial_send側の上位互換)
- [x] feed既定値の不一致を解消する: 共通configへ集約し、ファームウェア側`PlotterConfig.h`との関係と意図的な差(QR 600=ハッチ品質優先、SVG 800、TEXT 3000、推定用1200)をコメントで明記。値自体は用途依存のため統一しない(RR3の通り)
- [x] 共通化後、既存の生成G-codeサンプルと出力が一致することを回帰確認する(QR/SVGでバイト一致を確認)

注意: `server.py`のsubprocess再利用構造とpyserial依存の閉じ込めは維持した。

## RF2.2 大型ファイルの分割

- [x] `server.py`(1182行)を分割: `gcode_processing.py`(変換パイプライン)+`webui_settings.py`(設定/CLI引数構築)+server.py(HTTP層+subprocess管理、772行)。分割単位のテスト12件を追加
- [-] `app.js`(2163行)の分割は**保留**: ブラウザでの動作確認手段が無い環境での無検証分割はリスクが利益を上回る。着手時はWebUIを起動して画面別に回帰確認しながら行うこと
- [-] `serial_send.py`(1257行)の分割は**保留**: 計画の通り実機check CSV回帰とセットが前提であり、実機未接続のため。tokenizer/feed定数のcommon移行までは実施済み

## RF2.3 依存関係の整理

- [x] root `requirements.txt`へ統合(pyserial/qrcode/Pillow/pytest)。各ツールのrequirements.txtは`-r ../../requirements.txt`参照へ
- [x] バージョンピンの方針を統一する(メジャーバージョン上限つき範囲指定)
- [x] `tools/webui/README.md`(および qr_tool/serial_tool README)のvenv手順を統合後の手順へ更新する

## RF2 完了条件

- [x] G-code生成・トークナイザ・feed定数の重複がゼロ
- [x] 全hostテストが`pytest`一発で通る(54件)
- [-] WebUIからのQR/テキスト/画像→G-code→送信の一連の手動確認は未実施(実機未接続。変換系はユニットテストで担保)

---

# RF3: ドキュメント リファクタリング

## 目的

実態と矛盾する記述の解消、PLANS.mdの分割、参照の一元化。

## RF3.1 即時修正(コード変更不要、先行可)

- [x] `README.md`末尾の重複タイトル`# CoreXY_Plotter` ×2行を削除する
- [x] `AGENTS.md` §13の陳腐化を解消する(実態へ更新し、一覧は`docs/command_reference.md`参照へ)
- [x] `SPEC.md` §15コマンド表へ未記載のLEDコマンド6個を追加する
- [x] `G4`の記載不整合を解消する(AGENTS §13の更新に含む)
- [x] `SPEC.md` §2「非目的」をESP32内WebUIに限定し、Host WebUI実装済み(§20)を明記

## RF3.2 PLANS.mdの分割(2340行 → 1659行)

- [x] §12 手動テスト手順 → `docs/manual_tests.md`。各checkの実行手順の正は`tools/serial_tool/docs/`と明記(重複本文の完全解消は今後の棚卸しで実施)
- [x] §13 Codex用プロンプト → `docs/codex_prompts.md`(アーカイブ)
- [x] §14 リスク・未解決事項 → `docs/risks.md`
- [x] §15 変更履歴 → `CHANGELOG.md`
- [x] 分割後のPLANS.mdはPhase計画+チェックリスト+マイルストーンに限定。旧セクション位置にリンクを残し、AGENTS.md §15の更新先も同時改訂(RR5)

## RF3.3 コマンドリファレンスの一元化

- [x] `docs/command_reference.md`を新設し、全Serialコマンド+対応G-codeを引数・応答・前提条件付きで一覧化する
- [x] SPEC/README/AGENTSのコマンド記述は要約+リンクへ置き換える(SPEC §15は応答ルール仕様として保持し、完全一覧はリファレンスが正)
- [x] `CommandDispatcher.cpp`のコマンド一覧と突き合わせて欠落ゼロを確認する

## RF3.4 WebUI文書の統合

- [x] SPEC §20を仕様の正とし、`docs/webui_product_design.md`/`docs/webui_wireframe.md`は経緯資料として位置づけを明記
- [x] 画面一覧の三重記載を解消(product_designはSPECに無い設計判断のみ残し、PLANS §11.1.2は進捗記録と注記)

## RF3.5 as-builtアーキテクチャ文書

- [x] `docs/architecture.md`を新設し、現状のモジュール構成・タスク実態・queue接続・Core割り付けを「現状」として記述する
- [x] AGENTS.md §7/§9の「完成形」記述と現状の差分を明記する(5点)

## RF3 完了条件

- [x] 実態と矛盾する記述(RF3.1の5点)がゼロ
- [x] コマンドはリファレンス1箇所を見れば全部わかる
- [x] PLANS.mdが計画・進捗のみになっている

---

# RF4: リポジトリ衛生

## 目的

生成物・placeholder・依存宣言の整理。

## チェックリスト

- [x] `tools/qr_tool/20260611_212014.gcode`を削除する
- [x] `tools/qr_tool/qr_hello.svg`を`examples/`へ移す(README参照も更新)
- [x] `.gitignore`へツール出力パターンを追加する(ツール直下の.gcode/.svg/.csv、webui uploads/output、__pycache__)
- [x] `include/README`、`lib/README`、`test/README`をプロジェクト固有の説明へ書き換える
- [x] `.github/workflows/`のCI追加(RF0.3と同一作業)

## RF4 完了条件

- [x] `git ls-files`に用途不明な生成物が無い
- [x] 新たな生成物コミットが`.gitignore`で予防されている

---

## 4. 優先度まとめ

(完了済み。履歴として保持)

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
| RR1 | [-] | RF1.1のmotion経路分割は、実機でしか出ない脱調・drift・timing問題を再発させるリスクがある | 分割は小さく刻み、各段階で`pio run`両env+native 16テストを通過済み。**homing/gcode/job_lifecycle/lookahead check CSVの実機実行は未実施**(開発機に実機未接続)。実機接続時に最優先で実行し、RF1の`[-]`を更新する |
| RR2 | [x] | stepperFeedTask/tmcTaskへの実装移動はCore 1内のtask間同期設計が必要 | RF1.1では移動せず、`docs/architecture.md`で現状と完成形の差分を明文化した。移動は`PLANS.md`側の設計課題 |
| RR3 | [x] | RF2.1のfeed既定値統一は、意図的にツールごとへ調整されていた値を潰す可能性 | 値は統一せず`tools/common/plotter_gcode.py`へ集約し、各値の用途依存の理由をコメントで明記した |
| RR4 | [x] | `logMessage`プレフィクスのenum化はSerial Tool/WebUIのexpectパターンを壊しうる | enum化は見送り、マクロ定数化+互換要件の明文化に留めた。出力文字列は不変 |
| RR5 | [x] | PLANS.md分割はAGENTS.md §15ルールと外部参照を壊しうる | AGENTS.md §15の更新先を同時改訂し、旧セクション位置にリンクを残した |
| RR6 | [x] | native test基盤の`Arduino.h`スタブはテスト拡大で不足しうる | RF0.1でUnityへ移行し、スタブは必要関数のみの現構成を維持(TrapezoidPlanner/JunctionPlannerテストは現スタブで動作) |
| RR7 | [ ] | app.js(2163行)は依然テスト・分割なし | 分割着手時はWebUIをブラウザで起動し画面別に回帰確認する。node等のJS実行環境があればユニットテスト導入を先行する |

---

## 6. 変更履歴

| 日付 | 変更 | 更新者 |
|---|---|---|
| 2026-07-02 | 初版作成。ファームウェア/ドキュメント/ツールの調査結果(行番号付き)を基にRF0〜RF4を定義 | Codex |
| 2026-07-08 | RF0〜RF4を実施し完了状態へ更新。RF0(pio test -e native統合+Unity移行+planner/sync計16テスト、pytest 54件統一、GitHub Actions CI)。RF1(MotionTask 1075→400行: MotionSyncTracker/TimedSegmentExecutor/XYMotionPlanner/JobLifecycleHandler/MotionDiagnostics/GcodeCommandTranslator抽出+テーブル駆動dispatch、重複8項目排除、UiTaskレイアウトテーブル、LedPatternEngine3責務分離、config集約、Messages.h/ログdropポリシー)。RF2(tools/common新設で出力バイト一致を回帰確認、server.py 1182→772行分割+テスト12件、requirements統合。app.js/serial_send.py分割は保留)。RF3(即時修正5点、command_reference.md新設、PLANS.md 2340→1659行分割、WebUI文書統合、architecture.md新設)。RF4(生成物削除、.gitignore、README雛形書き換え)。実機回帰(RR1)のみ未実施 | Claude |
