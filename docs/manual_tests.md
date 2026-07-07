# 手動テスト手順

旧`PLANS.md` §12から移動した実機/simulationの手動テスト手順。
各checkの実行手順の正は`tools/serial_tool/docs/`配下の個別check文書とし、
本書は横断的な手順とSerial Tool仕様(待ち時間、ABORT、no-op等)を保持する。


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
