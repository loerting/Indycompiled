#!/usr/bin/env bash
# Differential test (Stage 3): runs the game headless with INDY_DIFFTEST=<suite>; once the level runs, the suite
# calls each tested function in the original v1.2 code and in our C code with the same inputs and compares the
# results (Libs/indy/indyDiff.c). Prints the report (game/run/Resource/indy_difftest.txt) per level.
# Usage: Scripts/indy/test_diff.sh <suite> [levels=1]   (suites: AudioLib, sithThing; levels e.g. "1 2 3" or all)
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
suite="${1:?usage: test_diff.sh <suite> [levels]}"
levels="${2:-1}"
[[ $levels == all ]] && levels="$(seq -s ' ' 1 17)"
report="$ROOT/game/run/Resource/indy_difftest.txt"
fail=0
for lvl in $levels; do
    rm -f "$report"
    INDY_DIFFTEST="$suite" INDY_SMOKE_INTERVAL=1000 INDY_SMOKE_PROC=Indy3D.exe \
        "$ROOT/Scripts/indy/smoke.sh" 60 Resource/Jones3D.exe Indy3D.exe "$lvl" > /dev/null 2>&1
    echo "== level $lvl"
    if [[ -f "$report" ]]; then
        cat "$report"
        grep -q "DONE, no unexpected differences" "$report" || fail=1
    else
        echo "no report: the suite did not run (see game/run/Resource/JonesLog.txt)"
        fail=1
    fi
done
exit $fail
