#!/usr/bin/env bash
# Local recipe: copy the owner's game data into the Android build's asset staging dir, so it is packed into
# the APK. Runs automatically before every Gradle build (task :app:packGameData). The staging dir lives
# under android/app/build/ (git-ignored): game data is never committed and the APK is never distributed.
#
# Usage: bash Scripts/android/pack_data.sh <staging dir> [game folder]
#   Game folder: the installed game (with Resource/), or its Resource folder. Default: game/run of this checkout,
#   else of the main checkout (for git worktrees).
#   Packed in the install layout: Resource/ (GOBs and movies, read in place by the game), KeySets/ (control schemes,
#   copied to internal storage on first start), and the list of it all, indy_assets.txt (Libs/std/SDL/stdAndroidSDL.c).
#   Not packed: the Windows setup (Install/) and the user's Jones.cfg (the game creates its own).
#   The staging dir is mirrored (stale files are removed).
set -euo pipefail

dest=${1:?usage: pack_data.sh <staging dir> [game folder]}
src=${2:-}
here=$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)
if [[ -z $src ]]; then
    src=$here/game/run
    if [[ ! -d $src ]]; then
        common=$(git -C "$here" rev-parse --path-format=absolute --git-common-dir 2>/dev/null || true)
        [[ -n $common ]] && src=$(dirname "$common")/game/run
    fi
fi
[[ -d $src && ! -d $src/Resource && $(basename "$src") == Resource ]] && src=$(dirname "$src")

mkdir -p "$dest"
if [[ ! -d $src/Resource ]]; then
    echo "pack_data: no game data at '$src': building without it" >&2
    find "$dest" -mindepth 1 -delete
    exit 0
fi

# -rt only: the checkout may be on exFAT (no permissions). -L: the game folder may hold links to the data.
rsync -rtL --delete --delete-excluded \
    --include='/Resource/' --include='/Resource/*.[gG][oO][bB]' --include='/Resource/*.[sS][nN][mM]' \
    --include='/KeySets/***' --exclude='*' \
    "$src/" "$dest/"
(cd "$dest" && find . -type f ! -name indy_assets.txt | sed 's|^\./||' | sort > indy_assets.txt)
echo "pack_data: $(wc -l < "$dest/indy_assets.txt") file(s), $(du -sh "$dest" | cut -f1) from $src"
