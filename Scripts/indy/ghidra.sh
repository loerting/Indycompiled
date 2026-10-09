#!/usr/bin/env bash
# Ghidra project for Indy3D.exe v1.2 (PROJECT.md §5.6, §5.7). Project lives in game/ghidra/ (git-ignored).
#
#   Scripts/indy/ghidra.sh init        create the project, import + analyze the exe, apply the address map
#   Scripts/indy/ghidra.sh apply-map   re-apply Scripts/indy/rti_v12.csv after the map changed
#   Scripts/indy/ghidra.sh gui         open Ghidra (with the i3 fix for grey Java windows)
#   Scripts/indy/ghidra.sh decompile <name or 0xaddress or range:0xstart-0xend>...
#                                      decompile into game/review/decomp/<name>.c (git-ignored: never commit it)
#   Scripts/indy/ghidra.sh list-file <outfile> <source path substring, e.g. sith\Engine\sithPhysics.c>
#                                      a source file's functions, found by their assert path strings (debug build)
#
# INDY_GHIDRA_DEBUG=1: the same for the debug build (game/debug-it/indy3d.exe, from the Italian CD1.GOB; upstream's
# reference), in its own project game/ghidra-debug/, without the address map (its addresses differ); decompile output
# goes to game/review/decomp-debug/.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
GHIDRA=/opt/ghidra
PROJ_DIR="$ROOT/game/ghidra"
PROJ=Indy3D
EXE="$ROOT/game/original/Resource/Indy3D.exe"
MAP="$ROOT/Scripts/indy/rti_v12.csv"
SCRIPTS="$ROOT/Scripts/indy/ghidra"
DECOMP="$ROOT/game/review/decomp"
if [ "${INDY_GHIDRA_DEBUG:-0}" = 1 ]; then
    PROJ_DIR="$ROOT/game/ghidra-debug"
    PROJ=Indy3DDebug
    EXE="$ROOT/game/debug-it/indy3d.exe"
    MAP=""
    DECOMP="$ROOT/game/review/decomp-debug"
fi
EXE_NAME="$(basename "$EXE")"
export JAVA_HOME=/usr/lib/jvm/java-21-openjdk   # Ghidra 12 needs Java 21+; the system default is 17

case "${1:-}" in
    init)
        mkdir -p "$PROJ_DIR"
        if [ -n "$MAP" ]; then
            "$GHIDRA/support/analyzeHeadless" "$PROJ_DIR" "$PROJ" -import "$EXE" -overwrite \
                -scriptPath "$SCRIPTS" -postScript ImportIndyMap.java "$MAP"
        else
            "$GHIDRA/support/analyzeHeadless" "$PROJ_DIR" "$PROJ" -import "$EXE" -overwrite
        fi
        ;;
    apply-map)
        "$GHIDRA/support/analyzeHeadless" "$PROJ_DIR" "$PROJ" -process "$EXE_NAME" -noanalysis \
            -scriptPath "$SCRIPTS" -postScript ImportIndyMap.java "$MAP"
        ;;
    decompile)
        shift
        mkdir -p "$DECOMP"
        "$GHIDRA/support/analyzeHeadless" "$PROJ_DIR" "$PROJ" -process "$EXE_NAME" -noanalysis -readOnly \
            -scriptPath "$SCRIPTS" -postScript DecompileFunctions.java "$DECOMP" "$@"
        ;;
    list-file)
        "$GHIDRA/support/analyzeHeadless" "$PROJ_DIR" "$PROJ" -process "$EXE_NAME" -noanalysis -readOnly \
            -scriptPath "$SCRIPTS" -postScript ListSourceFunctions.java "$2" "$3"
        ;;
    gui)
        export _JAVA_AWT_WM_NONREPARENTING=1   # i3: without this, Java windows stay grey
        exec "$GHIDRA/ghidraRun" "$PROJ_DIR/$PROJ.gpr"
        ;;
    *)
        sed -n '2,15p' "$0"
        exit 2
        ;;
esac
