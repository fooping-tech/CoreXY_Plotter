# CoreXY Plotter Firmware

M5Stack Core2でCoreXYペンプロッタを制御するためのPlatformIOファームウェアです。
初期版は、安全なbring-upと将来のplanner拡張に必要な責務分離を優先しています。

## Build

```bash
pio run
```

## Test

```bash
# nativeテスト(CoreXY kinematics / planner / segment、ホストPC上で実行)
pio test -e native

# hostツールのテスト(Python)
python -m pip install -r requirements.txt
pytest
```

WindowsでnativeテストにはGCC(MinGW-w64等)がPATHに必要です。
CI(GitHub Actions)はpush/PR時に`pio run`両env、`pio test -e native`、`pytest`を実行します。

## Host WebUI

PCやRaspberry PiからUSB Serial経由で状態確認、手動操作、G-code送信を行う
Host WebUIがあります。

起動方法、`.venv`の作り方、シリアルポート設定は
[tools/webui/README.md](tools/webui/README.md)を参照してください。

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

全Serialコマンド・G-codeの一覧は[docs/command_reference.md](docs/command_reference.md)を参照してください。

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

## Text to G-code

日本語文字列を描画する場合は、ホスト側CLIの`tools/text_tool/kst32b_to_gcode.py`で
KST32BストロークフォントデータからG-codeを生成します。InkscapeやHershey Textには
依存しません。詳細は[tools/text_tool/README.md](tools/text_tool/README.md)を参照してください。

## Known limitations

- TMC2209のMS1/MS2またはジャンパで、A/Bアドレスをそれぞれ`0`と`1`に設定する必要があります。
- `TMC_STATUS`の診断値とメロディ用1200mA profileは実機条件に合わせた確認が必要です。
- `TEST_A`/`TEST_B`の独立A/B moveはbring-up用で、厳密なXY線形補間を保証しません。
- G-codeは最小対応です。`G0/G1/G4/G20/G21/G28/G90/G91/M3/M5/M114`のみ対応し、arc、Z、checksum検証、完全なGRBL互換は未実装です。`G4`は`P`ミリ秒指定のみ対応します。
- look-aheadとjunction deviationは実装済みですが、実機調整は未完了です。
