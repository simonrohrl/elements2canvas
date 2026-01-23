#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ROOT_DIR="$SCRIPT_DIR/../.."
CHROMIUM_SRC="./chromium/src"

if [ ! -d "$CHROMIUM_SRC" ]; then
    echo "Error: Chromium source not found at $CHROMIUM_SRC"
    exit 1
fi

# List of patches to apply in order
PATCHES=(
    "$ROOT_DIR/01_layout/patch/layout.patch"
    "$ROOT_DIR/04_paint/patch/paint.patch"
    "$ROOT_DIR/common/patch/build.patch"
    "$ROOT_DIR/common/patch/cli-flags.patch"
)

echo "Applying patches to $CHROMIUM_SRC..."
for f in "${PATCHES[@]}"; do
    if [ -f "$f" ]; then
        patch_name="$(basename "$f")"
        # Check if patch is already applied by testing if it can be reversed
        if git -C "$CHROMIUM_SRC" apply --reverse --check "$f" 2>/dev/null; then
            echo "Skipping $patch_name (already applied)"
        else
            echo "Applying $patch_name"
            git -C "$CHROMIUM_SRC" apply "$f"
        fi
    else
        echo "Warning: Patch not found: $f"
    fi
done

echo "Successfully applied patches"