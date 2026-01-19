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

if [ ! -f "$LOG_FILE" ]; then
    echo "Error: $LOG_FILE not found"
    exit 1
fi

python3 - "$LOG_FILE" "$PROPERTY_TREES_FILE" "$LAYERS_FILE" "$BLINK_LAYERS_FILE" "$BLINK_PROPERTY_TREES_FILE" << 'PYTHON_SCRIPT'
import json
import re
import sys

log_file = sys.argv[1]
property_trees_file = sys.argv[2]
layers_file = sys.argv[3]
blink_layers_file = sys.argv[4]
blink_property_trees_file = sys.argv[5]

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
blink_layers_match = re.search(r'BLINK_LAYERS:\s*(\{"BlinkLayers":\s*\[[\s\S]*?\]\})', content)
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
blink_props_match = re.search(r'BLINK_PROPERTY_TREES:\s*(\{"transform_tree":\s*\{[\s\S]*?\}\})', content)
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
PYTHON_SCRIPT
