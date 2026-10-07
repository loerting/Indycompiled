#!/usr/bin/env bash
# Headless emulator test of the Android build: boot the x86_64 AVD without window or audio, install, start,
# a screenshot every INDY_EMU_INTERVAL seconds (default 10) for INDY_EMU_SECS seconds (default 60), each followed by
# a Back key press (skips movies), then logcat and the game's own logs (stderr.txt, JonesLog.txt), kill the emulator.
# No real phone involved.
# Usage: Scripts/android/test_emulator.sh [apk] [output dir]
#   Defaults: android/app/build/outputs/apk/debug/app-debug.apk, game/screens/android-<timestamp>/
set -uo pipefail
cd "$(dirname "$0")/../.."
APK=${1:-android/app/build/outputs/apk/debug/app-debug.apk}
OUT=${2:-game/screens/android-$(date +%Y%m%d-%H%M)}
SDK=$HOME/Android/Sdk
adb() { "$SDK/platform-tools/adb" "$@"; }
AVD=Pixel_3a_API_34_extension_level_7_x86_64
# The emulator needs ~3 GB on top of what runs already; on 2026-10-07 it froze the desktop (out of memory).
# Refuse to start unless INDY_EMU_MIN_MB (default 7000) MB are available.
avail=$(awk '/^MemAvailable:/ { print int($2 / 1024) }' /proc/meminfo)
if (( avail < ${INDY_EMU_MIN_MB:-7000} )); then
    echo "test_emulator: only ${avail} MB available, need ${INDY_EMU_MIN_MB:-7000}: close programs first" >&2
    exit 2
fi
mkdir -p "$OUT"
"$SDK/emulator/emulator" -avd "$AVD" -no-window -no-audio -no-snapshot -no-boot-anim -gpu swiftshader_indirect -memory 2048 -cores 2 >"$OUT/emulator.log" 2>&1 &
EMU=$!
trap 'adb emu kill >/dev/null 2>&1; sleep 3; kill $EMU 2>/dev/null' EXIT
adb wait-for-device
for i in $(seq 1 180); do
    [[ $(adb shell getprop sys.boot_completed 2>/dev/null | tr -d '\r') == 1 ]] && break
    sleep 2
done
echo "booted after ~$((i*2)) s"
adb shell input keyevent 82 >/dev/null 2>&1   # unlock
adb shell settings put secure immersive_mode_confirmations confirmed   # no first-run fullscreen cling
adb install -r "$APK" || exit 1
adb logcat -c
adb shell am start -W -n com.indycompiled.game/.IndyActivity
t0=$(date +%s)
secs=${INDY_EMU_SECS:-60}; step=${INDY_EMU_INTERVAL:-10}
for ((t = step; t <= secs; t += step)); do
    sleep $step
    adb exec-out screencap -p >"$OUT/shot-${t}s.png"
    adb shell pidof com.indycompiled.game >/dev/null || { echo "exited before ${t}s"; break; }
    [[ ${INDY_EMU_NOKEYS:-0} == 1 ]] || adb shell input keyevent 4   # Back = Escape
done
echo "ran $(( $(date +%s) - t0 )) s"
adb shell pidof com.indycompiled.game >/dev/null && echo "still running" || echo "not running"
adb logcat -d -s SDL SDL/APP SDLActivity AndroidRuntime DEBUG libc >"$OUT/logcat.txt"
for f in stderr.txt JonesLog.txt; do
    adb shell run-as com.indycompiled.game cat "files/Resource/$f" >"$OUT/$f" 2>/dev/null
done
echo "output: $OUT"
