# 変更履歴

旧`PLANS.md` §15から移動。機能実装の変更履歴。


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
