#!/usr/bin/env bash
# Headless emulator test of the Android build: boot the x86_64 AVD without window or audio, install, start,
# screenshots at ~3 s and ~10 s, tap (the demo exits), logcat, kill the emulator. No real phone involved.
# Usage: Scripts/android/test_emulator.sh [apk] [output dir]
#   Defaults: android/app/build/outputs/apk/debug/app-debug.apk, game/screens/android-<timestamp>/
set -uo pipefail
cd "$(dirname "$0")/../.."
APK=${1:-android/app/build/outputs/apk/debug/app-debug.apk}
OUT=${2:-game/screens/android-$(date +%Y%m%d-%H%M)}
SDK=$HOME/Android/Sdk
adb() { "$SDK/platform-tools/adb" "$@"; }
AVD=Pixel_3a_API_34_extension_level_7_x86_64
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
sleep 3; adb exec-out screencap -p >"$OUT/demo_03s.png"
sleep 7; adb exec-out screencap -p >"$OUT/demo_10s.png"
echo "screens taken at +3/+$(( $(date +%s) - t0 )) s"
adb shell input tap 500 500; sleep 2
adb shell pidof com.indycompiled.game && echo "still running after tap" || echo "exited after tap"
adb logcat -d -s SDL SDL/APP SDLActivity AndroidRuntime DEBUG libc >"$OUT/logcat.txt"
