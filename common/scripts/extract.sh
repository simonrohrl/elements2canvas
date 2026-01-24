#!/bin/bash

# Extract all JSON data from chrome_debug.log by calling individual extract scripts

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/../.." && pwd)"

echo "Extracting all data from chrome_debug.log..."
echo ""

echo "=== 01_layout ==="
"$PROJECT_DIR/01_layout/extract.sh"
echo ""

echo "=== 02_shape ==="
"$PROJECT_DIR/02_shape/extract.sh"
echo ""

echo "=== 03_layer ==="
"$PROJECT_DIR/03_layer/extract.sh"
echo ""

echo "=== 04_paint ==="
"$PROJECT_DIR/04_paint/extract.sh"
echo ""

echo "Done."
