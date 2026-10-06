#!/usr/bin/env bash
# Play the game from game/run/ in the play prefix (with audio), inside a Wine virtual desktop window
# (avoids fullscreen mode switches under i3). PROJECT.md §5.5.
#
# Usage: Scripts/indy/play.sh [WIDTHxHEIGHT=1280x960]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
export WINEPREFIX="$HOME/.local/share/indycompiled/prefix"
export WINEDEBUG="${WINEDEBUG:--all}"
export WINEDLLOVERRIDES="${WINEDLLOVERRIDES:-mscoree,mshtml=}"
[[ -f "$WINEPREFIX/system.reg" ]] || { echo "play prefix missing: run Scripts/indy/setup.sh" >&2; exit 1; }

size="${1:-1280x960}"
exe="$ROOT/game/run/Resource/Indy3D.exe"
cd "$(dirname "$exe")"
exec wine explorer "/desktop=Indy,$size" "$(winepath -w "$exe")"
