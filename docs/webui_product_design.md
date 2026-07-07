# Host WebUI Product Design Brief

> 位置づけ: 本書はWebUI設計時のデザイン経緯資料である。
> 画面構成・操作ルール・APIの仕様の正は`SPEC.md` §20とし、差異がある場合はSPECが優先する。

## Scope

The initial WebUI runs on a PC or Raspberry Pi and controls the M5Stack Core2 firmware through USB Serial.
The firmware remains the source of truth for motion safety, alarms, homing, and command validation.

Job sending must reuse `tools/serial_tool/serial_send.py`.
The WebUI must not reimplement queue retry, ACK waiting, Job Lifecycle wrapping, or failure abort behavior.

## Primary Users

- Operator bringing up and testing the CoreXY plotter
- Operator sending generated G-code jobs
- Developer diagnosing firmware, serial, homing, and safety behavior

## Product Principles

- Make machine state visible before controls.
- Keep unsafe actions visually distinct.
- Disable controls when the host does not have enough state to make a safe request.
- Keep logs close to the action that produced them.
- Preview before sending, but never treat preview as a replacement for firmware safety.

## Information Architecture

画面一覧(Dashboard / Manual Control / Job / Console / Settings)と操作ルールは
`SPEC.md` §20.1〜§20.3を正とする。ここではSPECに含めていない設計判断のみ残す。

- Jog step選択肢は 0.1 mm / 1 mm / 5 mm とする
- Consoleにはfirmware出力に加えてhost bridge出力も表示する
- 状態不明(host stateが取得できない)時はmotionを伴う操作をdisabledにする

## G-code Preview Requirements

Supported for MVP:

- `G0` and `G1` XY line segments
- `G20` and `G21`
- `G90` and `G91`
- `M3` and `M5`
- `G28` as a home marker
- `G4` as a dwell marker

Preview rendering:

- Pen-down path and pen-up travel use different colors.
- Soft limit rectangle is always visible.
- File bounds are shown.
- Segments outside soft limits are highlighted and listed as warnings.
- Unsupported G-code lines are listed as warnings.

## Non-goals For MVP

- Running an HTTP server on the ESP32
- Replacing firmware safety checks
- Editing G-code in the browser
- Full GRBL compatibility
- Arc interpolation preview for `G2`/`G3`
- Pause/resume
- Feed override
