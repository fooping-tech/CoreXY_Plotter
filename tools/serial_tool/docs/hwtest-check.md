# HW Test Check (コントローラPCB検査)

コントローラPCBを単体で検査するための専用ファームウェア(`m5stack-core2-hwtest` env)と手順です。
通常のプロッタファームウェアとは別ビルドで、planner/G-code/homingを含みません。
再利用するのはハードウェアドライバ層(`TMC2209Manager`、`StepperBackendFastAccel`、`PenController`、`NeoPixelController`)だけです。

前提: PCBのピン割り付けは`include/Core2PinMap.h`(AGENTS.md §5)と同じ。

## Build / Upload

```bash
pio run -e m5stack-core2-hwtest
pio run -e m5stack-core2-hwtest --target upload
pio device monitor -b 115200
```

検査が終わったら通常ファームウェアを書き戻します。

```bash
pio run -e m5stack-core2 --target upload
```

## 通電前の目視/テスター確認(ファームウェア不要)

| 項目 | 確認内容 |
|---|---|
| VM-GND | 短絡していないこと(抵抗値確認) |
| 5V/3V3-GND | 短絡していないこと |
| TMC2209 EN | GNDに接続されていること |
| TMC2209 A MS1/MS2 | どちらもLOW (address 0) |
| TMC2209 B MS1/MS2 | MS1=HIGH, MS2=LOW (address 1) |
| PDN_UART | GPIO14 TX - 1kΩ - 両ドライバPDN_UART、GPIO13 RXが同じ節点 |
| limit pull-up | GPIO35/36は内部pull-up無し。外付けpull-upがあること |
| モータ | **最初はモータを外して**通電する(下記CSVのMOTOR行は後で実施) |

## 推奨手順

1. VM未接続、USBのみで書き込み → `INFO`、`LIMIT`、`LED_TEST`、`PENUP`/`PENDOWN`
   (TMC2209はVMが無いとUART応答しないので`TMC_PING`はFAILになる)
2. VM接続、モータ未接続 → `SELFTEST`、`TMC_PING`、`TMC_INIT 400`、`TMC_STATUS`
3. 電源を切ってモータ接続、可動部を作業範囲中央へ手で移動 → `MOTOR A 400` / `MOTOR B 400`
4. `LIMIT_WATCH 15`でスイッチを押して離す

## CSV

`tools/serial_tool/examples/hwtest_check.csv`(手順2〜4をまとめたもの。モータ接続済み前提)

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-XXXX \
  --csv tools/serial_tool/examples/hwtest_check.csv \
  --startup-delay 4 \
  --timeout 20 \
  --echo
```

## Commands

| Command | 動作 | モータ動作 |
|---|---|---|
| `HELP` / `INFO` | コマンド一覧 / ピン割り付け | なし |
| `POWER` | AXP192のVBUS/バッテリ電圧(VMは計測不可) | なし |
| `SELFTEST` | limit安定性、TMC A/B UART、STEP/DIR→IOINループバックを自動判定 | 1 microstep程度 |
| `LIMIT` | X/Y limitの生レベルとON/OFF | なし |
| `LIMIT_WATCH [sec]` | 指定秒数limitのedgeを記録。チャタリング(20ms内の連続edge)を`bounce`として数える | なし |
| `PIN_CHECK` | STEP/DIR 4本を1本ずつHIGHにし、両TMCのIOINで該当ビットだけが追従するか確認 | 1 microstep/STEP |
| `PIN_SET <A_STEP\|A_DIR\|B_STEP\|B_DIR> <0\|1>` | 静的レベル出力(テスター計測用) | STEPを0→1で1 microstep |
| `TMC_PING` | UART読み出しのみ(レジスタ書込みなし) | なし |
| `TMC_INIT [mA]` | 16 microstep、指定RMS電流(既定400mA、最大1100mA)で設定 | なし |
| `TMC_STATUS` | IOIN/GSTAT/DRV_STATUSを読み判定 | なし |
| `MOTOR <A\|B> [steps] [steps_s]` | 単独モータ移動(既定400 steps/800 steps/s、最大1600 steps/4000 steps/s)。途中で1回DRV_STATUSを読みopen load/shortを確認 | あり |
| `STOP` | モータ停止、LIMIT_WATCH/SERVO_SWEEP中断 | 停止 |
| `PENUP` / `PENDOWN` / `SERVO <deg>` / `SERVO_SWEEP` | サーボ | サーボ |
| `LED <r> <g> <b>` / `LED_TEST` / `LED_OFF` | NEOPIXEL | なし |

Core2のタッチボタン: 左=SELFTEST、中=PEN UP/DOWN、右=LED_TEST。LCDに各結果を表示します。

## 判定

| 結果 | 期待ログ |
|---|---|
| SELFTEST | `SELFTEST RESULT PASS fails=0` |
| TMC_PING | `TMC_A addr=0 ver=0x21 ... ENN=0 MS1=0 MS2=0`、`TMC_B addr=1 ver=0x21 ... ENN=0 MS1=1 MS2=0`、`TMC_PING PASS` |
| PIN_CHECK | `ALL_LOW`は全0、`A_STEP`行は`A.STEP=1`のみ、`A_DIR`行は`A.DIR=1`のみ…と対角だけ1 |
| TMC_STATUS (init後) | `GSTAT`の`drv_err=0 uv_cp=0`、`DRV_STATUS`の`ot=0 s2ga=0 s2gb=0` |
| MOTOR | `MOTOR A DONE PASS`、`mid-move ... ola=0 olb=0`、モータが回り、逆符号で戻る |
| LIMIT_WATCH | 押下/解放でX/Y各2 edge、`bounce=0`が望ましい |
| LED_TEST | 全灯R→G→B→W、その後白1灯が0番から`NEOPIXEL_LED_COUNT-1`番まで移動 |

## Troubleshooting

| 症状 | 想定原因 |
|---|---|
| `TMC_x ... no valid UART reply` | VM未接続、PDN_UART配線、1kΩ抵抗、GPIO13/14の入替、MS1/MS2アドレス設定ミス |
| A/B両方NO REPLYだがVMはある | TX/RX逆、1kΩの位置違い(RXが抵抗の手前側)、TMCの`PDN_UART`がGND/VIOに固定 |
| AとBの片方だけ応答 | 応答しない側のアドレスピン、または両方同じアドレスで衝突 |
| `MS1/MS2 address=x expected y` | アドレスピンのはんだ/ジャンパ |
| `ENN is HIGH` | ENがGNDに落ちていない(ドライバ出力が無効) |
| PIN_CHECKで別ビットが1 | STEP/DIRやA/Bの配線入替、隣接ネット短絡 |
| PIN_CHECKで期待ビットが0 | GPIO→ドライバ間の断線、M-BUSピン違い |
| `uv_cp=1` | VM電圧不足、チャージポンプ周りのコンデンサ |
| `ola`/`olb`=1 | モータコイル未接続、コネクタ不良(高速時は誤検出あり。800 steps/s程度で判定) |
| `s2ga`/`s2gb`=1 | モータ出力の短絡、配線ミス |
| `LIMIT_x unstable` | 入力が浮いている(外付けpull-up欠落)、ノイズ |
| `LIMIT_x active while idle` | スイッチ押下中、NO/NC取り違え、`LIMIT_ACTIVE_LOW`と回路の不一致 |
| LEDが途中から点かない | その番号のLEDのDIN/DOUT間の断線、`NEOPIXEL_LED_COUNT`設定 |

## Notes

- TMC2209のENはGND固定なので、**VMを入れた瞬間からモータは励磁される**。UART未設定時はVrefとMS1/MS2(=アドレスピン)に従う standalone動作になる。
- STEP/DIRの静的出力(`PIN_CHECK`/`PIN_SET`)は最初の`MOTOR`実行前のみ使える。`MOTOR`後はピンがFastAccelStepperへ移るため、再度使うにはリセットする。
- STEPパルス列はFastAccelStepperだけが生成する。hwtestは静的レベル以外を直接出さない。
- `MOTOR`は単独モータ移動なので、CoreXYでは可動部がXY斜め方向に動く(A: +X+Y、B: +X−Y)。開始時点でOFFだったlimitがONになると停止する。
