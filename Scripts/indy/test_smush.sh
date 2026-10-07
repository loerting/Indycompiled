#!/usr/bin/env bash
# SMUSH decoder test: builds Scripts/indy/smush_test.c natively together with Libs/smush/smushDecoder.c and
# compares every decoded frame (byte for byte, RGB565) and the decoded audio with FFmpeg's output.
# FFmpeg differs from the original decoder in one known point: its glyph tables leave out the edge line of glyphs
# that fill neither side. A second pass builds the decoder with FFmpeg's rule (SMUSH_TEST_FFMPEG) and fails if any
# frame differs before the first motion copy that reads outside the picture (undefined in the original: it reads
# past its buffer; FFmpeg skips the copy), or if the audio differs.
# Only reads the movies. Usage: Scripts/indy/test_smush.sh [file.snm ...]   (default: game/run/Resource/*.snm)
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
command -v ffmpeg > /dev/null || { echo "ffmpeg not found"; exit 2; }
CC="${CC:-cc}"

tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT
for variant in "" "-DSMUSH_TEST_FFMPEG"; do
    "$CC" -std=c11 -O2 -Wall -Wextra $variant -I"$ROOT/Libs/smush" -o "$tmp/smush_test$variant" \
        "$ROOT/Scripts/indy/smush_test.c" "$ROOT/Libs/smush/smushDecoder.c" || exit 2
done

files=("$@")
if [[ ${#files[@]} -eq 0 ]]; then
    shopt -s nullglob
    files=("$ROOT"/game/run/Resource/*.snm)
fi
[[ ${#files[@]} -gt 0 ]] || { echo "no .snm files given or found in game/run/Resource"; exit 2; }

fail=0
for f in "${files[@]}"; do
    "$tmp/smush_test" "$f" 2> /dev/null || fail=1
    echo "  with FFmpeg's glyph rule:"
    "$tmp/smush_test-DSMUSH_TEST_FFMPEG" "$f" 2> /dev/null > "$tmp/ffrule.txt" || fail=1
    sed -n -e 's/^  \(video\|frame\|first\)/    \1/p' "$tmp/ffrule.txt"
done
[[ $fail -eq 0 ]] && echo "PASS" || echo "FAIL"
exit $fail
