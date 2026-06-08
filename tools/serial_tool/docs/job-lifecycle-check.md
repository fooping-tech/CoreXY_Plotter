# Job Lifecycle Check

Phase 10.5の`JOB_BEGIN` / `JOB_END` / `JOB_STATUS`を確認する手順です。

## CSV

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --csv tools/serial_tool/examples/job_lifecycle_check.csv \
  --startup-delay 4 \
  --startup-drain 1 \
  --timeout 30 \
  --echo
```

## G-code Job Mode

正式G-code送信では、ホスト側preambleへ起動/終了処理を混ぜず、`--job-lifecycle`で`JOB_BEGIN`と`JOB_END`を前後に送ります。
`JOB_BEGIN`はTMC未readyなら自動で`TMC_INIT`相当を実行します。
`JOB_BEGIN_AUTO_HOME=false`では、事前に`G28`相当のbring-upが済んでいる状態で実行します。
`JOB_BEGIN_AUTO_HOME=true`では、未homed時に`JOB_BEGIN`内でHOME相当を自動実行します。
Serial Toolは`JOB_BEGIN`について、`--timeout`の既定値に関係なく最大60秒まで`JOB_BEGIN OK`を待ちます。

```bash
python tools/serial_tool/serial_send.py \
  --port /dev/cu.usbserial-023591AC \
  --gcode tools/text_tool/examples/gcode/text_robo.gcode \
  --job-lifecycle \
  --startup-delay 4 \
  --startup-drain 1 \
  --queue-mode \
  --stream-gcode-motion \
  --echo
```

## Expected

| Command | Expected log |
|---|---|
| `JOB_BEGIN` | `JOB_BEGIN OK ... pen=UP` |
| `JOB_STATUS` while running | `JOB_STATUS state=RUNNING` |
| G-code `G0/G1` | `ACK_XY target=...` |
| `JOB_END` | `JOB_END park target=(5.000,50.000)`, `JOB_END_JINGLE complete`, `JOB_END OK ... PEN=UP` |
| final `JOB_STATUS` | `JOB_STATUS state=IDLE` |

## Manual Reject Checks

`serial_send.py` treats `REJECT:` as a failure, so these checks are manual.

1. During a running job, send bare `XY 5 5 600`.
2. Confirm `REJECT: command XY not allowed while job_state=RUNNING source=SERIAL`.
3. Outside a job, send `JOB_ABORT`.
4. Confirm `JOB_ABORT rejected reason=no_active_job` and that no low-level stop is triggered.
5. During a running job, send `ABORT`.
6. Confirm the motion stops, alarm is set, homed is invalidated, and `JOB_STATUS` reports `ABORTED`.

## Notes

- `JOB_BEGIN` requires idle motion queues, no alarm, and `HOMED=YES`.
- `JOB_BEGIN` auto-runs TMC initialization when TMC is not ready.
- `JOB_BEGIN_AUTO_HOME=false` rejects unhomed jobs with `not_homed`.
- `JOB_BEGIN_AUTO_HOME=true` auto-runs HOME when unhomed. Use only after limit switch direction and E-stop behavior are verified.
- `JOB_BEGIN` may take longer than normal commands because it can include AUTO_HOME; the serial tool waits up to 60 seconds for `JOB_BEGIN OK`.
- Start-time rejects such as `not_homed` leave the job state retryable; the next `JOB_BEGIN` should not fail with `job_not_idle` unless a job is actually active or alarm recovery is still required.
- `JOB_END` is explicit because firmware does not know the end of a streamed serial G-code file unless the host sends it.
- `JOB_END` raises the pen, parks at `X=5mm, Y=Y_MAX_MM-5mm`, then plays a short original 8-bit-style two-motor chord jingle.
