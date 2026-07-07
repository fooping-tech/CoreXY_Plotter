# リスク・未解決事項

旧`PLANS.md` §14から移動。機能実装に関するリスク管理表。
リファクタリングのリスクは`PLANS_REFACTORING.md` §5にある。


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
