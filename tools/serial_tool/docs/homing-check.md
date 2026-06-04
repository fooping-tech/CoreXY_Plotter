# Homing Check

Homing実装の実機確認用チェックです。
`HOME_X`、`HOME_Y`、`HOME`をSerial経由で実行し、limit入力、二段階homing、homed状態を確認します。
同時に、X/Y switchの取り違え、コネクタ未接続、switch極性、limit方向の設置ミスを実機動作として確認できます。

## Safety

- 初回は可動部がlimit方向へ安全に動ける位置から始めてください。
- `HOMING_X_DIR` / `HOMING_Y_DIR` がlimit switch方向と逆の場合、max travelまで進む可能性があります。
- E-stopまたはモータ電源をすぐ切れる状態で実行してください。
- `ALARM_CLEAR`は前回のalarmを消すためにCSVへ入れています。原因が未解決のalarmを無視して続行しないでください。

## CSV

`tools/serial_tool/examples/homing_check.csv`

limit入力だけを先に切り分ける場合は、[Limit Check](limit-check.md)を実行してください。
`homing_check.csv`は実際にモータを動かすため、switch設置位置とlimit方向の最終確認として使います。

## Run

Homingはlimit到達まで時間がかかるため、通常の短い確認より`--timeout`を長めにします。

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/homing_check.csv \
  --startup-delay 4 \
  --timeout 95 \
  --echo
```

送信予定だけを確認する場合:

```bash
python tools/serial_tool/serial_send.py \
  --csv tools/serial_tool/examples/homing_check.csv \
  --dry-run
```

## Expected Logs

| Step | What to confirm |
|---|---|
| `CONFIG` | `SIMULATION_MODE=0`、`HOMING enabled=1`、`x_dir`、`y_dir`、seek/slow/backoff/max travelが意図通り |
| `LIMIT_STATUS` | 手でlimitを押した時のraw/debouncedが意図通り。通常は開始前に対象limitがOFF。詳しくは[Limit Check](limit-check.md)を先に実行する |
| `TMC_INIT` | `ready=YES`。ここで失敗する場合はTMC UART、address、motor driver電源を先に確認する |
| `TMC_STATUS` | `TMC_STATUS ready=YES`、A/B driverが通常profileになっている |
| `HOME_X` | `HOME_X started`後、fast seek、backoff、slow seekを経て`HOME_X set zero`が出る |
| `HOME_STATUS` after `HOME_X` | `x_homed=YES` |
| `HOME_Y` | `HOME_Y started`後、fast seek、backoff、slow seekを経て`HOME_Y set zero`が出る |
| `HOME_STATUS` after `HOME_Y` | `y_homed=YES`、両軸完了後は`homed=YES` |
| `HOME` | X/Yを順番にhomingし、最後に`HOME complete`が出る |
| final `POS` | `HOMED=YES`、`X_HOMED=YES`、`Y_HOMED=YES`、原点座標、limit状態が意図通り |

## Installation Checks

| Symptom | What to check |
|---|---|
| `HOME_X`中に`homing target-other limit active`で止まる | X/Y switchの取り違え、X switchがGPIO35側へ接続されていないか、Y入力が浮いていないか |
| X方向へ動いているのにX switchがONにならない | X switchのコネクタ未接続、GPIO36への接続、switch位置、`HOMING_X_DIR` |
| `HOME_Y`中にX limit側がONになる | X/Y switchの取り違え、Y switchがGPIO36側へ接続されていないか |
| 起動直後から片側だけ常にON | switch極性、外付けpull-up/pull-down、コネクタの短絡 |
| limitに当たっても止まらずmax travel alarmになる | switch位置、switch配線、`LIMIT_ACTIVE_LOW`、対象軸のhoming方向 |

## Related Config

- `HOMING_ENABLED`
- `HOMING_X_DIR` / `HOMING_Y_DIR`
- `HOMING_SEEK_FEED_MM_MIN`
- `HOMING_SLOW_FEED_MM_MIN`
- `HOMING_BACKOFF_MM`
- `HOMING_MAX_TRAVEL_X_MM` / `HOMING_MAX_TRAVEL_Y_MM`
- `HOMING_SET_X_MM` / `HOMING_SET_Y_MM`
- `HOMING_LIMIT_DEBOUNCE_MS`
- `HOMING_REQUIRE_HOMED_FOR_XY_MOVE`
- `LIMIT_ACTIVE_LOW`

## Post-Homing Motion Check

Homing完了後、limitから離れる方向に安全な余裕があることを確認してから、必要に応じて別途以下を手動送信してください。

```text
XY 10 0 300
XY 10 10 300
```

この確認で通常XY移動が拒否される場合は、`POS`と`LIMIT_STATUS`でhomed状態、現在座標、limit入力を確認してください。

## Notes

- CSV側ではCoreXY変換、soft limit判定、homing状態遷移を実装しません。
- `expect`はSerialログの確認用です。limit switchの機械的な取り付け方向やE-stop確認の代替にはなりません。
- `CONFIG`で`SIMULATION_MODE=1`の場合、Serialログだけ出てSTEP/DIRは出力されません。実機homing前に`SIMULATION_MODE=0`でbuild/uploadしてください。
- `TMC_INIT`または`TMC_STATUS`で`ready=NO`の場合、TMC2209が設定できていないためモータが保持・回転しない可能性があります。
- 原点角などでX/Y limitが両方ONの状態から開始した場合、対象外limitは「開始時点からON」として許容し、対象軸をbackoffしてから低速seekします。対象外limitが開始後に新しくONになった場合はalarmで止めます。
- X homing中に`homing target-other limit active`で止まる場合、firmwareはXではなくY limitがONになったと判断しています。X/Y limit配線の入れ替わり、`X_LIMIT_PIN`/`Y_LIMIT_PIN`の接続先、Y入力の浮きを確認してください。
