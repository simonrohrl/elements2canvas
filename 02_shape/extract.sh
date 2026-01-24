#!/bin/bash

# Extract shape data from chrome_debug.log
# Extracts SHAPE as shape.json

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
echo "Extracting shape data from $LOG_FILE"

python3 - "$LOG_FILE" "$SHAPE_FILE" << 'PYTHON_SCRIPT'
import json
import sys

log_file = sys.argv[1]
shape_file = sys.argv[2]

with open(log_file, 'r', errors='ignore') as f:
    content = f.read()

# Extract SHAPE JSON
shape_entries = []
search_pos = 0
while True:
    start = content.find('SHAPE: ', search_pos)
    if start == -1:
        break
    json_start = start + len('SHAPE: ')
    line_end = content.find('\n', json_start)
    if line_end == -1:
        line_end = len(content)
    json_str = content[json_start:line_end].strip()
    try:
        data = json.loads(json_str)
        shape_entries.append({
            'size': len(json_str),
            'data': data
        })
    except json.JSONDecodeError:
        pass
    search_pos = line_end

if shape_entries:
    print(f"  Found {len(shape_entries)} SHAPE entries")

    # Pick the largest entry (most complete with text shaping data)
    largest = max(shape_entries, key=lambda e: e['size'])

    print(f"  Using largest entry: {largest['size']} bytes")
    with open(shape_file, 'w') as out:
        json.dump(largest['data'], out, indent=3)
    print(f"  Extracted {shape_file}")
else:
    print("  Warning: SHAPE not found in log")
PYTHON_SCRIPT
