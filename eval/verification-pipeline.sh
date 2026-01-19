#!/bin/bash

# Verification Pipeline Script
# Builds Chromium, runs browser, extracts JSON logs

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Duration to run browser (in seconds)
BROWSER_DURATION=${1:-8}

echo "=== Chromium DOM Tracing Verification Pipeline ==="
echo ""

# Step 1: Build Chromium
echo "[1/4] Building Chromium..."
"$PROJECT_DIR/scripts/build.sh"
echo "Build completed."
echo ""

# Step 2: Start browser in background
echo "[2/4] Starting Chromium for ${BROWSER_DURATION} seconds..."
"$PROJECT_DIR/scripts/start.sh" &
CHROMIUM_PID=$!

# Wait for specified duration
sleep "$BROWSER_DURATION"

# Kill Chromium
echo "Stopping Chromium..."
pkill -f "chromium"
echo ""

# Step 3: Copy debug log
echo "[3/4] Copying debug log..."
"$PROJECT_DIR/eval/copy_debug_log.sh"
echo ""

# Step 4: Extract JSON data
echo "[4/4] Extracting JSON data..."
"$PROJECT_DIR/eval/extract_json.sh"
echo ""

echo "=== Pipeline Complete ==="
echo "Open test.html in a browser to visualize the layer tree."
