"""ホストツール共通のプロッタG-codeユーティリティ。

G-code生成(G21/G90/M5プリアンブル+M3/M5/G0/G1)、G-code語のトークナイズ、
feed既定値をここへ集約する。各ツールでの重複実装を禁止する。
"""

from __future__ import annotations

import re
from typing import Callable

# ============================================================================
# feed既定値 [mm/min]
# ============================================================================
# ファームウェア側との関係:
# - firmware include/PlotterConfig.h の MAX_FEED_MM_MIN は
#   30000steps/s / (80steps/mm * sqrt2) * 60 ≈ 15910 mm/min。
#   ここの既定値はいずれもその範囲内にある。
# - feedを省略した XY / G0 / G1 はfirmware側 DEFAULT_FEED_MM_MIN を使う。
# ツールごとの差は意図的な調整値(機械条件依存)であり、統一しない:
# - QRは高密度ハッチングのにじみ・読み取り品質優先で遅め。
# - SVG/画像トレースは一般線画向け。
# - テキスト(KST32Bストローク)は単純な直線ストロークのため速め。
QR_DRAW_FEED_MM_MIN = 600.0
QR_TRAVEL_FEED_MM_MIN = 1800.0
SVG_DRAW_FEED_MM_MIN = 800.0
SVG_TRAVEL_FEED_MM_MIN = 1200.0
TEXT_DRAW_FEED_MM_MIN = 3000.0
TEXT_TRAVEL_FEED_MM_MIN = 8000.0
# serial_toolがfeed未指定motionのホスト側timeout推定に使う仮feed。
# 実際の速度はfirmware側 DEFAULT_FEED_MM_MIN が決める。
HOST_ESTIMATE_FEED_MM_MIN = 1200.0

# ============================================================================
# G-code語トークナイザ
# ============================================================================
GCODE_WORD_RE = re.compile(r"([A-Za-z])\s*([-+]?(?:\d+(?:\.\d*)?|\.\d+))")


def strip_gcode_comments(line: str) -> str:
    """`;`以降と`(...)`コメントを取り除く。"""
    no_semicolon = line.split(";", maxsplit=1)[0]
    return re.sub(r"\([^)]*\)", "", no_semicolon)


def gcode_words(line: str) -> dict[str, float]:
    """1行のG-codeを`{文字: 数値}`へ分解する。コメントは無視する。"""
    return {
        match.group(1).upper(): float(match.group(2))
        for match in GCODE_WORD_RE.finditer(strip_gcode_comments(line))
    }


# ============================================================================
# G-code emitter
# ============================================================================
def _default_fmt(value: float) -> str:
    text = f"{value:.3f}".rstrip("0").rstrip(".")
    return text if text and text != "-0" else "0"


class GcodeEmitter:
    """`G21/G90/M5`で始まるプロッタ用G-codeの共通ビルダ。

    座標・feedの文字列書式はツールごとに異なる(既存出力互換のため)ので、
    フォーマッタを注入できる。構造(プリアンブル、pen up/down、G0/G1、
    dwell挿入)はここで一元化する。
    """

    def __init__(
        self,
        coord_fmt: Callable[[float], str] = _default_fmt,
        feed_fmt: Callable[[float], str] = _default_fmt,
    ) -> None:
        self._coord = coord_fmt
        self._feed = feed_fmt
        self.lines: list[str] = ["G21", "G90", "M5"]

    def raw(self, line: str) -> None:
        self.lines.append(line)

    def blank(self) -> None:
        self.lines.append("")

    def comment(self, text: str) -> None:
        self.lines.append(f"; {text}")

    def travel(self, x_mm: float, y_mm: float, feed_mm_min: float) -> None:
        self.lines.append(
            f"G0 X{self._coord(x_mm)} Y{self._coord(y_mm)} F{self._feed(feed_mm_min)}"
        )

    def draw(self, x_mm: float, y_mm: float, feed_mm_min: float) -> None:
        self.lines.append(
            f"G1 X{self._coord(x_mm)} Y{self._coord(y_mm)} F{self._feed(feed_mm_min)}"
        )

    def pen_down(self, dwell_ms: int = 0) -> None:
        self.lines.append("M3")
        if dwell_ms > 0:
            self.lines.append(f"G4 P{dwell_ms}")

    def pen_up(self, dwell_ms: int = 0, skip_if_up: bool = False) -> None:
        if skip_if_up and self.lines and self.lines[-1] == "M5":
            return
        self.lines.append("M5")
        if dwell_ms > 0:
            self.lines.append(f"G4 P{dwell_ms}")

    def text(self) -> str:
        return "\n".join(self.lines) + "\n"
