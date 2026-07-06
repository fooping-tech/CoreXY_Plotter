#!/bin/sh
# nativeテストはPlatformIOへ統合済み。このスクリプトは薄いラッパとして残す。
# Windows/Linuxとも `pio test -e native` で同じテストが実行できる。
set -eu

cd "$(dirname "$0")/.."
exec pio test -e native "$@"
