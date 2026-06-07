# CoreXY Plotter Firmware

M5Stack Core2でCoreXYペンプロッタを制御するためのPlatformIOファームウェアです。
初期版は、安全なbring-upと将来のplanner拡張に必要な責務分離を優先しています。

## Build

```bash
pio run
```

## Simulation mode

`include/PlotterConfig.h`の`SIMULATION_MODE`は初期確認時に`1`へ切り替えて使えます。この状態では
`XY`、`TEST_A`、`TEST_B`コマンドは予定動作をログ表示しますが、モータ出力APIを
呼びません。実機確認前に`SELFTEST`とsimulation用コマンドを実行してください。

## Safety

- 初回起動はモータ電源を切った状態で行ってください。
- GPIO35/36のリミット入力には外付けpull-upが必要です。
- サーボ電源はCore2からではなく外部5V電源を推奨します。
- TMC2209 UART TX側には1kΩ直列抵抗を入れてください。
- GPIO33の外付けNEOPIXELは初期状態で消灯し、輝度上限を低く設定しています。
- TMC2209 A/BのENはGND固定で常時activeです。電気的に停止するには外部スイッチ
  またはE-stop回路が必要です。

## Bring-up commands

```text
LED 255 0 0
LED_PIXEL 0 0 255 0
LED_PATTERN PACIFICA
LED_PATTERN FIRE
LED_BRIGHTNESS 24
LED_PARAM SPEED 160
LED_STATUS
LED_OFF
TMC_INIT
MELODY
G28
G21
G90
G1 X10 Y10 F600
G91
G1 X5 Y0 F600
M3
M5
M114
```

`MELODY`は診断専用です。起動時には自動再生せず、通常motionがidleでTMC UARTが
readyの場合だけ明示実行します。終了または中断時には通常TMC profileへ戻します。

## Known limitations

- TMC2209のMS1/MS2またはジャンパで、A/Bアドレスをそれぞれ`0`と`1`に設定する必要があります。
- `TMC_STATUS`の診断値とメロディ用1200mA profileは実機条件に合わせた確認が必要です。
- `TEST_A`/`TEST_B`の独立A/B moveはbring-up用で、厳密なXY線形補間を保証しません。
- G-codeは最小対応です。`G0/G1/G20/G21/G28/G90/G91/M3/M5/M114`のみ対応し、arc、Z、checksum検証、完全なGRBL互換は未実装です。
- look-aheadとjunction deviationは実装済みですが、実機調整は未完了です。
# CoreXY_Plotter
# CoreXY_Plotter
