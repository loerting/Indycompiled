#!/usr/bin/env bash
# Play the game from game/run/ in the play prefix (with audio), inside a Wine virtual desktop window
# (avoids fullscreen mode switches under i3). PROJECT.md §5.5.
#
# Usage: Scripts/indy/play.sh [--build] [WIDTHxHEIGHT=1280x960]
#   --build  start our build (Jones3D.exe injects Jones3D.dll into Indy3D.exe v1.2; deploy it first with
#            Scripts/indy/deploy.sh) instead of the unmodified original game
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
export WINEPREFIX="$HOME/.local/share/indycompiled/prefix"
export WINEDEBUG="${WINEDEBUG:--all}"
export WINEDLLOVERRIDES="${WINEDLLOVERRIDES:-mscoree,mshtml=}"
[[ -f "$WINEPREFIX/system.reg" ]] || { echo "play prefix missing: run Scripts/indy/setup.sh" >&2; exit 1; }

exe="$ROOT/game/run/Resource/Indy3D.exe"
if [[ "${1:-}" == "--build" ]]; then
    exe="$ROOT/game/run/Resource/Jones3D.exe"
    shift
fi
size="${1:-1280x960}"
cd "$(dirname "$exe")"
exec wine explorer "/desktop=Indy,$size" "$(winepath -w "$exe")"
