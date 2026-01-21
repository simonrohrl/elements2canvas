#!/bin/bash

# Extract JSON data from chrome_debug.log
# Extracts RAW_PAINT_OPS, LAYOUT_TREE, LAYER_TREE, STACKING_NODES, PROPERTY_TREES to JSON files

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(cd "$SCRIPT_DIR/.." && pwd)"
LOG_FILE="$PROJECT_DIR/chrome_debug.log"
RAW_PAINT_OPS_FILE="$PROJECT_DIR/raw_paint_ops.json"
LAYOUT_TREE_FILE="$PROJECT_DIR/layout_tree.json"
LAYER_TREE_FILE="$PROJECT_DIR/layer_tree.json"
STACKING_NODES_FILE="$PROJECT_DIR/stacking_nodes.json"
PROPERTY_TREES_FILE="$PROJECT_DIR/property_trees.json"

if [ ! -f "$LOG_FILE" ]; then
    echo "Error: $LOG_FILE not found"
    exit 1
fi

# delete files to start fresh
rm -f "$RAW_PAINT_OPS_FILE" "$LAYOUT_TREE_FILE" "$LAYER_TREE_FILE" "$STACKING_NODES_FILE" "$PROPERTY_TREES_FILE"
echo "Deleted old files to start fresh"

python3 - "$LOG_FILE" "$RAW_PAINT_OPS_FILE" "$LAYOUT_TREE_FILE" "$LAYER_TREE_FILE" "$STACKING_NODES_FILE" "$PROPERTY_TREES_FILE" << 'PYTHON_SCRIPT'
import json
import sys

log_file = sys.argv[1]
raw_paint_ops_file = sys.argv[2]
layout_tree_file = sys.argv[3]
layer_tree_file = sys.argv[4]
stacking_nodes_file = sys.argv[5]
property_trees_file = sys.argv[6]

with open(log_file, 'r', errors='ignore') as f:
    content = f.read()

# Extract RAW_PAINT_OPS JSON (flat list of paint ops without chunk grouping)
# The format is: RAW_PAINT_OPS: {"paint_ops": [...]}
# The JSON is on a single line
raw_paint_ops_start = content.find('RAW_PAINT_OPS: ')
if raw_paint_ops_start != -1:
    json_start = raw_paint_ops_start + len('RAW_PAINT_OPS: ')
    line_end = content.find('\n', json_start)
    if line_end == -1:
        line_end = len(content)
    raw_paint_ops_json_str = content[json_start:line_end].strip()
    try:
        data = json.loads(raw_paint_ops_json_str)
        with open(raw_paint_ops_file, 'w') as out:
            json.dump(data, out, indent=3)
        print(f"  Extracted {raw_paint_ops_file}")
    except json.JSONDecodeError as e:
        print(f"  Warning: Failed to parse RAW_PAINT_OPS JSON: {e}")
else:
    print("  Warning: RAW_PAINT_OPS not found in log")

# Extract LAYOUT_TREE JSON (layout tree in DOM order)
# The format is: LAYOUT_TREE: {"layout_tree": [...]}
# The JSON is on a single line
layout_tree_start = content.find('LAYOUT_TREE: ')
if layout_tree_start != -1:
    json_start = layout_tree_start + len('LAYOUT_TREE: ')
    line_end = content.find('\n', json_start)
    if line_end == -1:
        line_end = len(content)
    layout_tree_json_str = content[json_start:line_end].strip()
    try:
        data = json.loads(layout_tree_json_str)
        with open(layout_tree_file, 'w') as out:
            json.dump(data, out, indent=3)
        print(f"  Extracted {layout_tree_file}")
    except json.JSONDecodeError as e:
        print(f"  Warning: Failed to parse LAYOUT_TREE JSON: {e}")
else:
    print("  Warning: LAYOUT_TREE not found in log")

# Extract LAYER_TREE JSON (PaintLayer tree structure)
# The format is: LAYER_TREE: {"layer_tree": [...]}
# The JSON is on a single line
layer_tree_start = content.find('LAYER_TREE: ')
if layer_tree_start != -1:
    json_start = layer_tree_start + len('LAYER_TREE: ')
    line_end = content.find('\n', json_start)
    if line_end == -1:
        line_end = len(content)
    layer_tree_json_str = content[json_start:line_end].strip()
    try:
        data = json.loads(layer_tree_json_str)
        with open(layer_tree_file, 'w') as out:
            json.dump(data, out, indent=3)
        print(f"  Extracted {layer_tree_file}")
    except json.JSONDecodeError as e:
        print(f"  Warning: Failed to parse LAYER_TREE JSON: {e}")
else:
    print("  Warning: LAYER_TREE not found in log")

# Extract PAINT_ORDER JSON (stacking nodes with z-order lists)
# The format is: PAINT_ORDER: {"stacking_nodes": [...]}
# The JSON is on a single line
paint_order_start = content.find('PAINT_ORDER: ')
if paint_order_start != -1:
    json_start = paint_order_start + len('PAINT_ORDER: ')
    line_end = content.find('\n', json_start)
    if line_end == -1:
        line_end = len(content)
    stacking_nodes_json_str = content[json_start:line_end].strip()
    try:
        data = json.loads(stacking_nodes_json_str)
        with open(stacking_nodes_file, 'w') as out:
            json.dump(data, out, indent=3)
        print(f"  Extracted {stacking_nodes_file}")
    except json.JSONDecodeError as e:
        print(f"  Warning: Failed to parse PAINT_ORDER JSON: {e}")
else:
    print("  Warning: PAINT_ORDER not found in log")

# Extract PROPERTY_TREES JSON (transform, clip, effect trees)
# The format is: PROPERTY_TREES: {"transform_tree": {...}, "clip_tree": {...}, "effect_tree": {...}}
# The JSON is on a single line
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
            json.dump(data, out, indent=3)
        print(f"  Extracted {property_trees_file}")
    except json.JSONDecodeError as e:
        print(f"  Warning: Failed to parse PROPERTY_TREES JSON: {e}")
else:
    print("  Warning: PROPERTY_TREES not found in log")
PYTHON_SCRIPT
