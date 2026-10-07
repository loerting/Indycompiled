#!/usr/bin/env bash
# Level sweep (PROJECT.md §8.4): start every level in our build (headless, silent), and report per level whether
# the game survived, whether the level's .cnd loaded, engine errors and Wine crash messages.
# Exercises much more engine code than one level, so it also catches address-map errors.
#
# Usage: Scripts/indy/level_sweep.sh [seconds per level=60] [levels="1 2 ... 17"]
# INDY_SWEEP_EXE=<exe in game/run/Resource> runs that exe directly, e.g. the standalone build (Stage 4) copied there.
# Output: game/screens/sweep-<timestamp>/ (per level: log, Wine log, end screenshot) and summary.md
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
secs="${1:-60}"
levels="${2:-$(seq -s ' ' 1 17)}"
out="$ROOT/game/screens/sweep-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$out"
log="$ROOT/game/run/Resource/JonesLog.txt"

printf '| level | alive | loaded | engine errors | wine err | end screenshot colours |\n|---|---|---|---|---|---|\n' > "$out/summary.md"
for lvl in $levels; do
    rm -f "$log" "$ROOT/game/run/Resource/JonesError.txt"
    if [[ -n "${INDY_SWEEP_EXE:-}" ]]; then
        result=$(INDY_SMOKE_PROC="$INDY_SWEEP_EXE" "$ROOT/Scripts/indy/smoke.sh" "$secs" "Resource/$INDY_SWEEP_EXE" "$lvl" 2>&1)
    else
        result=$(INDY_SMOKE_PROC=Indy3D.exe "$ROOT/Scripts/indy/smoke.sh" "$secs" Resource/Jones3D.exe Indy3D.exe "$lvl" 2>&1)
    fi
    alive=$(grep -q '^ALIVE' <<< "$result" && echo yes || echo NO)
    wlog=$(sed -n 's/^log: //p' <<< "$result")
    shot=$(ls -t "$ROOT"/game/screens/smoke-*-end.png | head -1)
    cp "$log" "$out/level$lvl.log" 2>/dev/null
    cp "$wlog" "$out/level$lvl.wine.log" 2>/dev/null
    cp "$shot" "$out/level$lvl.png" 2>/dev/null
    loaded=$(grep -oE 'to load [0-9]+_[A-Za-z]+\.cnd' "$log" 2>/dev/null | grep -v jones3dstatic | tail -1 | sed 's/to load //')
    errs=$(grep -ciE 'error|assert|fail' "$log" 2>/dev/null)
    werr=$(grep -cE ':err:' "$wlog" 2>/dev/null | head -1)
    [[ -f "$ROOT/game/run/Resource/JonesError.txt" ]] && cp "$ROOT/game/run/Resource/JonesError.txt" "$out/level$lvl.error.txt"
    colours=$(magick "$shot" -format '%k' info: 2>/dev/null)
    printf '| %s | %s | %s | %s | %s | %s |\n' "$lvl" "$alive" "${loaded:--}" "${errs:-0}" "${werr:-0}" "$colours" | tee -a "$out/summary.md"
done
echo "summary: $out/summary.md"
