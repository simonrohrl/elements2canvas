#!/usr/bin/env python3
"""Extract border data and create border_painter input files.

Transforms nodes from layout_tree.json that have border_widths/border_colors
into border_painter input format by matching with stroked DrawRectOp/DrawRRectOp
from raw_paint_ops.json.
"""
import json
import sys
import os


def find_bordered_nodes(layout_tree):
    """Find all nodes with border_widths and border_colors."""
    bordered = []
    for node in layout_tree:
        # Must have geometry
        if 'geometry' not in node:
            continue
        # Must have border widths and colors
        if 'border_widths' not in node or 'border_colors' not in node:
            continue
        # Skip if all borders are zero
        bw = node['border_widths']
        if bw['top'] == 0 and bw['right'] == 0 and bw['bottom'] == 0 and bw['left'] == 0:
            continue
        bordered.append(node)
    return bordered


def geometry_matches(layout_geom, border_width, op_rect, tolerance=2.0):
    """Check if layout geometry matches paint op rect within tolerance.

    The stroke rect is inset by strokeWidth/2 from the outer edge.
    layout_geom: {x, y, width, height}
    op_rect: [left, top, right, bottom]
    """
    inset = border_width / 2.0
    expected_left = layout_geom['x'] + inset
    expected_top = layout_geom['y'] + inset
    expected_right = layout_geom['x'] + layout_geom['width'] - inset
    expected_bottom = layout_geom['y'] + layout_geom['height'] - inset

    return (abs(expected_left - op_rect[0]) <= tolerance and
            abs(expected_top - op_rect[1]) <= tolerance and
            abs(expected_right - op_rect[2]) <= tolerance and
            abs(expected_bottom - op_rect[3]) <= tolerance)


def color_matches(layout_color, op_flags, tolerance=0.01):
    """Check if colors match within tolerance."""
    return (abs(layout_color['r'] - op_flags.get('r', 0)) <= tolerance and
            abs(layout_color['g'] - op_flags.get('g', 0)) <= tolerance and
            abs(layout_color['b'] - op_flags.get('b', 0)) <= tolerance and
            abs(layout_color['a'] - op_flags.get('a', 1)) <= tolerance)


def radii_matches(layout_radii, stroke_width, op_radii, tolerance=1.0):
    """Check if border radii match after stroke adjustment.

    Adjusted radii = layout_radii - strokeWidth/2
    """
    if layout_radii is None and op_radii is None:
        return True
    if layout_radii is None or op_radii is None:
        return False
    if len(layout_radii) != len(op_radii):
        return False

    adjustment = stroke_width / 2.0
    for lr, opr in zip(layout_radii, op_radii):
        expected = max(0, lr - adjustment)
        if abs(expected - opr) > tolerance:
            return False
    return True


def match_with_raw_ops(layout_node, raw_ops, used_indices):
    """Match bordered layout node with corresponding stroked DrawRectOp/DrawRRectOp.

    Args:
        layout_node: Node from layout_tree.json with border data
        raw_ops: List of paint ops from raw_paint_ops.json
        used_indices: Set of already-used op indices (modified in place)

    Returns:
        Tuple of (matched_op, op_index) or (None, -1) if no match
    """
    layout_geom = layout_node.get('geometry')
    border_widths = layout_node.get('border_widths')
    border_colors = layout_node.get('border_colors')
    border_radii = layout_node.get('border_radii')

    if not layout_geom or not border_widths or not border_colors:
        return None, -1

    # Use top border width (assuming uniform for now)
    stroke_width = border_widths['top']
    border_color = border_colors['top']

    for idx, op in enumerate(raw_ops):
        # Skip already-used ops
        if idx in used_indices:
            continue

        # Only match stroked DrawRectOp and DrawRRectOp
        op_type = op.get('type')
        if op_type not in ('DrawRectOp', 'DrawRRectOp'):
            continue

        op_flags = op.get('flags', {})
        # Must be stroke style (style=1)
        if op_flags.get('style', 0) != 1:
            continue

        # Must have matching stroke width
        if abs(op_flags.get('strokeWidth', 0) - stroke_width) > 0.5:
            continue

        op_rect = op.get('rect')
        if not op_rect:
            continue

        # Check geometry match
        if not geometry_matches(layout_geom, stroke_width, op_rect):
            continue

        # Check color match
        if not color_matches(border_color, op_flags):
            continue

        # Check radii match
        op_radii = op.get('radii')
        if border_radii and op_type == 'DrawRRectOp':
            if not radii_matches(border_radii, stroke_width, op_radii):
                continue
        elif border_radii and op_type == 'DrawRectOp':
            # Node has radii but op doesn't - skip
            continue
        elif not border_radii and op_type == 'DrawRRectOp':
            # Node has no radii but op does - could still match if radii are small
            pass

        return op, idx

    return None, -1


def create_border_painter_input(layout_node, matched_op):
    """Create border_painter input JSON from layout node and matched op."""
    geom = layout_node['geometry']
    border_widths = layout_node['border_widths']
    border_colors = layout_node['border_colors']

    result = {
        "geometry": {
            "x": geom['x'],
            "y": geom['y'],
            "width": geom['width'],
            "height": geom['height']
        },
        "border_widths": {
            "top": border_widths['top'],
            "right": border_widths['right'],
            "bottom": border_widths['bottom'],
            "left": border_widths['left']
        },
        "border_colors": {
            "top": border_colors['top'],
            "right": border_colors['right'],
            "bottom": border_colors['bottom'],
            "left": border_colors['left']
        },
        "visibility": "visible",
        "node_id": layout_node.get('id', 0),
        "state_ids": {
            "transform_id": matched_op.get('transform_id', 0),
            "clip_id": matched_op.get('clip_id', 0),
            "effect_id": matched_op.get('effect_id', 0)
        }
    }

    # Add border_radii if present
    if 'border_radii' in layout_node:
        result['border_radii'] = layout_node['border_radii']

    return result


def main():
    if len(sys.argv) < 4:
        print("Usage: extract_border_inputs.py <layout_tree.json> <raw_paint_ops.json> <output_dir>")
        sys.exit(1)

    layout_tree_file = sys.argv[1]
    raw_ops_file = sys.argv[2]
    output_dir = sys.argv[3]

    with open(layout_tree_file) as f:
        layout_tree = json.load(f)['layout_tree']

    with open(raw_ops_file) as f:
        raw_ops = json.load(f)['paint_ops']

    os.makedirs(output_dir, exist_ok=True)

    bordered_nodes = find_bordered_nodes(layout_tree)
    print(f"  Found {len(bordered_nodes)} nodes with borders")

    # Count stroked rect ops in raw_paint_ops
    stroked_ops = [op for op in raw_ops
                   if op.get('type') in ('DrawRectOp', 'DrawRRectOp')
                   and op.get('flags', {}).get('style') == 1]
    print(f"  Found {len(stroked_ops)} stroked DrawRectOp/DrawRRectOp in raw_paint_ops.json")

    # Track used raw_ops indices
    used_indices = set()
    matched_count = 0

    for i, node in enumerate(bordered_nodes):
        matched_op, op_idx = match_with_raw_ops(node, raw_ops, used_indices)
        if matched_op:
            used_indices.add(op_idx)
            input_data = create_border_painter_input(node, matched_op)
            output_file = os.path.join(output_dir, f"input_{i:03d}.json")
            with open(output_file, 'w') as f:
                json.dump(input_data, f, indent=2)
            name = node.get('name', 'unknown')[:40]
            print(f"  Created {os.path.basename(output_file)} for '{name}'")
            matched_count += 1
        else:
            name = node.get('name', 'unknown')[:40]
            # print(f"  Warning: No match for '{name}'")

    print(f"  Matched {matched_count}/{len(bordered_nodes)} bordered nodes")


if __name__ == '__main__':
    main()
