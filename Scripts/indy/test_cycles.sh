#!/usr/bin/env bash
# "Every N-th frame" events vs frame rate (FIX-0003, fpsIndependentCycles): counts how often a cycle of 16 comes
# round per second (cyc16 in the INDY_INPUT_TRACE position line) at several frame caps, with FIX-0003 off and on.
# Expected: cap/16 per second without the fix (1.9 at 30 FPS, 7.5 at 120), 30/16 = 1.9 at every cap with it.
# Jones.cfg is changed for each run and restored afterwards. Takes about 55 s per run.
# Usage: Scripts/indy/test_cycles.sh [caps="30 120"]
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
c.setdefault("indycompiled", {}).setdefault("toggles", {})["fpsIndependentCycles"] = on
json.dump(c, open(path, "w"), indent=4)
PY
}

measure() {
    python3 -I - "$1" <<'PY'
import re, statistics, sys
pts = [(int(m.group(1)) / 1000.0, float(m.group(2)), int(m.group(3)))
       for m in (re.search(r"t (\d+) player pos .* fps ([\d.]+) health [\d.]+ cyc16 (\d+)", l)
                 for l in open(sys.argv[1], encoding="latin1")) if m]
if len(pts) < 3:
    print("no trace"); sys.exit()
(t0, _, c0), (t1, _, c1) = pts[1], pts[-1]   # skip the first sample (level start)
print(f"{(c1 - c0) / (t1 - t0):5.2f} cycles of 16 per second at {statistics.median(f for _, f, _ in pts):5.1f} fps")
PY
}

for cap in ${1:-30 120}; do
    for fix in 0 1; do
        set_fix "$fix"
        INDY_SMOKE_NOKEYS=1 INDY_SMOKE_INTERVAL=1000 INDY_SMOKE_PROC=Indy3D.exe \
            INDY_SMOKE_ACTIONS="12:key Escape;24:key Escape" INDY_FPS_CAP=$cap INDY_INPUT_TRACE=500 \
            "$ROOT/Scripts/indy/smoke.sh" 54 Resource/Jones3D.exe Indy3D.exe 1 > /dev/null 2>&1
        cp "$log" "$ROOT/Build/cycles-$cap-$fix.log"
        printf "cap %3s fps, FIX-0003 %-3s: %s\n" "$cap" "$([[ $fix == 1 ]] && echo on || echo off)" "$(measure "$ROOT/Build/cycles-$cap-$fix.log")"
    done
done
