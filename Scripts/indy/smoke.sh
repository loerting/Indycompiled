#!/usr/bin/env bash
# Headless smoke test (PROJECT.md §8.4): run the game from game/run/ under Xvfb in a Wine virtual
# desktop, take screenshots along the way, report whether the game process survived, then stop Wine.
#
# Runs in the separate test prefix (created by Scripts/indy/setup.sh). That prefix has no audio driver,
# and its own wineserver, so a game you play in the main prefix is never touched.
# Safety net: while the test runs, any audio stream opened by a process of the test prefix is muted.
#
# INDY_SMOKE_PROC=<name> checks another process (e.g. Indy3D.exe when starting the Jones3D.exe launcher).
# INDY_SMOKE_INTERVAL=<s> screenshot interval (default 15); INDY_SMOKE_NOKEYS=1 sends no Escape key presses.
# INDY_SMOKE_ACTIONS="<second>:<xdotool command>;..." runs scripted input, e.g. "40:keydown Up;44:keyup Up";
#   "<second>:shot <name>" takes a named screenshot (smoke-<timestamp>-<name>.png).
# Usage: Scripts/indy/smoke.sh [seconds=45] [exe relative to game/run=Resource/Indy3D.exe] [game args...]
# Output: game/screens/smoke-<timestamp>-<t>s.png and smoke-<timestamp>.log
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
RUN="$ROOT/game/run"
OUT="$ROOT/game/screens"
export WINEPREFIX="$HOME/.local/share/indycompiled/prefix-test"
export WINEDEBUG="${WINEDEBUG:-fixme-all}"
export WINEDLLOVERRIDES="${WINEDLLOVERRIDES:-mscoree,mshtml=}"
[[ -f "$WINEPREFIX/system.reg" ]] || { echo "test prefix missing: run Scripts/indy/setup.sh" >&2; exit 1; }

secs="${1:-45}"
exe="${2:-Resource/Indy3D.exe}"
shift $(( $# > 2 ? 2 : $# ))
stamp="$(date +%Y%m%d-%H%M%S)"
mkdir -p "$OUT"
log="$OUT/smoke-$stamp.log"

# Mute every sink input whose process belongs to the test prefix (checked via /proc/<pid>/environ).
mute_watch() {
    while :; do
        LC_ALL=C pactl list sink-inputs 2>/dev/null |
            awk '/^Sink Input #/ { id = substr($3, 2) } /application.process.id = / { gsub(/"/, "", $3); print id, $3 }' |
            while read -r id pid; do
                if tr '\0' '\n' < "/proc/$pid/environ" 2>/dev/null | grep -qxF "WINEPREFIX=$WINEPREFIX"; then
                    pactl set-sink-input-mute "$id" 1 2>/dev/null && echo "muted stream $id (pid $pid)" >> "$log.audio"
                fi
            done
        sleep 0.3
    done
}
mute_watch &
watcher=$!
trap 'kill $watcher 2>/dev/null; wineserver -k 2>/dev/null || true' EXIT

xvfb-run -a -s "-screen 0 1024x768x24" bash -s -- "$RUN" "$exe" "$secs" "$log" "$OUT/smoke-$stamp" "$@" <<'INNER'
run="$1"; exe="$2"; secs="$3"; log="$4"; shot="$5"; shift 5
cd "$run/$(dirname "$exe")"
# full Windows path: explorer/start.exe do not search the working directory
wine explorer /desktop=Indy,800x600 "$(winepath -w "$run/$exe")" "$@" >"$log" 2>&1 &
proc="${INDY_SMOKE_PROC:-$(basename "$exe")}"   # process to check, e.g. Indy3D.exe when starting the launcher
# INDY_SMOKE_ACTIONS="<second>:<xdotool command>;..." e.g. "40:keydown Up;44:keyup Up" (scripted input)
declare -A actions
IFS=';' read -ra parts <<< "${INDY_SMOKE_ACTIONS:-}"
for part in "${parts[@]}"; do
    [[ -n "$part" ]] && actions[${part%%:*}]+="${part#*:};"
done
for ((t = 1; t <= secs; t++)); do
    sleep 1
    if [[ -n "${actions[$t]:-}" ]]; then
        IFS=';' read -ra cmds <<< "${actions[$t]}"
        for c in "${cmds[@]}"; do
            if [[ "$c" == shot\ * ]]; then import -window root "$shot-${c#shot }.png"   # "shot <name>": named screenshot
            elif [[ -n "$c" ]]; then xdotool $c 2>/dev/null; fi
        done
    fi
    if (( t % ${INDY_SMOKE_INTERVAL:-15} == 0 )); then
        import -window root "$shot-${t}s.png"
        [[ "${INDY_SMOKE_NOKEYS:-0}" == 1 ]] || xdotool key Escape 2>/dev/null || true   # skip intro videos / splash screens
    fi
done
import -window root "$shot-end.png"
# ask Wine for its process list (pgrep would also match the explorer command line)
if WINEDEBUG=-all wine tasklist 2>/dev/null | tr -d "\0\r" | grep -qai "^$proc"; then echo "ALIVE after ${secs}s: $proc"; else echo "NOT RUNNING after ${secs}s: $proc"; fi
wineserver -k 2>/dev/null || true
INNER

echo "log: $log"
[[ -f "$log.audio" ]] && echo "WARNING: audio streams were opened and muted, see $log.audio"
ls -1 "$OUT"/smoke-"$stamp"*.png
