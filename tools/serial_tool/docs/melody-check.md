# Melody Check

TMC UART、モータA、診断用メロディprofileの切り替えを確認するチェックです。
`MELODY`は診断専用で、通常motionがidleでTMC UARTがreadyの場合だけ実行します。

## CSV

`tools/serial_tool/examples/melody_check.csv`

## Run

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/melody_check.csv \
  --startup-delay 4 \
  --timeout 5 \
  --echo
```

## Checks

| Step | What to confirm |
|---|---|
| `CONFIG` | `MOTOR_EN=HARDWIRED_GND`を理解してから実行する |
| `POS` | `ALARM=NO`、limitがACTIVEでない |
| `TMC_INIT` | TMC UARTが初期化される |
| `TMC_STATUS` | `ready=YES` |
| `MELODY` | Aモータから短い音階が鳴り、`MELODY complete; normal TMC profile restored`が出る |

## Related Config

- `MOTOR_MELODY_ENABLED`
- `MOTOR_MELODY_MICROSTEPS`
- `MOTOR_MELODY_RMS_CURRENT_MA`
- `MOTOR_MELODY_SPREADCYCLE`
- `MOTOR_MELODY_NOTE_GAP_MS`

## Stop Conditions

- 異音、発熱、limit反応、想定外の動きがあれば停止してください。
- `ERROR: MELODY TMC UART is not ready` が出た場合は、`TMC_INIT`、PDN_UART配線、A/Bアドレスを確認してください。
