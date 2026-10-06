#!/usr/bin/env bash
# Controller test without a controller: a virtual Xbox 360 pad (virtual_pad.py, Linux uinput) drives Indy in
# Canyonlands through the whole path SDL -> Wine winebus -> XInput -> stdControl -> keyset bindings -> ENH-0001.
# Jones.cfg is switched to controller mode with the XBOX360 keyset for the run and restored afterwards.
# Checks: the game sees one XInput pad, and the left stick moves Indy (half push walks, full push runs, turning
# follows the push). INDY_INPUT_TRACE logs the stick readings once a second.
# INDY_TEST_ANALOG=0|1 forces ENH-0001 (analog movement) off or on for the run.
# Usage: Scripts/indy/test_gamepad.sh
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
cfg="$ROOT/game/run/Resource/Jones.cfg"
log="$ROOT/game/run/Resource/JonesLog.txt"
backup=$(mktemp)
cp "$cfg" "$backup"
trap 'cp "$backup" "$cfg"; rm -f "$backup"' EXIT

python3 -I - "$cfg" <<'PY'
import json, os, sys
path = sys.argv[1]
c = json.load(open(path))
c.setdefault("controls", {}).update({"controller": True, "configFile": "XBOX360"})
analog = os.environ.get("INDY_TEST_ANALOG")  # "0"/"1": force ENH-0001 off/on for this run
if analog in ("0", "1"):
    c.setdefault("indycompiled", {}).setdefault("toggles", {})["analogMovement"] = analog == "1"
json.dump(c, open(path, "w"), indent=4)
PY

# timeline in seconds from the pad's start (about 1 s before the game's): the opening fly-by ends at 40-50 s.
# Indy starts near a ledge edge: a half push walks him to it and he stops there, turns don't move him, and the
# full push at the end runs, which jumps off the ledge (Game Over, the same as Shift+Up on the keyboard).
pad="52:ly=-0.5;55:ly=0;57:lx=1.0;58:lx=0;60:lx=0.6;62:lx=0;64:ly=-1.0;64.6:ly=0"
python3 -I "$ROOT/Scripts/indy/virtual_pad.py" 70 "$pad" > "$ROOT/Build/virtual_pad.log" 2>&1 &
padpid=$!
sleep 1
# screenshots only for a look (smoke.sh's clock drifts behind by ~0.3 s per screenshot); the checks use the log
INDY_SMOKE_NOKEYS=1 INDY_SMOKE_INTERVAL=1000 INDY_SMOKE_PROC=Indy3D.exe \
    INDY_SMOKE_ACTIONS="12:key Escape;24:key Escape;50:shot still;66:shot end" \
    INDY_INPUT_TRACE=${INDY_INPUT_TRACE:-250} "$ROOT/Scripts/indy/smoke.sh" 68 Resource/Jones3D.exe Indy3D.exe 1 > /dev/null 2>&1
wait "$padpid"

echo "--- virtual pad"; cat "$ROOT/Build/virtual_pad.log"
echo "--- game log (controllers, analog movement)"
grep -a -E "XInput|Gamepad|Joystick|joystick|indyInput: " "$log"
echo "--- stick readings (INDY_INPUT_TRACE, once a second; non-zero only)"
grep -a "indyInput trace" "$log" | grep -v "player pos" | grep -E "=-?0\.[0-9]*[1-9]|=-?1\.00" | sed 's/^.*trace: //' | uniq -c
stamp=$(ls -t "$ROOT"/game/screens/smoke-*-still.png | head -1 | sed 's/-still\.png$//')
echo "screenshots: $stamp-{still,end}.png"

check() { if grep -a -q -E "$2" "$log"; then echo "ok    $1"; else echo "FAIL  $1"; fail=1; fi; }
fail=0
echo "--- checks"
check "one XInput pad found"            "Total XInput gamepad controllers found: 1"
check "half push walks"                 "indyInput: forward stick 0\.[0-9]+ -> walk"
check "full push runs"                  "indyInput: forward stick 1\.00 -> run"
check "full right push reads 1.00"      "trace: right .*axis 80000000=1\.00"
check "partial right push reads ~0.47"  "trace: right .*axis 80000000=0\.4[0-9]"
# walking stops at the ledge edge and turning keeps Indy in place: the position traced between the walk decision
# and the run decision stays put after the walk (a Game Over would stop the trace instead)
still=$(grep -a -E -e "-> walk|-> run|player pos" "$log" | awk '/-> walk/ { w = 1; next } /-> run/ { exit } w { print $(NF-4), $(NF-3) }' | tail -12 | sort -u | wc -l)
if (( still == 1 )); then echo "ok    Indy stops at the ledge edge and turns in place"; else echo "FAIL  Indy kept moving after the walk ($still positions)"; fail=1; fi
[[ $fail == 0 ]] && echo "PASS" || echo "CHECK the log: $log"
