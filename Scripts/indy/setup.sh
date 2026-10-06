#!/usr/bin/env bash
# Runtime setup under Wine (PROJECT.md §5.5):
#   1. verify game/original against its SHA256SUMS
#   2. create two Wine prefixes on the root filesystem (Wine needs symlinks; /mnt/nvme is exFAT):
#        prefix       for playing (audio on)
#        prefix-test  for automated runs (Scripts/indy/smoke.sh): audio driver "none", own wineserver
#   3. assemble game/run/ as a copy of the original install (no symlinks on exFAT), without ddraw.dll
#   4. write the game's registry settings (values from game/original/regs.cmd), pointing at game/run/, into both
#
# Usage: Scripts/indy/setup.sh [--refresh-run] [--keep-ddraw]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ORIG="$ROOT/game/original"
RUN="$ROOT/game/run"
PREFIX_BASE="$HOME/.local/share/indycompiled"
export WINEDEBUG="${WINEDEBUG:--all}"
export WINEDLLOVERRIDES="${WINEDLLOVERRIDES:-mscoree,mshtml=}" # don't offer to install Wine Mono/Gecko

refresh_run=0
keep_ddraw=0
for arg in "$@"; do
    case "$arg" in
        --refresh-run) refresh_run=1 ;;
        --keep-ddraw) keep_ddraw=1 ;;
        *) echo "unknown option: $arg" >&2; exit 2 ;;
    esac
done

echo "== verifying $ORIG"
(cd "$ORIG" && sha256sum --check --quiet SHA256SUMS)

if [[ $refresh_run -eq 1 || ! -d "$RUN" ]]; then
    echo "== assembling $RUN"
    rm -rf "$RUN"
    cp -r "$ORIG" "$RUN"
    rm -f "$RUN/SHA256SUMS"
    [[ $keep_ddraw -eq 1 ]] || rm -f "$RUN/Resource/ddraw.dll"
fi

regfile="$(mktemp --suffix=.reg)"
trap 'rm -f "$regfile"' EXIT
reg_escape() { printf '%s' "$1" | sed 's/\\/\\\\/g'; }

for kind in play test; do
suffix=""
[[ $kind == test ]] && suffix="-test"
export WINEPREFIX="$PREFIX_BASE/prefix$suffix"
if [[ ! -f "$WINEPREFIX/system.reg" ]]; then
    echo "== creating Wine prefix $WINEPREFIX"
    mkdir -p "$WINEPREFIX"
    wineboot --init
    wineserver --wait
fi
echo "== writing registry settings ($kind)"
winrun="$(winepath -w "$RUN")"
run_esc="$(reg_escape "$winrun")"
cat > "$regfile" <<EOF
REGEDIT4

[HKEY_CURRENT_USER\\Software\\LucasArts Entertainment Company LLC\\Indiana Jones and the Infernal Machine\\v1.0]
"CD Path"=".\\\\"
"Executable"="$run_esc\\\\Resource\\\\Indy3d.exe"
"Install Path"="$run_esc"
"JoystickID"="1"
"Launcher"="$run_esc\\\\Jones3d.exe"
"Source Dir"=".\\\\"
"Source Path"=".\\\\"
"UninstallString"=""
"InstallType"=dword:00000001
"Sound Volume"=hex:b8,1e,45,3f
"Magic"=dword:000927d8
"Installed"=dword:00000002
"Start Mode"=dword:00000000
"Joystick Control"=dword:00000001
"Configuration"="XBOX360"
"Performance Level"=dword:00000004
"Display"="Primary Display Driver"
"3D Device"="Direct3D HAL"
"Width"=dword:00000280
"Height"=dword:000001e0
"BPP"=dword:00000020
"Fog Density"=hex:00,00,80,3f
"Buffering"=dword:00000001
"Fog"=dword:00000001
"Filter"=dword:00000002
"Show Text"=dword:00000001
"Map Rotation"=dword:00000000
"Show Hints"=dword:00000000
"Difficulty"=dword:00000003
"Default Run"=dword:00000000
"Sound 3D"=dword:00000000
"ReverseSound"=dword:00000000
EOF
wine regedit /S "$(winepath -w "$regfile")"
if [[ $kind == test ]]; then
    wine reg add 'HKCU\Software\Wine\Drivers' /v Audio /t REG_SZ /d "" /f >/dev/null   # no audio driver
fi
wineserver --wait
echo "== done: $kind prefix $WINEPREFIX, run folder $RUN ($winrun)"
done
