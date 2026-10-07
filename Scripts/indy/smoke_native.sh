#!/usr/bin/env bash
# Headless smoke test of the native Linux build (Stage 5): runs it under Xvfb, silent (SDL dummy audio), in a run
# directory of its own (game data linked from game/run, its own config and SaveGames, so the Wine install and your
# config stay untouched). Screenshots every INDY_SMOKE_INTERVAL seconds (default 10); Escape after each to skip movies.
# INDY_SMOKE_ACTIONS="<second>:<xdotool command>;..." as in smoke.sh. Other INDY_* debug variables pass through.
# INDY_NATIVE_WRAPPER: command line to run the game under, e.g. "valgrind --log-file=vg.txt".
# Usage: Scripts/indy/smoke_native.sh [seconds=40] [binary=Build/linux-i686/Jones3D/Jones3D] [game args...]
# Output: game/screens/native-<timestamp>/ (shot-<t>s.png, stdout.log, stderr.log, JonesLog.txt)
set -uo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
secs=${1:-40}
bin=$(realpath "${2:-$ROOT/Build/linux-i686/Jones3D/Jones3D}")
shift 2 2>/dev/null || shift $#
RUN=${INDY_NATIVE_RUN:-${XDG_RUNTIME_DIR:-/tmp}/indycompiled-native-run}
OUT="$ROOT/game/screens/native-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$OUT" "$RUN/Resource" "$RUN/SaveGames"

# Run directory: links to the game data (links can't live on the exFAT drive, so this lives in tmpfs)
G="$ROOT/game/run"
for f in "$G"/Resource/*.GOB "$G"/Resource/*.snm; do ln -sfn "$f" "$RUN/Resource/$(basename "$f")"; done
for d in KeySets Install; do ln -sfn "$G/$d" "$RUN/$d"; done
[[ -f "$RUN/Resource/Jones.cfg" ]] || cp "$G/Resource/Jones.cfg.bak-20261006" "$RUN/Resource/Jones.cfg" # the original defaults

export SDL_AUDIO_DRIVER=${SDL_AUDIO_DRIVER:-dummy}
export INDY_NO_GAMEPAD=${INDY_NO_GAMEPAD:-1} # never drive (or rumble) a real controller from a test
xvfb-run -a -s "-screen 0 ${INDY_SMOKE_SCREEN:-1024x768}x24" bash -s -- "$RUN" "$bin" "$secs" "$OUT" "$@" <<'INNER'
run="$1"; bin="$2"; secs="$3"; out="$4"; shift 4
cd "$run/Resource"
${INDY_NATIVE_WRAPPER:-} "$bin" "$@" >"$out/stdout.log" 2>"$out/stderr.log" &
pid=$!
declare -A actions
IFS=';' read -ra parts <<< "${INDY_SMOKE_ACTIONS:-}"
for part in "${parts[@]}"; do
    [[ -n "$part" ]] && actions[${part%%:*}]+="${part#*:};"
done
for ((t = 1; t <= secs; t++)); do
    sleep 1
    kill -0 $pid 2>/dev/null || { echo "EXITED after ${t}s"; break; }
    if [[ -n "${actions[$t]:-}" ]]; then
        IFS=';' read -ra cmds <<< "${actions[$t]}"
        for c in "${cmds[@]}"; do
            if [[ "$c" == shot\ * ]]; then import -window root "$out/shot-${c#shot }.png"
            elif [[ -n "$c" ]]; then xdotool $c 2>/dev/null; fi
        done
    fi
    if (( t % ${INDY_SMOKE_INTERVAL:-10} == 0 )); then
        import -window root "$out/shot-${t}s.png"
        [[ "${INDY_SMOKE_NOKEYS:-0}" == 1 ]] || xdotool key Escape 2>/dev/null || true
    fi
done
if kill -0 $pid 2>/dev/null; then
    echo "ALIVE after ${secs}s"; kill $pid
    for ((i = 0; i < 10 && $(kill -0 $pid 2>/dev/null && echo 1 || echo 0); i++)); do sleep 0.5; done
    kill -0 $pid 2>/dev/null && { echo "still alive (assert dialog?): SIGKILL"; kill -9 $pid; }
fi
wait $pid; echo "exit code $?"
INNER
cp "$RUN/Resource/JonesLog.txt" "$OUT/" 2>/dev/null
echo "output: $OUT"
