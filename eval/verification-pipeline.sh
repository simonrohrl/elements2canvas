#!/bin/bash

# Verification Pipeline Script
# Builds Chromium, runs browser, extracts JSON logs

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"

# Duration to run browser (in seconds)
BROWSER_DURATION=${1:-8}

echo "=== Text Painter Verification Pipeline ==="
echo ""

# Step 1: Build Chromium
echo "[1/6] Building Chromium..."
  "$PROJECT_DIR/scripts/build.sh"
echo "Build completed."
echo ""

# Step 2: Start browser in background
echo "[2/6] Starting Chromium for ${BROWSER_DURATION} seconds..."
"$PROJECT_DIR/scripts/start.sh" &
CHROMIUM_PID=$!

# Wait for specified duration
sleep "$BROWSER_DURATION"

# Kill Chromium
echo "Stopping Chromium..."
pkill -f "chromium"
echo ""

# Step 3: Copy debug log
echo "[3/6] Copying debug log..."
"$PROJECT_DIR/eval/copy_debug_log.sh"
echo ""

# Step 4: Extract JSON (including property_trees.json)
echo "[4/6] Extracting JSON data..."
"$PROJECT_DIR/eval/extract_json.sh"
echo ""

# Step 5: Transform and run text_painter
echo "[5/6] Running text_painter..."
mkdir -p "$PROJECT_DIR/eval/text_inputs"
rm -f "$PROJECT_DIR/eval/text_inputs"/*.json

python3 "$SCRIPT_DIR/extract_text_inputs.py" \
  "$PROJECT_DIR/layout_tree.json" \
  "$PROJECT_DIR/raw_paint_ops.json" \
  "$PROJECT_DIR/property_trees.json" \
  "$PROJECT_DIR/eval/text_inputs"

# Run text_painter on each input
for input_file in "$PROJECT_DIR/eval/text_inputs"/input_*.json; do
  [ -f "$input_file" ] || continue
  output_file="${input_file/input_/output_}"
  "$PROJECT_DIR/text_painter/text_painter" -i "$input_file" -o "$output_file"
  echo "  Processed $(basename "$input_file") -> $(basename "$output_file")"
done
echo ""

# Step 6: Compare
echo "[6/6] Comparing paint ops..."
python3 "$SCRIPT_DIR/compare_paint_ops.py" \
  "$PROJECT_DIR/raw_paint_ops.json" \
  "$PROJECT_DIR/eval/text_inputs"
echo ""

echo "=== Pipeline Complete ==="
