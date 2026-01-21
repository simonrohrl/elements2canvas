#!/usr/bin/env python3
"""Extract LayoutBlock data and create block_painter input files.

Transforms LayoutBlock nodes from layout_tree.json into block_painter input format
by matching with corresponding DrawRectOp/DrawRRectOp from raw_paint_ops.json using geometry.
"""
import json
import sys
import os


def find_layout_blocks(layout_tree):
    """Find all LayoutBlock nodes with background color."""
    blocks = []
    for node in layout_tree:
        # Must have geometry
        if 'geometry' not in node:
            continue
        # Must have background color
        if 'background_color' not in node:
            continue
        # Skip text nodes
        if 'text' in node:
            continue
        blocks.append(node)
    return blocks


def geometry_matches(block_geom, op_rect, tolerance=1.0):
    """Check if block geometry matches paint op rect within tolerance.

    block_geom: {x, y, width, height}
    op_rect: [left, top, right, bottom]
    """
    left = block_geom['x']
    top = block_geom['y']
    right = block_geom['x'] + block_geom['width']
    bottom = block_geom['y'] + block_geom['height']

    return (abs(left - op_rect[0]) <= tolerance and
            abs(top - op_rect[1]) <= tolerance and
            abs(right - op_rect[2]) <= tolerance and
            abs(bottom - op_rect[3]) <= tolerance)


def color_matches(block_color, op_flags, tolerance=0.01):
    """Check if colors match within tolerance."""
    return (abs(block_color['r'] - op_flags.get('r', 0)) <= tolerance and
            abs(block_color['g'] - op_flags.get('g', 0)) <= tolerance and
            abs(block_color['b'] - op_flags.get('b', 0)) <= tolerance and
            abs(block_color['a'] - op_flags.get('a', 1)) <= tolerance)


def radii_matches(block_radii, op_radii):
    """Check if border radii match."""
    if block_radii is None and op_radii is None:
        return True
    if block_radii is None or op_radii is None:
        return False
    if len(block_radii) != len(op_radii):
        return False
    for br, opr in zip(block_radii, op_radii):
        if abs(br - opr) > 1.0:
            return False
    return True


def match_with_raw_ops(layout_block, raw_ops, used_indices):
    """Match LayoutBlock with corresponding DrawRectOp/DrawRRectOp.

    Args:
        layout_block: LayoutBlock node from layout_tree.json
        raw_ops: List of paint ops from raw_paint_ops.json
        used_indices: Set of already-used op indices (modified in place)

    Returns:
        Tuple of (matched_op, op_index) or (None, -1) if no match
    """
    block_geom = layout_block.get('geometry')
    block_color = layout_block.get('background_color')
    block_radii = layout_block.get('border_radii')

    if not block_geom or not block_color:
        return None, -1

    for idx, op in enumerate(raw_ops):
        # Skip already-used ops
        if idx in used_indices:
            continue

        # Only match DrawRectOp and DrawRRectOp
        op_type = op.get('type')
        if op_type not in ('DrawRectOp', 'DrawRRectOp'):
            continue

        op_rect = op.get('rect')
        op_flags = op.get('flags', {})
        op_radii = op.get('radii')

        if not op_rect:
            continue

        # Check geometry match
        if not geometry_matches(block_geom, op_rect):
            continue

        # Check color match
        if not color_matches(block_color, op_flags):
            continue

        # Check radii match
        if block_radii and op_type == 'DrawRRectOp':
            if not radii_matches(block_radii, op_radii):
                continue
        elif block_radii and op_type == 'DrawRectOp':
            # Block has radii but op doesn't - skip
            continue
        elif not block_radii and op_type == 'DrawRRectOp':
            # Block has no radii but op does - skip
            continue

        return op, idx

    return None, -1


def create_block_painter_input(layout_block, matched_op):
    """Create block_painter input JSON from LayoutBlock and matched op."""
    geom = layout_block['geometry']
    bg_color = layout_block['background_color']

    result = {
        "geometry": {
            "x": geom['x'],
            "y": geom['y'],
            "width": geom['width'],
            "height": geom['height']
        },
        "background_color": {
            "r": bg_color['r'],
            "g": bg_color['g'],
            "b": bg_color['b'],
            "a": bg_color['a']
        },
        "visibility": "visible",
        "node_id": layout_block.get('id', 0),
        "state_ids": {
            "transform_id": matched_op.get('transform_id', 0),
            "clip_id": matched_op.get('clip_id', 0),
            "effect_id": matched_op.get('effect_id', 0)
        }
    }

    # Add border_radii if present
    if 'border_radii' in layout_block:
        result['border_radii'] = layout_block['border_radii']

    # Add box_shadow if present
    if 'box_shadow' in layout_block:
        result['box_shadow'] = layout_block['box_shadow']

    return result


def main():
    if len(sys.argv) < 4:
        print("Usage: extract_block_inputs.py <layout_tree.json> <raw_paint_ops.json> <output_dir>")
        sys.exit(1)

    layout_tree_file = sys.argv[1]
    raw_ops_file = sys.argv[2]
    output_dir = sys.argv[3]

    with open(layout_tree_file) as f:
        layout_tree = json.load(f)['layout_tree']

    with open(raw_ops_file) as f:
        raw_ops = json.load(f)['paint_ops']

    os.makedirs(output_dir, exist_ok=True)

    layout_blocks = find_layout_blocks(layout_tree)
    print(f"  Found {len(layout_blocks)} LayoutBlock nodes with background color")

    # Count rect/rrect ops in raw_paint_ops
    rect_ops = [op for op in raw_ops if op.get('type') in ('DrawRectOp', 'DrawRRectOp')]
    print(f"  Found {len(rect_ops)} DrawRectOp/DrawRRectOp in raw_paint_ops.json")

    # Track used raw_ops indices
    used_indices = set()
    matched_count = 0

    for i, block in enumerate(layout_blocks):
        matched_op, op_idx = match_with_raw_ops(block, raw_ops, used_indices)
        if matched_op:
            used_indices.add(op_idx)
            input_data = create_block_painter_input(block, matched_op)
            output_file = os.path.join(output_dir, f"input_{i:03d}.json")
            with open(output_file, 'w') as f:
                json.dump(input_data, f, indent=2)
            name = block.get('name', 'unknown')[:40]
            print(f"  Created {os.path.basename(output_file)} for '{name}'")
            matched_count += 1
        else:
            name = block.get('name', 'unknown')[:40]
            # print(f"  Warning: No match for '{name}'")

    print(f"  Matched {matched_count}/{len(layout_blocks)} LayoutBlock nodes")


if __name__ == '__main__':
    main()
