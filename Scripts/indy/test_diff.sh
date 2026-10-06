#!/usr/bin/env bash
# Differential test (Stage 3): runs the game headless with INDY_DIFFTEST=<suite>; once the first level runs, the
# suite calls each tested function in the original v1.2 code and in our C code with the same inputs and compares
# the results (Libs/indy/indyDiff.c). Prints the report (game/run/Resource/indy_difftest.txt).
# Usage: Scripts/indy/test_diff.sh <suite>   (suites: AudioLib)
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
suite="${1:?usage: test_diff.sh <suite>}"
report="$ROOT/game/run/Resource/indy_difftest.txt"
rm -f "$report"
INDY_DIFFTEST="$suite" INDY_SMOKE_NOKEYS=1 INDY_SMOKE_INTERVAL=1000 INDY_SMOKE_PROC=Indy3D.exe \
    "$ROOT/Scripts/indy/smoke.sh" 45 Resource/Jones3D.exe Indy3D.exe 1 > /dev/null 2>&1
if [[ -f "$report" ]]; then
    cat "$report"
    grep -q "DONE, no unexpected differences" "$report"
else
    echo "no report: the suite did not run (see game/run/Resource/JonesLog.txt)"
    exit 1
fi
