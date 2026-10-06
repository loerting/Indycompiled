#!/usr/bin/env bash
# Turn rate vs frame rate (FIX-0001, fpsIndependentTurning): in Canyonlands, hold Right for 4 s while standing still
# and measure Indy's turn rate from the position trace, at several frame caps (INDY_FPS_CAP), with FIX-0001 off and on.
# Without the fix the rate grows with the frame rate; with it, it should be the same at every cap.
# Jones.cfg is changed for each run and restored afterwards. Takes about 65 s per run.
# Usage: Scripts/indy/test_turnrate.sh [caps="30 60 120"]
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
c.setdefault("indycompiled", {}).setdefault("toggles", {})["fpsIndependentTurning"] = on
json.dump(c, open(path, "w"), indent=4)
PY
}

# turn rate in degrees per second from the trace: heading change per sample; the samples turning faster than 30% of
# the fastest one are the turn (the idle animation sways the heading a little), and their median is the rate
measure() {
    python3 -I - "$1" <<'PY'
import re, statistics, sys
pts = []
for line in open(sys.argv[1], encoding="latin1"):
    m = re.search(r"t (\d+) player pos .* heading (-?[\d.]+) moveStatus \d+ fps ([\d.]+)", line)
    if m:
        pts.append((int(m.group(1)) / 1000.0, float(m.group(2)), float(m.group(3))))
rates = []
for (t0, h0, _), (t1, h1, fps) in zip(pts, pts[1:]):
    dh = (h1 - h0 + 180) % 360 - 180
    if t1 > t0:
        rates.append((abs(dh) / (t1 - t0), fps))
top = max((r for r, _ in rates), default=0)
turning = [(r, f) for r, f in rates if top > 0 and r > 0.3 * top]
if len(turning) < 5:
    print("no turn measured"); sys.exit()
rate = statistics.median(r for r, _ in turning)
fps = statistics.median(f for _, f in turning)
print(f"{rate:6.1f} deg/s at {fps:5.1f} fps ({len(turning)} samples)")
PY
}

for cap in ${1:-30 60 120}; do
    for fix in 0 1; do
        set_fix "$fix"
        INDY_SMOKE_NOKEYS=1 INDY_SMOKE_INTERVAL=1000 INDY_SMOKE_PROC=Indy3D.exe \
            INDY_SMOKE_ACTIONS="12:key Escape;24:key Escape;52:keydown Right;56:keyup Right" \
            INDY_FPS_CAP=$cap INDY_INPUT_TRACE=100 \
            "$ROOT/Scripts/indy/smoke.sh" 60 Resource/Jones3D.exe Indy3D.exe 1 > /dev/null 2>&1
        cp "$log" "$ROOT/Build/turnrate-$cap-$fix.log"
        printf "cap %3s fps, FIX-0001 %-3s: %s\n" "$cap" "$([[ $fix == 1 ]] && echo on || echo off)" "$(measure "$ROOT/Build/turnrate-$cap-$fix.log")"
    done
done
