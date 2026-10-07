#!/usr/bin/env bash
# Local recipe: copy the owner's game data into the Android build's asset staging dir, so it is packed into
# the APK. Runs automatically before every Gradle build (task :app:packGameData). The staging dir lives
# under android/app/build/ (git-ignored): game data is never committed and the APK is never distributed.
#
# Usage: bash Scripts/android/pack_data.sh <staging dir> [game Resource folder]
#   Default folder: game/run/Resource of this checkout, else of the main checkout (for git worktrees).
#   Packed for now: the intro movies (*.snm) only. The staging dir is mirrored (stale files are removed).
set -euo pipefail

dest=${1:?usage: pack_data.sh <staging dir> [game Resource folder]}
src=${2:-}
here=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
if [[ -z $src ]]; then
    src=$here/game/run/Resource
    if [[ ! -d $src ]]; then
        common=$(git -C "$here" rev-parse --path-format=absolute --git-common-dir 2>/dev/null || true)
        [[ -n $common ]] && src=$(dirname "$common")/game/run/Resource
    fi
fi

mkdir -p "$dest"
if [[ ! -d $src ]]; then
    echo "pack_data: no game data at '$src': building without it" >&2
    find "$dest" -mindepth 1 -delete
    exit 0
fi

# -rt only: the checkout may be on exFAT (no permissions)
rsync -rt --delete --delete-excluded --include='*.snm' --include='*.SNM' --exclude='*' "$src/" "$dest/"
echo "pack_data: $(find "$dest" -type f | wc -l) file(s), $(du -sh "$dest" | cut -f1) from $src"
