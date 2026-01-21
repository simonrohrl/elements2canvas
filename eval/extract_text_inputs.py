#!/usr/bin/env python3
"""Extract LayoutText data and create text_painter input files.

Transforms LayoutText nodes from layout_tree.json into text_painter input format
by matching with corresponding DrawTextBlobOp from raw_paint_ops.json using glyph arrays.
"""
import json
import sys
import os


def find_layout_texts(layout_tree):
    """Find all LayoutText nodes with fragments."""
    texts = []
    for node in layout_tree:
        if 'text' in node and 'fragments' in node and node['fragments']:
            # Check that fragments have runs with glyphs
            has_valid_runs = False
            for frag in node['fragments']:
                if 'runs' in frag and frag['runs']:
                    for run in frag['runs']:
                        if 'glyphs' in run and run['glyphs']:
                            has_valid_runs = True
                            break
                if has_valid_runs:
                    break
            if has_valid_runs:
                texts.append(node)
    return texts


def match_with_raw_ops(layout_text, raw_ops, used_indices):
    """Match LayoutText with corresponding DrawTextBlobOp by glyphs.

    Args:
        layout_text: LayoutText node from layout_tree.json
        raw_ops: List of paint ops from raw_paint_ops.json
        used_indices: Set of already-used op indices (modified in place)

    Returns:
        Tuple of (matched_op, op_index) or (None, -1) if no match
    """
    if not layout_text.get('fragments'):
        return None, -1

    for frag in layout_text['fragments']:
        if not frag.get('runs'):
            continue
        for run in frag['runs']:
            if not run.get('glyphs'):
                continue
            lt_glyphs = run['glyphs']

            for idx, op in enumerate(raw_ops):
                # Skip already-used ops to ensure 1:1 matching
                if idx in used_indices:
                    continue
                if op.get('type') != 'DrawTextBlobOp':
                    continue
                if not op.get('runs'):
                    continue
                for op_run in op['runs']:
                    op_glyphs = op_run.get('glyphs', [])
                    if lt_glyphs == op_glyphs:
                        return op, idx
    return None, -1


def create_text_painter_input(layout_text, matched_op, property_trees):
    """Create text_painter input JSON from LayoutText and matched op."""
    frag = layout_text['fragments'][0]
    run = frag['runs'][0]
    op_run = matched_op['runs'][0]

    # Compute color from flags (ARGB format #AARRGGBB)
    flags = matched_op.get('flags', {})
    a = int(flags.get('a', 1) * 255)
    r = int(flags.get('r', 0) * 255)
    g = int(flags.get('g', 0) * 255)
    b = int(flags.get('b', 0) * 255)
    fill_color = f"#{a:02x}{r:02x}{g:02x}{b:02x}"

    # Compute bounds from matched_op
    bounds = matched_op.get('bounds', [0, 0, 0, 0])
    bounds_x = bounds[0] if len(bounds) > 0 else 0
    bounds_y = bounds[1] if len(bounds) > 1 else 0
    bounds_width = (bounds[2] - bounds[0]) if len(bounds) > 2 else frag.get('width', 0)
    bounds_height = (bounds[3] - bounds[1]) if len(bounds) > 3 else frag.get('height', 0)

    return {
        "fragment": {
            "text": layout_text['text'],
            "from": frag.get('start', 0),
            "to": frag.get('end', len(layout_text['text'])),
            "shape_result": {
                "bounds": {
                    "x": bounds_x,
                    "y": bounds_y,
                    "width": bounds_width,
                    "height": bounds_height
                },
                "runs": [{
                    "font": op_run.get('font', {
                        "family": "Arial",
                        "size": 16,
                        "weight": 400,
                        "width": 5,
                        "slant": 0,
                        "scaleX": 1,
                        "skewX": 0,
                        "embolden": False,
                        "linearMetrics": True,
                        "subpixel": True,
                        "forceAutoHinting": False,
                        "typefaceId": 0,
                        "ascent": 14,
                        "descent": 4
                    }),
                    "glyphs": op_run['glyphs'],      # Use matched_op glyphs
                    "positions": op_run['positions'], # Use matched_op positions
                    "offsetX": op_run.get('offsetX', 0),
                    "offsetY": op_run.get('offsetY', 0),
                    "positioning": op_run.get('positioning', 1)  # Use matched_op positioning
                }]
            }
        },
        "box": {
            "x": matched_op.get('x', 0),
            "y": matched_op.get('y', 0),
            "width": frag.get('width', 0),
            "height": frag.get('height', 0)
        },
        "style": {
            "fill_color": fill_color,
            "stroke_color": "#ff000000",
            "stroke_width": flags.get('strokeWidth', 0),
            "emphasis_mark_color": "#ff000000",
            "current_color": fill_color,
            "color_scheme": "light",
            "paint_order": "normal"
        },
        "paint_phase": "foreground",
        "node_id": matched_op.get('nodeId', 0),
        "state_ids": {
            "transform_id": matched_op.get('transform_id', 0),
            "clip_id": matched_op.get('clip_id', 0),
            "effect_id": matched_op.get('effect_id', 0)
        }
    }


def main():
    if len(sys.argv) < 5:
        print("Usage: extract_text_inputs.py <layout_tree.json> <raw_paint_ops.json> <property_trees.json> <output_dir>")
        sys.exit(1)

    layout_tree_file = sys.argv[1]
    raw_ops_file = sys.argv[2]
    property_trees_file = sys.argv[3]
    output_dir = sys.argv[4]

    with open(layout_tree_file) as f:
        layout_tree = json.load(f)['layout_tree']

    with open(raw_ops_file) as f:
        raw_ops = json.load(f)['paint_ops']

    # property_trees is optional - may not exist yet
    property_trees = {}
    if os.path.exists(property_trees_file):
        with open(property_trees_file) as f:
            property_trees = json.load(f)

    os.makedirs(output_dir, exist_ok=True)

    layout_texts = find_layout_texts(layout_tree)
    print(f"  Found {len(layout_texts)} LayoutText nodes with glyph data")

    # Track used raw_ops indices to ensure 1:1 matching
    used_indices = set()
    matched_count = 0

    for i, lt in enumerate(layout_texts):
        matched_op, op_idx = match_with_raw_ops(lt, raw_ops, used_indices)
        if matched_op:
            used_indices.add(op_idx)  # Mark this op as used
            input_data = create_text_painter_input(lt, matched_op, property_trees)
            output_file = os.path.join(output_dir, f"input_{i:03d}.json")
            with open(output_file, 'w') as f:
                json.dump(input_data, f, indent=2)
            text_preview = lt['text'][:30] + '...' if len(lt['text']) > 30 else lt['text']
            print(f"  Created {os.path.basename(output_file)} for '{text_preview}'")
            matched_count += 1
        else:
            text_preview = lt['text'][:30] + '...' if len(lt['text']) > 30 else lt['text']
            print(f"  Warning: No match for '{text_preview}'")

    print(f"  Matched {matched_count}/{len(layout_texts)} LayoutText nodes")


if __name__ == '__main__':
    main()
