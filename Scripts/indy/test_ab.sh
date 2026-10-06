#!/usr/bin/env bash
# A/B test of reimplemented functions (Stage 3): load each level twice, once with the exe's original functions
# (INDY_NOHOOK, which also routes our own C callers to the original through INDY_AB_ORIGINAL) and once with our C,
# and compare the world snapshots written at the end of sithOpen (INDY_DUMP_WORLD: every template and thing, raw bytes).
# Level loading is deterministic (same heap addresses, given the same environment size), so the snapshots must be
# byte-identical.
# Headless and silent (smoke.sh, test prefix); takes about 2 x <seconds> per level.
# Simulation A/B (AI, physics): also set INDY_FIXED_FRAME_MS=<ms> (fixed game time per frame) and
# INDY_DUMP_WORLD_FRAME=<n> (snapshot after n frames, then exit), and INDY_SMOKE_NOKEYS=1. Snapshots that aren't
# byte-identical are compared field by field (Scripts/indy/compare_world.py) with float tolerance INDY_AB_TOL
# (default 0: exact); the result line names the differing fields.
# Savegame A/B (DSS): also set INDY_SAVE_FRAME=<n> (save at frame n, before INDY_DUMP_WORLD_FRAME); both savegames
# must be byte-identical (apart from the sound clock).
# Restore A/B (DSS Process*): INDY_AB_RESTORE=<savegame file> and INDY_RESTORE_FRAME=<n>; the snapshot frame then
# counts from the restored level's start.
#
# Usage: Scripts/indy/test_ab.sh <func[,func...]> [levels="1 2 ... 17"] [seconds per run=45]
# Output: game/screens/ab-<timestamp>/ (per level: both snapshots, a diff on mismatch) and summary.md; exit 1 on mismatch
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
[[ $# -ge 1 ]] || { sed -n '2,10p' "$0"; exit 2; }
funcs="$1"
levels="${2:-$(seq -s ' ' 1 17)}"
secs="${3:-45}"
res="$ROOT/game/run/Resource"
out="$ROOT/game/screens/ab-$(date +%Y%m%d-%H%M%S)"
mkdir -p "$out"

# Restore A/B: INDY_AB_RESTORE=<savegame> and INDY_RESTORE_FRAME=<n>: both sides load the same savegame at frame n
[[ -n "${INDY_AB_RESTORE:-}" ]] && cp "$INDY_AB_RESTORE" "$res/indy_ab_restore.nds"
fail=0
printf '# A/B %s\n\n| level | world | things | original | ours | result |\n|---|---|---|---|---|---|\n' "$funcs" > "$out/summary.md"
for lvl in $levels; do
    for side in orig ours; do
        rm -f "$res/indy_ab_$side.txt"
        # same-length value on both sides: the CRT copies the environment onto the heap, so a shorter environment
        # would move every later allocation; underscores match no function
        nohook="$funcs"; [[ $side == ours ]] && nohook="${funcs//?/_}"
        rm -f "$res/indy_ab_$side.nds"
        INDY_SAVE_FILE="indy_ab_$side.nds" INDY_RESTORE_FILE="${INDY_AB_RESTORE:+indy_ab_restore.nds}" \
        INDY_NOHOOK="$nohook" INDY_DUMP_WORLD="indy_ab_$side.txt" INDY_SMOKE_PROC=Indy3D.exe \
            "$ROOT/Scripts/indy/smoke.sh" "$secs" Resource/Jones3D.exe Indy3D.exe "$lvl" > "$out/level$lvl.$side.smoke" 2>&1
        mv "$res/indy_ab_$side.txt" "$out/level$lvl.$side.txt" 2>/dev/null
        mv "$res/indy_ab_$side.nds" "$out/level$lvl.$side.nds" 2>/dev/null
    done
    o="$out/level$lvl.orig.txt"; u="$out/level$lvl.ours.txt"
    head1=$(head -1 "$o" 2>/dev/null)
    world=$(awk '{print $2}' <<< "$head1"); things=$(awk '{print $6}' <<< "$head1")
    so=$([[ -s $o ]] && sha256sum < "$o" | cut -c1-12 || echo missing)
    su=$([[ -s $u ]] && sha256sum < "$u" | cut -c1-12 || echo missing)
    if [[ $so == missing || $su == missing ]]; then
        result="NO SNAPSHOT"; fail=1
    elif [[ $so == "$su" ]]; then
        result="identical"
    elif python3 -I "$ROOT/Scripts/indy/compare_world.py" "$o" "$u" --tol "${INDY_AB_TOL:-0}" \
            --ignore "${INDY_AB_IGNORE:-renderData.rdFrameNum}" > "$out/level$lvl.diff"; then
        result="equal within tolerance ${INDY_AB_TOL:-0} ($(head -1 "$out/level$lvl.diff" | sed 's/.*; //'))"
    else
        result="DIFFERENT: $(sed -n '2,4p' "$out/level$lvl.diff" | sed 's/^ *//' | paste -sd';')"; fail=1
    fi
    if [[ -n "${INDY_SAVE_FRAME:-}" ]]; then   # savegame A/B: both saves must be byte-identical
        if [[ ! -s "$out/level$lvl.orig.nds" || ! -s "$out/level$lvl.ours.nds" ]]; then
            result+="; NO SAVEGAME"; fail=1
        else
            # the sound section stores the sound system's wall-clock time (8 bytes after its 0x12345678 marker and
            # the paused count): masked, everything else must match
            r=$(python3 -I - "$out/level$lvl.orig.nds" "$out/level$lvl.ours.nds" <<'PY'
import sys
a, b = (bytearray(open(f, "rb").read()) for f in sys.argv[1:3])
for d in (a, b):
    m = d.rfind(bytes.fromhex("78563412"))
    if m >= 0:
        d[m + 8:m + 16] = bytes(8)
diff = [i for i in range(min(len(a), len(b))) if a[i] != b[i]]
print("identical" if len(a) == len(b) and not diff else f"DIFFER at byte {diff[0] if diff else min(len(a), len(b))}")
PY
)
            result+="; savegames $r"; [[ $r == identical ]] || fail=1
        fi
    fi
    printf '| %s | %s | %s | %s | %s | %s |\n' "$lvl" "${world:--}" "${things:--}" "$so" "$su" "$result" | tee -a "$out/summary.md"
done
echo "summary: $out/summary.md"
exit $fail
