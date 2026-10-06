#!/usr/bin/env bash
# Ghidra project for Indy3D.exe v1.2 (PROJECT.md §5.6, §5.7). Project lives in game/ghidra/ (git-ignored).
#
#   Scripts/indy/ghidra.sh init        create the project, import + analyze the exe, apply the address map
#   Scripts/indy/ghidra.sh apply-map   re-apply Scripts/indy/rti_v12.csv after the map changed
#   Scripts/indy/ghidra.sh gui         open Ghidra (with the i3 fix for grey Java windows)
#   Scripts/indy/ghidra.sh decompile <name or 0xaddress>...
#                                      decompile into game/review/decomp/<name>.c (git-ignored: never commit it)
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
GHIDRA=/opt/ghidra
PROJ_DIR="$ROOT/game/ghidra"
PROJ=Indy3D
EXE="$ROOT/game/original/Resource/Indy3D.exe"
MAP="$ROOT/Scripts/indy/rti_v12.csv"
SCRIPTS="$ROOT/Scripts/indy/ghidra"
export JAVA_HOME=/usr/lib/jvm/java-21-openjdk   # Ghidra 12 needs Java 21+; the system default is 17

case "${1:-}" in
    init)
        mkdir -p "$PROJ_DIR"
        "$GHIDRA/support/analyzeHeadless" "$PROJ_DIR" "$PROJ" -import "$EXE" -overwrite \
            -scriptPath "$SCRIPTS" -postScript ImportIndyMap.java "$MAP"
        ;;
    apply-map)
        "$GHIDRA/support/analyzeHeadless" "$PROJ_DIR" "$PROJ" -process Indy3D.exe -noanalysis \
            -scriptPath "$SCRIPTS" -postScript ImportIndyMap.java "$MAP"
        ;;
    decompile)
        shift
        mkdir -p "$ROOT/game/review/decomp"
        "$GHIDRA/support/analyzeHeadless" "$PROJ_DIR" "$PROJ" -process Indy3D.exe -noanalysis -readOnly \
            -scriptPath "$SCRIPTS" -postScript DecompileFunctions.java "$ROOT/game/review/decomp" "$@"
        ;;
    gui)
        export _JAVA_AWT_WM_NONREPARENTING=1   # i3: without this, Java windows stay grey
        exec "$GHIDRA/ghidraRun" "$PROJ_DIR/$PROJ.gpr"
        ;;
    *)
        sed -n '2,9p' "$0"
        exit 2
        ;;
esac
