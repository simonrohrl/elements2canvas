#!/bin/bash

# Extract layout data from chrome_debug.log
# Extracts first LAYOUT_TREE entry as layout.json and PROPERTY_TREES as property_trees.json

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
LOG_FILE="$PROJECT_DIR/common/chrome_debug.log"
OUTPUT_DIR="$SCRIPT_DIR/reference"

LAYOUT_FILE="$OUTPUT_DIR/layout.json"
PROPERTY_TREES_FILE="$OUTPUT_DIR/properties.json"

if [ ! -f "$LOG_FILE" ]; then
    echo "Error: $LOG_FILE not found"
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

# Delete old files to start fresh
rm -f "$LAYOUT_FILE" "$PROPERTY_TREES_FILE"
echo "Extracting layout data from $LOG_FILE"

python3 - "$LOG_FILE" "$LAYOUT_FILE" "$PROPERTY_TREES_FILE" << 'PYTHON_SCRIPT'
import json
import sys

log_file = sys.argv[1]
layout_file = sys.argv[2]
property_trees_file = sys.argv[3]

with open(log_file, 'r', errors='ignore') as f:
    content = f.read()

# Extract FIRST LAYOUT_TREE JSON (pre-shape layout tree)
# Use the first entry which is the layout before text shaping
layout_tree_start = content.find('LAYOUT_TREE: ')
if layout_tree_start != -1:
    json_start = layout_tree_start + len('LAYOUT_TREE: ')
    line_end = content.find('\n', json_start)
    if line_end == -1:
        line_end = len(content)
    layout_tree_json_str = content[json_start:line_end].strip()
    try:
        data = json.loads(layout_tree_json_str)
        with open(layout_file, 'w') as out:
            json.dump(data, out, indent=2)
        print(f"  Extracted {layout_file}")
    except json.JSONDecodeError as e:
        print(f"  Warning: Failed to parse LAYOUT_TREE JSON: {e}")
else:
    print("  Warning: LAYOUT_TREE not found in log")

# Extract FIRST PROPERTY_TREES JSON (transform, clip, effect trees)
property_trees_start = content.find('PROPERTY_TREES: ')
if property_trees_start != -1:
    json_start = property_trees_start + len('PROPERTY_TREES: ')
    line_end = content.find('\n', json_start)
    if line_end == -1:
        line_end = len(content)
    property_trees_json_str = content[json_start:line_end].strip()
    try:
        data = json.loads(property_trees_json_str)
        with open(property_trees_file, 'w') as out:
            json.dump(data, out, indent=2)
        print(f"  Extracted {property_trees_file}")
    except json.JSONDecodeError as e:
        print(f"  Warning: Failed to parse PROPERTY_TREES JSON: {e}")
else:
    print("  Warning: PROPERTY_TREES not found in log")
PYTHON_SCRIPT
