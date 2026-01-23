#!/bin/bash

# Extract layer data from chrome_debug.log
# Extracts LAYER_TREE as layer_tree.json and PAINT_ORDER as stacking_nodes.json

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
LOG_FILE="$PROJECT_DIR/common/chrome_debug.log"
OUTPUT_DIR="$SCRIPT_DIR/reference"

LAYER_TREE_FILE="$OUTPUT_DIR/layer.json"
STACKING_NODES_FILE="$OUTPUT_DIR/stacking.json"

if [ ! -f "$LOG_FILE" ]; then
    echo "Error: $LOG_FILE not found"
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

# Delete old files to start fresh
rm -f "$LAYER_TREE_FILE" "$STACKING_NODES_FILE"
echo "Extracting layer data from $LOG_FILE"

python3 - "$LOG_FILE" "$LAYER_TREE_FILE" "$STACKING_NODES_FILE" << 'PYTHON_SCRIPT'
import json
import sys

log_file = sys.argv[1]
layer_tree_file = sys.argv[2]
stacking_nodes_file = sys.argv[3]

with open(log_file, 'r', errors='ignore') as f:
    content = f.read()

# Extract LAYER_TREE JSON (PaintLayer tree structure)
# Find the largest entry (most complete)
layer_tree_entries = []
search_pos = 0
while True:
    start = content.find('LAYER_TREE: ', search_pos)
    if start == -1:
        break
    json_start = start + len('LAYER_TREE: ')
    line_end = content.find('\n', json_start)
    if line_end == -1:
        line_end = len(content)
    json_str = content[json_start:line_end].strip()
    try:
        data = json.loads(json_str)
        layer_tree_entries.append({
            'size': len(json_str),
            'data': data
        })
    except json.JSONDecodeError:
        pass
    search_pos = line_end

if layer_tree_entries:
    print(f"  Found {len(layer_tree_entries)} LAYER_TREE entries")
    largest = max(layer_tree_entries, key=lambda e: e['size'])
    print(f"  Using largest entry: {largest['size']} bytes")
    with open(layer_tree_file, 'w') as out:
        json.dump(largest['data'], out, indent=2)
    print(f"  Extracted {layer_tree_file}")
else:
    print("  Warning: LAYER_TREE not found in log")

# Extract PAINT_ORDER JSON (stacking nodes with z-order lists)
# Find the largest entry (most complete)
paint_order_entries = []
search_pos = 0
while True:
    start = content.find('PAINT_ORDER: ', search_pos)
    if start == -1:
        break
    json_start = start + len('PAINT_ORDER: ')
    line_end = content.find('\n', json_start)
    if line_end == -1:
        line_end = len(content)
    json_str = content[json_start:line_end].strip()
    try:
        data = json.loads(json_str)
        paint_order_entries.append({
            'size': len(json_str),
            'data': data
        })
    except json.JSONDecodeError:
        pass
    search_pos = line_end

if paint_order_entries:
    print(f"  Found {len(paint_order_entries)} PAINT_ORDER entries")
    largest = max(paint_order_entries, key=lambda e: e['size'])
    print(f"  Using largest entry: {largest['size']} bytes")
    with open(stacking_nodes_file, 'w') as out:
        json.dump(largest['data'], out, indent=2)
    print(f"  Extracted {stacking_nodes_file}")
else:
    print("  Warning: PAINT_ORDER not found in log")
PYTHON_SCRIPT
