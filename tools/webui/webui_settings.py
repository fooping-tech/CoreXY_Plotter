"""WebUI送信設定の正規化とserial_send.py CLI引数の構築。

HTTP層(server.py)から分離した純ロジック。値検証はここで行い、
送信の実挙動はtools/serial_tool/serial_send.pyの責務のまま維持する。
"""

from __future__ import annotations

from pathlib import Path

DEFAULT_SEND_SETTINGS: dict[str, object] = {
    "commandTimeoutS": 5.0,
    "jobTimeoutS": 30.0,
    "motionTimeoutMarginS": 5.0,
    "autoMotionTimeout": True,
    "streamGcodeMotion": True,
    "jobLifecycle": True,
    "queueRetryDelayMs": 250,
    "queueRetryTimeoutS": 10.0,
}


def clamp_float(value: object, *, name: str, minimum: float, maximum: float) -> float:
    try:
        parsed = float(value)
    except (TypeError, ValueError) as exc:
        raise ValueError(f"{name} must be a number") from exc
    if not minimum <= parsed <= maximum:
        raise ValueError(f"{name} must be between {minimum:g} and {maximum:g}")
    return parsed


def clamp_int(value: object, *, name: str, minimum: int, maximum: int) -> int:
    try:
        parsed = int(value)
    except (TypeError, ValueError) as exc:
        raise ValueError(f"{name} must be an integer") from exc
    if not minimum <= parsed <= maximum:
        raise ValueError(f"{name} must be between {minimum} and {maximum}")
    return parsed


def bool_setting(value: object, *, name: str) -> bool:
    if isinstance(value, bool):
        return value
    if isinstance(value, str):
        normalized = value.strip().lower()
        if normalized in {"1", "true", "yes", "on"}:
            return True
        if normalized in {"0", "false", "no", "off"}:
            return False
    raise ValueError(f"{name} must be true or false")


def normalize_send_settings(raw: dict[str, object]) -> dict[str, object]:
    settings = dict(DEFAULT_SEND_SETTINGS)
    settings.update(raw)
    return {
        "commandTimeoutS": clamp_float(settings["commandTimeoutS"], name="commandTimeoutS", minimum=0.5, maximum=600.0),
        "jobTimeoutS": clamp_float(settings["jobTimeoutS"], name="jobTimeoutS", minimum=1.0, maximum=3600.0),
        "motionTimeoutMarginS": clamp_float(
            settings["motionTimeoutMarginS"],
            name="motionTimeoutMarginS",
            minimum=0.0,
            maximum=600.0,
        ),
        "autoMotionTimeout": bool_setting(settings["autoMotionTimeout"], name="autoMotionTimeout"),
        "streamGcodeMotion": bool_setting(settings["streamGcodeMotion"], name="streamGcodeMotion"),
        "jobLifecycle": bool_setting(settings["jobLifecycle"], name="jobLifecycle"),
        "queueRetryDelayMs": clamp_int(settings["queueRetryDelayMs"], name="queueRetryDelayMs", minimum=0, maximum=60000),
        "queueRetryTimeoutS": clamp_float(
            settings["queueRetryTimeoutS"],
            name="queueRetryTimeoutS",
            minimum=0.5,
            maximum=600.0,
        ),
    }


def command_args(port: str, baud: int, csv_path: Path, settings: dict[str, object]) -> list[str]:
    return [
        "--csv",
        str(csv_path),
        "--port",
        port,
        "--baud",
        str(baud),
        "--startup-delay",
        "0",
        "--startup-drain",
        "0.1",
        "--timeout",
        str(settings["commandTimeoutS"]),
        "--queue-mode",
        "--echo",
    ]


def job_args(port: str, baud: int, gcode_path: Path, settings: dict[str, object]) -> list[str]:
    args = [
        "--gcode",
        str(gcode_path),
        "--port",
        port,
        "--baud",
        str(baud),
        "--queue-mode",
        "--queue-retry-delay-ms",
        str(settings["queueRetryDelayMs"]),
        "--queue-retry-timeout",
        str(settings["queueRetryTimeoutS"]),
        "--timeout",
        str(settings["jobTimeoutS"]),
        "--motion-timeout-margin",
        str(settings["motionTimeoutMarginS"]),
        "--startup-delay",
        "0",
        "--startup-drain",
        "0.2",
        "--echo",
    ]
    if settings["streamGcodeMotion"]:
        args.append("--stream-gcode-motion")
    if settings["jobLifecycle"]:
        args.append("--job-lifecycle")
    if not settings["autoMotionTimeout"]:
        args.append("--no-auto-motion-timeout")
    return args
