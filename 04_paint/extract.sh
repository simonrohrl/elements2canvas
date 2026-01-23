#!/bin/bash

# Extract paint operations from chrome_debug.log
# Extracts RAW_PAINT_OPS (main document artifact) as raw_paint_ops.json

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
LOG_FILE="$PROJECT_DIR/common/chrome_debug.log"
OUTPUT_DIR="$SCRIPT_DIR/reference"

RAW_PAINT_OPS_FILE="$OUTPUT_DIR/paint.json"

if [ ! -f "$LOG_FILE" ]; then
    echo "Error: $LOG_FILE not found"
    exit 1
fi

mkdir -p "$OUTPUT_DIR"

# Delete old files to start fresh
rm -f "$RAW_PAINT_OPS_FILE"
echo "Extracting paint operations from $LOG_FILE"

python3 - "$LOG_FILE" "$RAW_PAINT_OPS_FILE" << 'PYTHON_SCRIPT'
import json
import sys

log_file = sys.argv[1]
raw_paint_ops_file = sys.argv[2]

with open(log_file, 'r', errors='ignore') as f:
    content = f.read()

# Extract RAW_PAINT_OPS JSON - find ALL entries and pick the main document
# Multiple entries exist due to: SVG images, multiple paint passes, etc.
# Strategy: deduplicate by artifact_id, then pick the 'document' type with most paint ops
raw_paint_ops_entries = []
search_pos = 0
while True:
    start = content.find('RAW_PAINT_OPS: ', search_pos)
    if start == -1:
        break
    json_start = start + len('RAW_PAINT_OPS: ')
    line_end = content.find('\n', json_start)
    if line_end == -1:
        line_end = len(content)
    json_str = content[json_start:line_end].strip()
    try:
        data = json.loads(json_str)
        raw_paint_ops_entries.append({
            'artifact_id': data.get('artifact_id', -1),
            'artifact_type': data.get('artifact_type', 'unknown'),
            'paint_op_count': data.get('paint_op_count', len(data.get('paint_ops', []))),
            'data': data
        })
    except json.JSONDecodeError:
        pass
    search_pos = line_end

if raw_paint_ops_entries:
    # Deduplicate by artifact_id - keep entry with most paint ops for each id
    artifact_map = {}
    for entry in raw_paint_ops_entries:
        aid = entry['artifact_id']
        if aid not in artifact_map or entry['paint_op_count'] > artifact_map[aid]['paint_op_count']:
            artifact_map[aid] = entry

    print(f"  Found {len(raw_paint_ops_entries)} RAW_PAINT_OPS entries, {len(artifact_map)} unique artifacts:")
    for entry in artifact_map.values():
        print(f"    - artifact_id={entry['artifact_id']}, type={entry['artifact_type']}, ops={entry['paint_op_count']}")

    # Find main document: first 'document' type with >= 500 paint ops, else largest
    MIN_OPS_THRESHOLD = 500
    # Get unique document artifacts in order of first appearance
    seen_ids = set()
    document_entries = []
    for e in raw_paint_ops_entries:
        if e['artifact_type'] == 'document' and e['artifact_id'] not in seen_ids:
            seen_ids.add(e['artifact_id'])
            # Use deduplicated version (with max paint ops for this id)
            document_entries.append(artifact_map[e['artifact_id']])
    # Pick first document above threshold
    main_doc = None
    for entry in document_entries:
        if entry['paint_op_count'] >= MIN_OPS_THRESHOLD:
            main_doc = entry
            break
    # Fallback: largest document if none above threshold
    if not main_doc and document_entries:
        main_doc = max(document_entries, key=lambda e: e['paint_op_count'])
    if main_doc:
        print(f"  Using main document: artifact_id={main_doc['artifact_id']} with {main_doc['paint_op_count']} paint ops")
        with open(raw_paint_ops_file, 'w') as out:
            json.dump(main_doc['data'], out, indent=3)
        print(f"  Extracted {raw_paint_ops_file}")
    else:
        # Fallback: use largest artifact
        main = max(artifact_map.values(), key=lambda e: e['paint_op_count'])
        print(f"  Warning: No document type found, using largest: artifact_id={main['artifact_id']}")
        with open(raw_paint_ops_file, 'w') as out:
            json.dump(main['data'], out, indent=3)
        print(f"  Extracted {raw_paint_ops_file}")
else:
    print("  Warning: RAW_PAINT_OPS not found in log")
PYTHON_SCRIPT
