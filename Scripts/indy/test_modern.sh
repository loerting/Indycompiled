#!/usr/bin/env bash
# Modern controls (ENH-0005): with the virtual pad in Canyonlands, push the left stick right (Indy should turn ~90
# degrees right and walk that way), then down (he should turn to face the camera, which keeps its direction, and walk
# toward it). Reports headings and walking directions from the INDY_INPUT_TRACE log.
# Jones.cfg is changed for the run and restored afterwards. About 70 s.
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cfg="$ROOT/game/run/Resource/Jones.cfg"
log="$ROOT/game/run/Resource/JonesLog.txt"
backup=$(mktemp)
cp "$cfg" "$backup"
trap 'cp "$backup" "$cfg"; rm -f "$backup"' EXIT
python3 -I - "$cfg" <<'PY'
import json, sys
c = json.load(open(sys.argv[1]))
c.setdefault("controls", {}).update({"controller": True, "configFile": "XBOX360"})
c.setdefault("indycompiled", {}).setdefault("toggles", {})["modernControls"] = True
json.dump(c, open(sys.argv[1], "w"), indent=4)
PY
python3 -I "$ROOT/Scripts/indy/virtual_pad.py" 70 "52:lx=0.6;54.5:lx=0;57:ly=0.6;60:ly=0" > "$ROOT/Build/virtual_pad.log" 2>&1 &
padpid=$!
sleep 1
INDY_INPUT_TRACE=100 INDY_SMOKE_NOKEYS=1 INDY_SMOKE_INTERVAL=1000 INDY_SMOKE_PROC=Indy3D.exe \
    INDY_SMOKE_ACTIONS="12:key Escape;24:key Escape" "$ROOT/Scripts/indy/smoke.sh" 66 Resource/Jones3D.exe Indy3D.exe 1 > /dev/null 2>&1
wait "$padpid"
cp "$log" "$ROOT/Build/modern.log"
python3 -I - "$ROOT/Build/modern.log" <<'PY'
import math, re, sys
rows, cur, views = [], None, []
for line in open(sys.argv[1], encoding="latin1"):
    m = re.search(r"t (\d+) player pos (\S+) (\S+) \S+ heading (\S+) moveStatus (\d+)", line)
    if m:
        cur = {"t": int(m.group(1)), "x": float(m.group(2)), "y": float(m.group(3)), "h": float(m.group(4)), "fwd": 0.0, "right": 0.0}
        rows.append(cur)
        continue
    m = re.search(r"trace: (forward|right) \(\d+ bindings\): (.*)", line)
    if m and cur:
        vals = [float(v) for v in re.findall(r"axis [0-9A-F]+=(-?[\d.]+)", m.group(2))]
        cur["fwd" if m.group(1) == "forward" else "right"] = max(vals, key=abs) if vals else 0.0
    m = re.search(r"indyCamera trace: t (\d+) .* view (\S+) world-stable (\d)", line)
    if m:
        views.append((int(m.group(1)), float(m.group(2))))
def phase(rows, key):
    idx = [i for i, r in enumerate(rows) if abs(r[key]) > 0.1]
    return rows[idx[0]:idx[-1] + 1] if idx else []
def wrap(a):
    return (a + 180) % 360 - 180
start_heading = rows[0]["h"]
def view_at(t):  # camera direction just before t
    before = [v for tv, v in views if tv <= t]
    return before[-1] if before else float("nan")
ok = True
for name, key, want in (("stick right", "right", -90.0), ("stick down", "fwd", 180.0)):
    part = phase(rows, key)
    if len(part) < 2:
        print(f"{name}: no stick activity"); ok = False; continue
    a, b = part[0], part[-1]
    dist = math.hypot(b["x"] - a["x"], b["y"] - a["y"])
    walk = math.degrees(math.atan2(b["y"] - a["y"], b["x"] - a["x"])) if dist > 0.01 else float("nan")
    ref = view_at(a["t"])
    face = wrap(b["h"] - ref)
    good = abs(wrap(face - want)) < 20 and dist > 0.05 and abs(wrap(walk - b["h"])) < 30
    ok &= good
    print(f"{name}: Indy turned to {face:+.0f} deg relative to the camera (want {want:+.0f}), walked {dist:.2f} along "
          f"{wrap(walk - ref):+.0f} deg  {'ok' if good else 'CHECK'}")
print("PASS" if ok else "CHECK the trace: Build/modern.log")
PY
