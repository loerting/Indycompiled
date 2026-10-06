#!/usr/bin/env bash
# Direction change (ENH-0003, quickDirectionChange): with the virtual pad, walk forward, then flip the stick to back
# through a short moment of neutral (as a real stick passes the centre), with ENH-0003 off and on. Reports how long
# after the flip Indy moves backwards (from the INDY_INPUT_TRACE position log).
# Jones.cfg is changed for each run and restored afterwards. About 70 s per run.
# Usage: Scripts/indy/test_reverse.sh
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cfg="$ROOT/game/run/Resource/Jones.cfg"
log="$ROOT/game/run/Resource/JonesLog.txt"
backup=$(mktemp)
cp "$cfg" "$backup"
trap 'cp "$backup" "$cfg"; rm -f "$backup"' EXIT

for on in 0 1; do
    python3 -I - "$cfg" "$on" <<'PY'
import json, sys
path, on = sys.argv[1], sys.argv[2] == "1"
c = json.load(open(path))
c.setdefault("controls", {}).update({"controller": True, "configFile": "XBOX360"})
c.setdefault("indycompiled", {}).setdefault("toggles", {})["quickDirectionChange"] = on
json.dump(c, open(path, "w"), indent=4)
PY
    # walk back first (away from the ledge in front of Indy), then forward, flip to back through 60 ms of neutral
    python3 -I "$ROOT/Scripts/indy/virtual_pad.py" 66 "52:ly=0.5;53.5:ly=-0.5;55:ly=0;55.06:ly=0.5;58:ly=0" > "$ROOT/Build/virtual_pad.log" 2>&1 &
    padpid=$!
    sleep 1
    INDY_INPUT_TRACE=50 INDY_SMOKE_NOKEYS=1 INDY_SMOKE_INTERVAL=1000 INDY_SMOKE_PROC=Indy3D.exe \
        INDY_SMOKE_ACTIONS="12:key Escape;24:key Escape" \
        "$ROOT/Scripts/indy/smoke.sh" 64 Resource/Jones3D.exe Indy3D.exe 1 > /dev/null 2>&1
    wait "$padpid"
    cp "$log" "$ROOT/Build/reverse-$on.log"
    python3 -I - "$ROOT/Build/reverse-$on.log" "$on" <<'PY'
import math, re, sys
pts = []
for line in open(sys.argv[1], encoding="latin1"):
    m = re.search(r"t (\d+) player pos (\S+) (\S+) \S+ heading (\S+) moveStatus (\d+)", line)
    if m:
        pts.append((int(m.group(1)), float(m.group(2)), float(m.group(3)), math.radians(float(m.group(4))), int(m.group(5))))
# forward speed along the heading per sample; the flip is where forward motion ends
v = []
for (t0, x0, y0, h0, s0), (t1, x1, y1, h1, s1) in zip(pts, pts[1:]):
    dt = (t1 - t0) / 1000.0
    v.append((t1, ((x1 - x0) * math.cos(h1) + (y1 - y0) * math.sin(h1)) / dt if dt > 0 else 0.0, s1))
fwd = [i for i, (t, sp, s) in enumerate(v) if sp > 0.05]
if not fwd:
    print("no forward walk found"); sys.exit()
last_fwd = fwd[-1]
back = next((i for i in range(last_fwd + 1, len(v)) if v[i][1] < -0.05), None)
statuses = sorted({s for t, sp, s in v[last_fwd:back + 1 if back else len(v)]})
label = "on " if sys.argv[2] == "1" else "off"
if back is None:
    print(f"ENH-0003 {label}: no backward walk after the flip")
else:
    print(f"ENH-0003 {label}: backward motion {v[back][0] - v[last_fwd][0]} ms after forward motion ended (move states in between: {statuses})")
PY
done
