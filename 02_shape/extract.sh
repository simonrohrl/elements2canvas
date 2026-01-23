#!/bin/bash

# Extract shaped layout data from chrome_debug.log
# Extracts the largest LAYOUT_TREE entry (with text shaping data) as shape.json

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
LOG_FILE="$PROJECT_DIR/common/chrome_debug.log"
OUTPUT_DIR="$SCRIPT_DIR/reference"

SHAPE_FILE="$OUTPUT_DIR/shape.json"

if [ ! -f "$LOG_FILE" ]; then
    echo "Error: $LOG_FILE not found"
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

# Delete old files to start fresh
rm -f "$SHAPE_FILE"
echo "Extracting shaped layout data from $LOG_FILE"

python3 - "$LOG_FILE" "$SHAPE_FILE" << 'PYTHON_SCRIPT'
import json
import sys

log_file = sys.argv[1]
shape_file = sys.argv[2]

with open(log_file, 'r', errors='ignore') as f:
    content = f.read()

# Extract ALL LAYOUT_TREE entries and pick the largest one (most complete with shaping data)
layout_tree_entries = []
search_pos = 0
while True:
    start = content.find('LAYOUT_TREE: ', search_pos)
    if start == -1:
        break
    json_start = start + len('LAYOUT_TREE: ')
    line_end = content.find('\n', json_start)
    if line_end == -1:
        line_end = len(content)
    json_str = content[json_start:line_end].strip()
    try:
        data = json.loads(json_str)
        layout_tree_entries.append({
            'size': len(json_str),
            'data': data
        })
    except json.JSONDecodeError:
        pass
    search_pos = line_end

if layout_tree_entries:
    print(f"  Found {len(layout_tree_entries)} LAYOUT_TREE entries")

    # Pick the largest entry (most complete with text shaping data)
    largest = max(layout_tree_entries, key=lambda e: e['size'])

    print(f"  Using largest entry: {largest['size']} bytes")
    with open(shape_file, 'w') as out:
        json.dump(largest['data'], out, indent=3)
    print(f"  Extracted {shape_file}")
else:
    print("  Warning: LAYOUT_TREE not found in log")
PYTHON_SCRIPT
