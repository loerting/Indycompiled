#!/usr/bin/env bash
# Savegame round trip (PROJECT.md §8.4): in Canyonlands, quicksave (F5), run off, quickload (F8), and check that the
# loaded scene matches the saved one and differs from the moved one. Exercises the savegame (DSS) code paths.
# Timing: the opening fly-by ends at about 40 s and saving stays disabled until then, so F5 comes at 50 s.
# Usage: Scripts/indy/test_savegame.sh
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
quick="$ROOT/game/run/SaveGames/QUICKSAVE.nds"
marker=$(mktemp)   # quicksave must be newer than this
out=$(INDY_SMOKE_NOKEYS=1 INDY_SMOKE_INTERVAL=1000 INDY_SMOKE_PROC=Indy3D.exe \
      INDY_SMOKE_ACTIONS="12:key Escape;24:key Escape;50:key F5;53:shot saved;54:keydown Up;58:keyup Up;60:shot moved;62:key F8;72:shot loaded" \
      "$ROOT/Scripts/indy/smoke.sh" 74 Resource/Jones3D.exe Indy3D.exe 1 2>&1)
alive=$(grep -q '^ALIVE' <<< "$out" && echo yes || echo NO)
saved=$([[ "$quick" -nt "$marker" ]] && echo yes || echo NO)
rm -f "$marker"
stamp=$(ls -t "$ROOT"/game/screens/smoke-*-saved.png | head -1 | sed 's/-saved\.png$//')
rmse() { magick compare -metric RMSE "$1" "$2" null: 2>&1 | sed -E 's/.*\((.*)\)/\1/'; }
diff_moved=$(rmse "$stamp-saved.png" "$stamp-moved.png")
diff_loaded=$(rmse "$stamp-saved.png" "$stamp-loaded.png")
echo "alive=$alive quicksave written=$saved; image difference saved/moved=$diff_moved saved/loaded=$diff_loaded"
if [[ "$saved" == yes ]] && awk -v m="$diff_moved" -v l="$diff_loaded" 'BEGIN { exit !(l < m / 2) }'; then
    echo "PASS: quicksave written, and the loaded scene matches the saved one"
else
    echo "CHECK: see $stamp-{saved,moved,loaded}.png"
fi
