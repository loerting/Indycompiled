#!/usr/bin/env bash
# Copy a build's Jones3D.exe + Jones3D.dll next to Indy3D.exe v1.2 in game/run/Resource/ (PROJECT.md §5.5).
# Usage: Scripts/indy/deploy.sh [preset=mingw-dx9-debug]
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
preset="${1:-mingw-dx9-debug}"
out="$ROOT/Build/$preset/Jones3D"
dest="$ROOT/game/run/Resource"
[[ -d "$dest" ]] || { echo "run folder missing: run Scripts/indy/setup.sh" >&2; exit 1; }
for f in Jones3D.exe Jones3D.dll; do
    [[ -f "$out/$f" ]] || { echo "not built: $out/$f (cmake --build --preset $preset)" >&2; exit 1; }
    cp "$out/$f" "$dest/$f"
done
echo "deployed $preset to $dest"
