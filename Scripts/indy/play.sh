#!/usr/bin/env bash
# Play the game from game/run/ in the play prefix (with audio), inside a Wine virtual desktop window
# (avoids fullscreen mode switches under i3). PROJECT.md §5.5.
#
# The desktop gets the game's resolution from Jones.cfg (graphics.width/height), and under i3 the window is switched
# to fullscreen, since a tiled window smaller than the desktop would cut the picture off ($mod+f toggles it back;
# INDY_NO_FULLSCREEN=1 skips it).
#
# Usage: Scripts/indy/play.sh [--build] [WIDTHxHEIGHT]
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

cfg="$ROOT/game/run/Resource/Jones.cfg"
size="${1:-$(python3 -I -c '
import json, sys
g = json.load(open(sys.argv[1]))["graphics"]
print("%dx%d" % (g["width"], g["height"]))' "$cfg" 2>/dev/null || echo 1280x960)}"

cd "$(dirname "$exe")"
wine explorer "/desktop=Indy,$size" "$(winepath -w "$exe")" &
pid=$!

if [[ -z "${INDY_NO_FULLSCREEN:-}" ]] && command -v i3-msg > /dev/null && command -v xdotool > /dev/null; then
    for _ in $(seq 100); do
        if xdotool search --name '^Indy - Wine Desktop$' > /dev/null 2>&1; then
            i3-msg -q '[title="^Indy - Wine Desktop$"] fullscreen enable' || true
            break
        fi
        sleep 0.2
    done
fi

wait "$pid"
