#!/usr/bin/env bash
# Continuous damage vs frame rate (FIX-0002, fpsIndependentDamage): INDY_TEST_DPS deals 100 damage per second to Indy
# for 3 s of game time (through the same path as drowning), at several frame caps, with FIX-0002 off and on.
# Expected with the fix: 300 health lost at every cap. Without it, sithThing_DamageThing truncates the per-frame
# amount: 3.3 -> 3 at 30 FPS, 1.7 -> 1 at 60 FPS, 0.8 -> 0 at 120 FPS.
# Jones.cfg is changed for each run and restored afterwards. Takes about 60 s per run.
# Usage: Scripts/indy/test_damage.sh [caps="30 60 120"]
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cfg="$ROOT/game/run/Resource/Jones.cfg"
log="$ROOT/game/run/Resource/JonesLog.txt"
backup=$(mktemp)
cp "$cfg" "$backup"
trap 'cp "$backup" "$cfg"; rm -f "$backup"' EXIT

set_fix() {
    python3 -I - "$cfg" "$1" <<'PY'
import json, sys
path, on = sys.argv[1], sys.argv[2] == "1"
c = json.load(open(path))
c.setdefault("indycompiled", {}).setdefault("toggles", {})["fpsIndependentDamage"] = on
json.dump(c, open(path, "w"), indent=4)
PY
}

for cap in ${1:-30 60 120}; do
    for fix in 0 1; do
        set_fix "$fix"
        INDY_SMOKE_NOKEYS=1 INDY_SMOKE_INTERVAL=1000 INDY_SMOKE_PROC=Indy3D.exe \
            INDY_SMOKE_ACTIONS="12:key Escape;24:key Escape" \
            INDY_FPS_CAP=$cap INDY_INPUT_TRACE=250 INDY_TEST_DPS="100@35+3" \
            "$ROOT/Scripts/indy/smoke.sh" 56 Resource/Jones3D.exe Indy3D.exe 1 > /dev/null 2>&1
        cp "$log" "$ROOT/Build/damage-$cap-$fix.log"
        result=$(python3 -I - "$ROOT/Build/damage-$cap-$fix.log" <<'PY'
import re, statistics, sys
pts = [(float(m.group(1)), float(m.group(2))) for m in
       (re.search(r"fps ([\d.]+) health ([\d.]+)", l) for l in open(sys.argv[1], encoding="latin1")) if m]
if not pts:
    print("no trace")
else:
    # Indy starts the level with 1000 health; the damage window may begin before the trace does
    print(f"{1000 - pts[-1][1]:5.0f} health lost at {statistics.median(f for f, _ in pts):5.1f} fps")
PY
)
        printf "cap %3s fps, FIX-0002 %-3s: %s\n" "$cap" "$([[ $fix == 1 ]] && echo on || echo off)" "$result"
    done
done
