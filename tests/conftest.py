"""pytest共通設定。

各ツールはパッケージ化されていない単独スクリプトのため、
tools/配下のディレクトリをここで一括してsys.pathへ追加する。
テストファイル個別のsys.path.insertは書かない。
"""

from __future__ import annotations

import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parents[1]

for _tool_dir in ("serial_tool", "qr_tool", "text_tool", "webui"):
    _path = str(REPO_ROOT / "tools" / _tool_dir)
    if _path not in sys.path:
        sys.path.insert(0, _path)
