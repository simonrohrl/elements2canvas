#!/bin/bash

# Extract layer data from chrome_debug.log
# Extracts LAYER as layer.json and STACKING as stacking.json

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
LOG_FILE="$PROJECT_DIR/common/chrome_debug.log"
OUTPUT_DIR="$SCRIPT_DIR/reference"

LAYER_FILE="$OUTPUT_DIR/layer.json"
STACKING_FILE="$OUTPUT_DIR/stacking.json"

if [ ! -f "$LOG_FILE" ]; then
    echo "Error: $LOG_FILE not found"
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

# Delete old files to start fresh
rm -f "$LAYER_FILE" "$STACKING_FILE"
echo "Extracting layer data from $LOG_FILE"

python3 - "$LOG_FILE" "$LAYER_FILE" "$STACKING_FILE" << 'PYTHON_SCRIPT'
import json
import sys

log_file = sys.argv[1]
layer_file = sys.argv[2]
stacking_file = sys.argv[3]

with open(log_file, 'r', errors='ignore') as f:
    content = f.read()

# Extract LAYER JSON (PaintLayer tree structure)
# Find the largest entry (most complete)
layer_entries = []
search_pos = 0
while True:
    start = content.find('LAYER: ', search_pos)
    if start == -1:
        break
    json_start = start + len('LAYER: ')
    line_end = content.find('\n', json_start)
    if line_end == -1:
        line_end = len(content)
    json_str = content[json_start:line_end].strip()
    try:
        data = json.loads(json_str)
        layer_entries.append({
            'size': len(json_str),
            'data': data
        })
    except json.JSONDecodeError:
        pass
    search_pos = line_end

if layer_entries:
    print(f"  Found {len(layer_entries)} LAYER entries")
    largest = max(layer_entries, key=lambda e: e['size'])
    print(f"  Using largest entry: {largest['size']} bytes")
    with open(layer_file, 'w') as out:
        json.dump(largest['data'], out, indent=2)
    print(f"  Extracted {layer_file}")
else:
    print("  Warning: LAYER not found in log")

# Extract STACKING JSON (stacking nodes with z-order lists)
# Find the largest entry (most complete)
stacking_entries = []
search_pos = 0
while True:
    start = content.find('STACKING: ', search_pos)
    if start == -1:
        break
    json_start = start + len('STACKING: ')
    line_end = content.find('\n', json_start)
    if line_end == -1:
        line_end = len(content)
    json_str = content[json_start:line_end].strip()
    try:
        data = json.loads(json_str)
        stacking_entries.append({
            'size': len(json_str),
            'data': data
        })
    except json.JSONDecodeError:
        pass
    search_pos = line_end

if stacking_entries:
    print(f"  Found {len(stacking_entries)} STACKING entries")
    largest = max(stacking_entries, key=lambda e: e['size'])
    print(f"  Using largest entry: {largest['size']} bytes")
    with open(stacking_file, 'w') as out:
        json.dump(largest['data'], out, indent=2)
    print(f"  Extracted {stacking_file}")
else:
    print("  Warning: STACKING not found in log")
PYTHON_SCRIPT
