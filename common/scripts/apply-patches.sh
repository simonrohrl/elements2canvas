#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PATCHES_DIR="$SCRIPT_DIR/../patches"
CHROMIUM_SRC="./chromium/src"

if [ ! -d "$CHROMIUM_SRC" ]; then
    echo "Error: Chromium source not found at $CHROMIUM_SRC"
    exit 1
fi

echo "Applying patches to $CHROMIUM_SRC..."
for f in "$PATCHES_DIR"/*.patch; do
    if [ -f "$f" ]; then
        patch_name="$(basename "$f")"
        # Check if patch is already applied by testing if it can be reversed
        if git -C "$CHROMIUM_SRC" apply --reverse --check "$f" 2>/dev/null; then
            echo "Skipping $patch_name (already applied)"
        else
            echo "Applying $patch_name"
            git -C "$CHROMIUM_SRC" apply "$f"
        fi
    fi
done

echo "Successfully applied patches"