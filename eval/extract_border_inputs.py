#!/usr/bin/env python3
"""Extract border data and create border_painter input files.

Transforms nodes from layout_tree.json that have border_widths/border_colors
into border_painter input format by matching with various border ops from
raw_paint_ops.json:

1. Stroked DrawRectOp/DrawRRectOp - uniform solid borders as single stroke
2. Filled thin DrawRectOp - per-side borders as thin filled rects
3. DrawLineOp - per-side borders as stroked lines
4. Double stroked rects - uniform double borders as 2 stroked rects
5. Groove/Ridge paired rects - split borders with light/dark halves
6. Dotted DrawLineOp - dotted borders rendered with dash pattern
"""
import json
import sys
import os
import math


def find_bordered_nodes(layout_tree):
    """Find all nodes with border_widths and border_colors."""
    bordered = []
    for node in layout_tree:
        if 'geometry' not in node:
            continue
        if 'border_widths' not in node or 'border_colors' not in node:
            continue
        bw = node['border_widths']
        if bw['top'] == 0 and bw['right'] == 0 and bw['bottom'] == 0 and bw['left'] == 0:
            continue
        bordered.append(node)
    return bordered


def is_uniform_border(border_widths):
    """Check if all border widths are the same."""
    bw = border_widths
    return bw['top'] == bw['right'] == bw['bottom'] == bw['left'] and bw['top'] > 0


def color_matches(layout_color, op_flags, tolerance=0.02):
    """Check if colors match within tolerance."""
    return (abs(layout_color['r'] - op_flags.get('r', 0)) <= tolerance and
            abs(layout_color['g'] - op_flags.get('g', 0)) <= tolerance and
            abs(layout_color['b'] - op_flags.get('b', 0)) <= tolerance)


def radii_matches(layout_radii, stroke_width, op_radii, tolerance=1.0):
    """Check if border radii match after stroke adjustment."""
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


def get_border_style(layout_node, side='top'):
    """Get border style from layout node."""
    bs = layout_node.get('border_styles', {})
    return bs.get(side, 'solid')


def match_stroked_rect(layout_node, raw_ops, used_indices):
    """Match uniform bordered node with stroked DrawRectOp/DrawRRectOp."""
    geom = layout_node['geometry']
    bw = layout_node['border_widths']
    bc = layout_node['border_colors']
    radii = layout_node.get('border_radii')

    if not is_uniform_border(bw):
        return None, -1

    stroke_width = bw['top']
    border_color = bc['top']
    inset = stroke_width / 2.0

    expected_rect = [
        geom['x'] + inset,
        geom['y'] + inset,
        geom['x'] + geom['width'] - inset,
        geom['y'] + geom['height'] - inset
    ]

    for idx, op in enumerate(raw_ops):
        if idx in used_indices:
            continue
        if op.get('type') not in ('DrawRectOp', 'DrawRRectOp'):
            continue
        flags = op.get('flags', {})
        if flags.get('style', 0) != 1:  # Must be stroked
            continue
        if abs(flags.get('strokeWidth', 0) - stroke_width) > 0.5:
            continue

        rect = op.get('rect', [])
        if len(rect) != 4:
            continue

        # Check geometry
        if not all(abs(expected_rect[i] - rect[i]) <= 2 for i in range(4)):
            continue

        # Check color
        if not color_matches(border_color, flags):
            continue

        # Check radii
        op_radii = op.get('radii')
        if radii and op.get('type') == 'DrawRRectOp':
            if not radii_matches(radii, stroke_width, op_radii):
                continue

        return op, idx

    return None, -1


def match_double_stroked_rects(layout_node, raw_ops, used_indices):
    """Match uniform double border rendered as 2 stroked rects.

    Double borders are rendered as two concentric stroked rectangles,
    each with strokeWidth = borderWidth/3 (rounded up).
    Outer rect: inset by sw/2 from outer edge
    Inner rect: inset by (borderWidth - sw/2) from outer edge
    """
    geom = layout_node['geometry']
    bw = layout_node['border_widths']
    bc = layout_node['border_colors']
    radii = layout_node.get('border_radii')

    if not is_uniform_border(bw):
        return None, []

    border_width = bw['top']
    border_color = bc['top']

    # For double border, stroke width is ceil(border_width/3)
    sw = math.ceil(border_width / 3)

    # Outer stroke center is at sw/2 inset
    outer_inset = sw / 2.0
    # Inner stroke center is at border_width - sw/2 inset
    inner_inset = border_width - sw / 2.0

    outer_expected = [
        geom['x'] + outer_inset,
        geom['y'] + outer_inset,
        geom['x'] + geom['width'] - outer_inset,
        geom['y'] + geom['height'] - outer_inset
    ]

    inner_expected = [
        geom['x'] + inner_inset,
        geom['y'] + inner_inset,
        geom['x'] + geom['width'] - inner_inset,
        geom['y'] + geom['height'] - inner_inset
    ]

    outer_op = None
    inner_op = None
    outer_idx = -1
    inner_idx = -1

    for idx, op in enumerate(raw_ops):
        if idx in used_indices:
            continue
        if op.get('type') not in ('DrawRectOp', 'DrawRRectOp'):
            continue
        flags = op.get('flags', {})
        if flags.get('style', 0) != 1:  # Must be stroked
            continue
        if abs(flags.get('strokeWidth', 0) - sw) > 1:
            continue
        if not color_matches(border_color, flags):
            continue

        rect = op.get('rect', [])
        if len(rect) != 4:
            continue

        # Check if matches outer
        if outer_op is None:
            if all(abs(outer_expected[i] - rect[i]) <= 2 for i in range(4)):
                outer_op = op
                outer_idx = idx
                continue

        # Check if matches inner
        if inner_op is None:
            if all(abs(inner_expected[i] - rect[i]) <= 2 for i in range(4)):
                inner_op = op
                inner_idx = idx

    if outer_op and inner_op:
        return {'outer': outer_op, 'inner': inner_op}, [outer_idx, inner_idx]

    return None, []


def match_dotted_lines(layout_node, raw_ops, used_indices):
    """Match bordered node with dotted DrawLineOp.

    Dotted borders are rendered as DrawLineOp with dash pattern.
    The lines may be slightly inset from the geometry edges.
    """
    geom = layout_node['geometry']
    bw = layout_node['border_widths']
    bc = layout_node['border_colors']

    if not is_uniform_border(bw):
        return None, []

    border_width = bw['top']
    border_color = bc['top']

    matched_ops = {}
    matched_indices = []

    # For dotted borders, lines are typically inset by a small amount
    # and drawn with the stroke centered on the line
    half = border_width / 2.0

    for side in ['top', 'right', 'bottom', 'left']:
        if side == 'top':
            exp_x0, exp_y0 = geom['x'], geom['y'] + half
            exp_x1, exp_y1 = geom['x'] + geom['width'], geom['y'] + half
        elif side == 'bottom':
            exp_x0, exp_y0 = geom['x'], geom['y'] + geom['height'] - half
            exp_x1, exp_y1 = geom['x'] + geom['width'], geom['y'] + geom['height'] - half
        elif side == 'left':
            exp_x0, exp_y0 = geom['x'] + half, geom['y']
            exp_x1, exp_y1 = geom['x'] + half, geom['y'] + geom['height']
        elif side == 'right':
            exp_x0, exp_y0 = geom['x'] + geom['width'] - half, geom['y']
            exp_x1, exp_y1 = geom['x'] + geom['width'] - half, geom['y'] + geom['height']

        for idx, op in enumerate(raw_ops):
            if idx in used_indices or idx in matched_indices:
                continue
            if op.get('type') != 'DrawLineOp':
                continue

            flags = op.get('flags', {})
            if abs(flags.get('strokeWidth', 0) - border_width) > 1:
                continue
            if not color_matches(border_color, flags):
                continue

            # Allow wider tolerance for dotted borders (they may be inset differently)
            tol = 5
            if not (abs(op['x0'] - exp_x0) <= tol and abs(op['y0'] - exp_y0) <= tol and
                    abs(op['x1'] - exp_x1) <= tol and abs(op['y1'] - exp_y1) <= tol):
                continue

            matched_ops[side] = op
            matched_indices.append(idx)
            break

    # Need all 4 sides for dotted border
    if len(matched_ops) == 4:
        return matched_ops, matched_indices

    return None, []


def match_groove_ridge_rects(layout_node, raw_ops, used_indices):
    """Match groove/ridge border rendered as paired thin filled rects.

    Groove and ridge borders are rendered as pairs of thin filled rectangles
    for each side - one light, one dark. Each half is border_width/2 thick.
    """
    geom = layout_node['geometry']
    bw = layout_node['border_widths']
    bc = layout_node['border_colors']

    if not is_uniform_border(bw):
        return None, []

    border_width = bw['top']
    border_color = bc['top']
    half_width = border_width / 2.0

    # Calculate expected rects for groove/ridge
    # Each side has 2 thin rects (outer half and inner half)
    expected_rects = {}

    # Top side: two horizontal rects at top
    expected_rects['top_outer'] = [geom['x'], geom['y'],
                                    geom['x'] + geom['width'], geom['y'] + half_width]
    expected_rects['top_inner'] = [geom['x'], geom['y'] + half_width,
                                    geom['x'] + geom['width'], geom['y'] + border_width]

    # Bottom side
    expected_rects['bottom_outer'] = [geom['x'], geom['y'] + geom['height'] - half_width,
                                       geom['x'] + geom['width'], geom['y'] + geom['height']]
    expected_rects['bottom_inner'] = [geom['x'], geom['y'] + geom['height'] - border_width,
                                       geom['x'] + geom['width'], geom['y'] + geom['height'] - half_width]

    # Right side: two vertical rects at right
    expected_rects['right_outer'] = [geom['x'] + geom['width'] - half_width, geom['y'],
                                      geom['x'] + geom['width'], geom['y'] + geom['height']]
    expected_rects['right_inner'] = [geom['x'] + geom['width'] - border_width, geom['y'],
                                      geom['x'] + geom['width'] - half_width, geom['y'] + geom['height']]

    # Left side
    expected_rects['left_outer'] = [geom['x'], geom['y'],
                                     geom['x'] + half_width, geom['y'] + geom['height']]
    expected_rects['left_inner'] = [geom['x'] + half_width, geom['y'],
                                     geom['x'] + border_width, geom['y'] + geom['height']]

    matched_ops = {}
    matched_indices = []

    # Try to match each expected rect
    for key, expected in expected_rects.items():
        for idx, op in enumerate(raw_ops):
            if idx in used_indices or idx in matched_indices:
                continue
            if op.get('type') != 'DrawRectOp':
                continue
            flags = op.get('flags', {})
            if flags.get('style', 0) != 0:  # Must be filled
                continue

            rect = op.get('rect', [])
            if len(rect) != 4:
                continue

            # Check geometry with tolerance
            if all(abs(expected[i] - rect[i]) <= 2 for i in range(4)):
                # Check if color is related to border color (darker or same)
                r, g, b = flags.get('r', 0), flags.get('g', 0), flags.get('b', 0)
                br, bg, bb = border_color['r'], border_color['g'], border_color['b']

                # Allow for darkened color (roughly 0.5-0.7x) or original
                is_same = color_matches(border_color, flags, tolerance=0.1)
                is_dark = (abs(r - br * 0.65) < 0.1 and
                          abs(g - bg * 0.65) < 0.1 and
                          abs(b - bb * 0.65) < 0.1)
                is_light = (r >= br * 0.9 and g >= bg * 0.9 and b >= bb * 0.9)

                if is_same or is_dark or is_light:
                    matched_ops[key] = op
                    matched_indices.append(idx)
                    break

    # Need at least 4 matched rects (one pair for at least 2 sides)
    if len(matched_ops) >= 4:
        return matched_ops, matched_indices

    return None, []


def match_filled_thin_rects(layout_node, raw_ops, used_indices):
    """Match non-uniform bordered node with filled thin rectangles.

    Chromium renders thin borders (especially table cell borders) as
    filled rectangles instead of stroked lines.
    """
    geom = layout_node['geometry']
    bw = layout_node['border_widths']
    bc = layout_node['border_colors']

    matched_ops = {}
    matched_indices = []

    for side in ['top', 'right', 'bottom', 'left']:
        width = bw[side]
        if width == 0:
            continue

        color = bc[side]

        # Calculate expected rect for this border side (filled rect)
        if side == 'top':
            expected = [geom['x'], geom['y'], geom['x'] + geom['width'], geom['y'] + width]
        elif side == 'bottom':
            expected = [geom['x'], geom['y'] + geom['height'] - width,
                       geom['x'] + geom['width'], geom['y'] + geom['height']]
        elif side == 'left':
            expected = [geom['x'], geom['y'], geom['x'] + width, geom['y'] + geom['height']]
        elif side == 'right':
            expected = [geom['x'] + geom['width'] - width, geom['y'],
                       geom['x'] + geom['width'], geom['y'] + geom['height']]

        # Find matching filled thin rect
        for idx, op in enumerate(raw_ops):
            if idx in used_indices or idx in matched_indices:
                continue
            if op.get('type') != 'DrawRectOp':
                continue
            flags = op.get('flags', {})
            if flags.get('style', 0) != 0:  # Must be filled
                continue

            rect = op.get('rect', [])
            if len(rect) != 4:
                continue

            # Check if thin rect
            w = rect[2] - rect[0]
            h = rect[3] - rect[1]
            if not ((w < 10 and h > 10) or (h < 10 and w > 10)):
                continue

            # Check geometry (allow tolerance)
            if not all(abs(expected[i] - rect[i]) <= 2 for i in range(4)):
                continue

            # Check color
            if not color_matches(color, flags):
                continue

            matched_ops[side] = op
            matched_indices.append(idx)
            break

    # Return if we matched at least one side
    if matched_ops:
        return matched_ops, matched_indices
    return None, []


def match_draw_lines(layout_node, raw_ops, used_indices):
    """Match bordered node with DrawLineOp (stroked lines).

    Chromium sometimes renders borders as stroked lines, especially
    for thicker borders or background-clip scenarios.
    """
    geom = layout_node['geometry']
    bw = layout_node['border_widths']
    bc = layout_node['border_colors']

    matched_ops = {}
    matched_indices = []

    for side in ['top', 'right', 'bottom', 'left']:
        width = bw[side]
        if width == 0:
            continue

        color = bc[side]
        half = width / 2.0

        # Calculate expected line endpoints (center of border)
        if side == 'top':
            exp_x0, exp_y0 = geom['x'], geom['y'] + half
            exp_x1, exp_y1 = geom['x'] + geom['width'], geom['y'] + half
        elif side == 'bottom':
            exp_x0, exp_y0 = geom['x'], geom['y'] + geom['height'] - half
            exp_x1, exp_y1 = geom['x'] + geom['width'], geom['y'] + geom['height'] - half
        elif side == 'left':
            exp_x0, exp_y0 = geom['x'] + half, geom['y']
            exp_x1, exp_y1 = geom['x'] + half, geom['y'] + geom['height']
        elif side == 'right':
            exp_x0, exp_y0 = geom['x'] + geom['width'] - half, geom['y']
            exp_x1, exp_y1 = geom['x'] + geom['width'] - half, geom['y'] + geom['height']

        # Find matching DrawLineOp
        for idx, op in enumerate(raw_ops):
            if idx in used_indices or idx in matched_indices:
                continue
            if op.get('type') != 'DrawLineOp':
                continue

            flags = op.get('flags', {})
            if abs(flags.get('strokeWidth', 0) - width) > 1:
                continue

            # Check position (allow tolerance)
            tol = 2
            if not (abs(op['x0'] - exp_x0) <= tol and abs(op['y0'] - exp_y0) <= tol and
                    abs(op['x1'] - exp_x1) <= tol and abs(op['y1'] - exp_y1) <= tol):
                continue

            # Check color
            if not color_matches(color, flags):
                continue

            matched_ops[side] = op
            matched_indices.append(idx)
            break

    if matched_ops:
        return matched_ops, matched_indices
    return None, []


def create_border_painter_input(layout_node, matched_result, match_type):
    """Create border_painter input JSON from layout node."""
    geom = layout_node['geometry']
    bw = layout_node['border_widths']
    bc = layout_node['border_colors']
    bs = layout_node.get('border_styles', {})

    # Get state_ids from first matched op
    if match_type == 'stroked_rect':
        op = matched_result
        state_ids = {
            'transform_id': op.get('transform_id', 0),
            'clip_id': op.get('clip_id', 0),
            'effect_id': op.get('effect_id', 0)
        }
    else:
        # For per-side matches, use first op's state_ids
        first_op = list(matched_result.values())[0]
        state_ids = {
            'transform_id': first_op.get('transform_id', 0),
            'clip_id': first_op.get('clip_id', 0),
            'effect_id': first_op.get('effect_id', 0)
        }

    result = {
        "geometry": {
            "x": geom['x'],
            "y": geom['y'],
            "width": geom['width'],
            "height": geom['height']
        },
        "border_widths": {
            "top": bw['top'],
            "right": bw['right'],
            "bottom": bw['bottom'],
            "left": bw['left']
        },
        "border_colors": {
            "top": bc['top'],
            "right": bc['right'],
            "bottom": bc['bottom'],
            "left": bc['left']
        },
        "visibility": "visible",
        "node_id": layout_node.get('id', 0),
        "state_ids": state_ids,
        "match_type": match_type  # For debugging/comparison
    }

    if 'border_radii' in layout_node:
        result['border_radii'] = layout_node['border_radii']

    # Add border_styles if present
    if bs:
        result['border_styles'] = {
            "top": bs.get('top', 'solid'),
            "right": bs.get('right', 'solid'),
            "bottom": bs.get('bottom', 'solid'),
            "left": bs.get('left', 'solid')
        }

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

    # Count different op types
    stroked_rects = len([op for op in raw_ops
                         if op.get('type') in ('DrawRectOp', 'DrawRRectOp')
                         and op.get('flags', {}).get('style') == 1])
    draw_lines = len([op for op in raw_ops if op.get('type') == 'DrawLineOp'])
    thin_rects = len([op for op in raw_ops
                      if op.get('type') == 'DrawRectOp'
                      and op.get('flags', {}).get('style') == 0
                      and len(op.get('rect', [])) == 4
                      and ((op['rect'][2] - op['rect'][0] < 10) !=
                           (op['rect'][3] - op['rect'][1] < 10))])

    print(f"  Found {stroked_rects} stroked rects, {draw_lines} lines, {thin_rects} thin filled rects")

    used_indices = set()
    match_counts = {
        'stroked_rect': 0,
        'filled_thin_rect': 0,
        'draw_line': 0,
        'double_stroked': 0,
        'dotted_lines': 0,
        'groove_ridge': 0
    }
    total_matched = 0

    for i, node in enumerate(bordered_nodes):
        matched = False
        match_type = None
        matched_result = None

        # Try matching strategies in order of preference
        # 1. Stroked rect (uniform solid borders)
        if is_uniform_border(node['border_widths']):
            op, idx = match_stroked_rect(node, raw_ops, used_indices)
            if op:
                used_indices.add(idx)
                matched_result = op
                match_type = 'stroked_rect'
                matched = True

        # 2. Double stroked rects (uniform double borders)
        if not matched and is_uniform_border(node['border_widths']):
            ops, indices = match_double_stroked_rects(node, raw_ops, used_indices)
            if ops:
                used_indices.update(indices)
                matched_result = ops
                match_type = 'double_stroked'
                matched = True

        # 3. Dotted lines (uniform dotted borders)
        if not matched and is_uniform_border(node['border_widths']):
            ops, indices = match_dotted_lines(node, raw_ops, used_indices)
            if ops:
                used_indices.update(indices)
                matched_result = ops
                match_type = 'dotted_lines'
                matched = True

        # 4. Groove/Ridge paired rects
        if not matched and is_uniform_border(node['border_widths']):
            ops, indices = match_groove_ridge_rects(node, raw_ops, used_indices)
            if ops:
                used_indices.update(indices)
                matched_result = ops
                match_type = 'groove_ridge'
                matched = True

        # 5. DrawLineOp (per-side stroked lines)
        if not matched:
            ops, indices = match_draw_lines(node, raw_ops, used_indices)
            if ops:
                used_indices.update(indices)
                matched_result = ops
                match_type = 'draw_line'
                matched = True

        # 6. Filled thin rects (per-side filled rectangles)
        if not matched:
            ops, indices = match_filled_thin_rects(node, raw_ops, used_indices)
            if ops:
                used_indices.update(indices)
                matched_result = ops
                match_type = 'filled_thin_rect'
                matched = True

        if matched:
            input_data = create_border_painter_input(node, matched_result, match_type)
            output_file = os.path.join(output_dir, f"input_{i:03d}.json")
            with open(output_file, 'w') as f:
                json.dump(input_data, f, indent=2)
            name = node.get('name', 'unknown')[:40]
            sides = list(matched_result.keys()) if isinstance(matched_result, dict) else ['all']
            print(f"  Created input_{i:03d}.json ({match_type}: {sides})")
            match_counts[match_type] += 1
            total_matched += 1

    print(f"\n  Summary:")
    print(f"    Stroked rect matches: {match_counts['stroked_rect']}")
    print(f"    Double stroked matches: {match_counts['double_stroked']}")
    print(f"    Dotted lines matches: {match_counts['dotted_lines']}")
    print(f"    Groove/Ridge matches: {match_counts['groove_ridge']}")
    print(f"    DrawLineOp matches: {match_counts['draw_line']}")
    print(f"    Filled thin rect matches: {match_counts['filled_thin_rect']}")
    print(f"    Total matched: {total_matched}/{len(bordered_nodes)} bordered nodes")


if __name__ == '__main__':
    main()
