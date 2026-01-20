#!/bin/bash

# Extract JSON data from chrome_debug.log
# Extracts property_trees, LayerTreeImpl, and Blink-level data to separate JSON files

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
LOG_FILE="$PROJECT_DIR/chrome_debug.log"
PROPERTY_TREES_FILE="$PROJECT_DIR/property_trees.json"
LAYERS_FILE="$PROJECT_DIR/layers.json"
BLINK_LAYERS_FILE="$PROJECT_DIR/blink_layers.json"
BLINK_PROPERTY_TREES_FILE="$PROJECT_DIR/blink_property_trees.json"
PAINT_ARTIFACT_FILE="$PROJECT_DIR/paint_artifact.json"
PAINT_ARTIFACT_PROPERTY_TREES_FILE="$PROJECT_DIR/paint_artifact_property_trees.json"
RAW_PAINT_OPS_FILE="$PROJECT_DIR/raw_paint_ops.json"

if [ ! -f "$LOG_FILE" ]; then
    echo "Error: $LOG_FILE not found"
    exit 1
fi

# delete all files to start fresh
rm -f "$PROPERTY_TREES_FILE" "$LAYERS_FILE" "$BLINK_LAYERS_FILE" "$BLINK_PROPERTY_TREES_FILE" "$PAINT_ARTIFACT_FILE" "$PAINT_ARTIFACT_PROPERTY_TREES_FILE" "$RAW_PAINT_OPS_FILE"
echo "Deleted all files to start fresh"

python3 - "$LOG_FILE" "$PROPERTY_TREES_FILE" "$LAYERS_FILE" "$BLINK_LAYERS_FILE" "$BLINK_PROPERTY_TREES_FILE" "$PAINT_ARTIFACT_FILE" "$PAINT_ARTIFACT_PROPERTY_TREES_FILE" "$RAW_PAINT_OPS_FILE" << 'PYTHON_SCRIPT'
import json
import re
import sys

log_file = sys.argv[1]
property_trees_file = sys.argv[2]
layers_file = sys.argv[3]
blink_layers_file = sys.argv[4]
blink_property_trees_file = sys.argv[5]
paint_artifact_file = sys.argv[6]
paint_artifact_property_trees_file = sys.argv[7]
raw_paint_ops_file = sys.argv[8]

with open(log_file, 'r', errors='ignore') as f:
    content = f.read()

# Extract property_trees JSON (CC level)
# Find "property_trees:" followed by a JSON object
property_trees_match = re.search(r'property_trees:\s*(\{[\s\S]*?\n\})\s*(?:\n|$)', content)
if property_trees_match:
    try:
        json_str = property_trees_match.group(1)
        data = json.loads(json_str)
        with open(property_trees_file, 'w') as out:
            json.dump(data, out, indent=3)
        print(f"  Extracted {property_trees_file}")
    except json.JSONDecodeError as e:
        print(f"  Warning: Failed to parse property_trees JSON: {e}")
else:
    print("  Warning: property_trees not found in log")

# Extract LayerTreeImpl JSON (CC level)
# Look for "cc::LayerImpls:" followed by JSON with "LayerTreeImpl"
layers_match = re.search(r'cc::LayerImpls:\s*(\{\s*"LayerTreeImpl"[\s\S]*?\n\})\s*(?:\n|$)', content)
if not layers_match:
    # Alternative: just find the LayerTreeImpl object directly
    layers_match = re.search(r'(\{\s*"LayerTreeImpl"\s*:\s*\[[\s\S]*?\n\s*\][\s\S]*?\n\})', content)

if layers_match:
    try:
        json_str = layers_match.group(1)
        data = json.loads(json_str)
        with open(layers_file, 'w') as out:
            json.dump(data, out, indent=3)
        print(f"  Extracted {layers_file}")
    except json.JSONDecodeError as e:
        print(f"  Warning: Failed to parse layers JSON: {e}")
else:
    print("  Warning: LayerTreeImpl not found in log")

# Extract BLINK_LAYERS JSON (Blink level - after layerization)
# The format is: BLINK_LAYERS: {"BlinkLayers": [...]}
# The JSON is on a single line, so we find the start and parse to end of line
blink_layers_match = None
blink_layers_start = content.find('BLINK_LAYERS: ')
if blink_layers_start != -1:
    json_start = blink_layers_start + len('BLINK_LAYERS: ')
    line_end = content.find('\n', json_start)
    if line_end == -1:
        line_end = len(content)
    blink_layers_json_str = content[json_start:line_end].strip()
    # Create a fake match object
    class FakeMatch:
        def __init__(self, s):
            self._s = s
        def group(self, n):
            return self._s
    blink_layers_match = FakeMatch(blink_layers_json_str)
if blink_layers_match:
    try:
        json_str = blink_layers_match.group(1)
        data = json.loads(json_str)
        with open(blink_layers_file, 'w') as out:
            json.dump(data, out, indent=3)
        print(f"  Extracted {blink_layers_file}")
    except json.JSONDecodeError as e:
        print(f"  Warning: Failed to parse BLINK_LAYERS JSON: {e}")
else:
    print("  Warning: BLINK_LAYERS not found in log")

# Extract BLINK_PROPERTY_TREES JSON (Blink level)
# The format is: BLINK_PROPERTY_TREES: {"transform_tree": ..., "clip_tree": ..., "effect_tree": ...}
# The JSON is on a single line, so we find the start and parse to end of line
blink_props_match = None
blink_props_start = content.find('BLINK_PROPERTY_TREES: ')
if blink_props_start != -1:
    json_start = blink_props_start + len('BLINK_PROPERTY_TREES: ')
    line_end = content.find('\n', json_start)
    if line_end == -1:
        line_end = len(content)
    blink_props_json_str = content[json_start:line_end].strip()
    blink_props_match = FakeMatch(blink_props_json_str)
if blink_props_match:
    try:
        json_str = blink_props_match.group(1)
        data = json.loads(json_str)
        with open(blink_property_trees_file, 'w') as out:
            json.dump(data, out, indent=3)
        print(f"  Extracted {blink_property_trees_file}")
    except json.JSONDecodeError as e:
        print(f"  Warning: Failed to parse BLINK_PROPERTY_TREES JSON: {e}")
else:
    print("  Warning: BLINK_PROPERTY_TREES not found in log")

# Extract PAINT_ARTIFACT JSON (raw PaintArtifact before layerization)
# The format is: PAINT_ARTIFACT: {"PaintChunks": [...]}
# The JSON is on a single line
paint_artifact_match = None
paint_artifact_start = content.find('PAINT_ARTIFACT: ')
if paint_artifact_start != -1:
    json_start = paint_artifact_start + len('PAINT_ARTIFACT: ')
    line_end = content.find('\n', json_start)
    if line_end == -1:
        line_end = len(content)
    paint_artifact_json_str = content[json_start:line_end].strip()
    paint_artifact_match = FakeMatch(paint_artifact_json_str)
if paint_artifact_match:
    try:
        json_str = paint_artifact_match.group(1)
        data = json.loads(json_str)
        with open(paint_artifact_file, 'w') as out:
            json.dump(data, out, indent=3)
        print(f"  Extracted {paint_artifact_file}")
    except json.JSONDecodeError as e:
        print(f"  Warning: Failed to parse PAINT_ARTIFACT JSON: {e}")
else:
    print("  Warning: PAINT_ARTIFACT not found in log")

# Extract PAINT_ARTIFACT_PROPERTY_TREES JSON (property trees from raw artifact)
# The format is: PAINT_ARTIFACT_PROPERTY_TREES: {"transform_tree": ..., ...}
# The JSON is on a single line
paint_artifact_props_match = None
paint_artifact_props_start = content.find('PAINT_ARTIFACT_PROPERTY_TREES: ')
if paint_artifact_props_start != -1:
    json_start = paint_artifact_props_start + len('PAINT_ARTIFACT_PROPERTY_TREES: ')
    line_end = content.find('\n', json_start)
    if line_end == -1:
        line_end = len(content)
    paint_artifact_props_json_str = content[json_start:line_end].strip()
    paint_artifact_props_match = FakeMatch(paint_artifact_props_json_str)
if paint_artifact_props_match:
    try:
        json_str = paint_artifact_props_match.group(1)
        data = json.loads(json_str)
        with open(paint_artifact_property_trees_file, 'w') as out:
            json.dump(data, out, indent=3)
        print(f"  Extracted {paint_artifact_property_trees_file}")
    except json.JSONDecodeError as e:
        print(f"  Warning: Failed to parse PAINT_ARTIFACT_PROPERTY_TREES JSON: {e}")
else:
    print("  Warning: PAINT_ARTIFACT_PROPERTY_TREES not found in log")

# Extract RAW_PAINT_OPS JSON (flat list of paint ops without chunk grouping)
# The format is: RAW_PAINT_OPS: {"paint_ops": [...]}
# The JSON is on a single line
raw_paint_ops_match = None
raw_paint_ops_start = content.find('RAW_PAINT_OPS: ')
if raw_paint_ops_start != -1:
    json_start = raw_paint_ops_start + len('RAW_PAINT_OPS: ')
    line_end = content.find('\n', json_start)
    if line_end == -1:
        line_end = len(content)
    raw_paint_ops_json_str = content[json_start:line_end].strip()
    raw_paint_ops_match = FakeMatch(raw_paint_ops_json_str)
if raw_paint_ops_match:
    try:
        json_str = raw_paint_ops_match.group(1)
        data = json.loads(json_str)
        with open(raw_paint_ops_file, 'w') as out:
            json.dump(data, out, indent=3)
        print(f"  Extracted {raw_paint_ops_file}")
    except json.JSONDecodeError as e:
        print(f"  Warning: Failed to parse RAW_PAINT_OPS JSON: {e}")
else:
    print("  Warning: RAW_PAINT_OPS not found in log")
PYTHON_SCRIPT
