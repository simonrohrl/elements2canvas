#!/usr/bin/env python3
"""Compare border_painter output with expected values from input files.

Handles multiple match types:
1. stroked_rect - uniform borders as single stroked DrawRectOp/DrawRRectOp
2. draw_line - per-side borders as DrawLineOp
3. filled_thin_rect - per-side borders as filled thin DrawRectOp
4. double_stroked - double borders as 2 stroked rects (outer + inner)
5. dotted_lines - dotted borders as 4 DrawLineOp with dash pattern
6. groove_ridge - groove/ridge borders as paired thin rects per side
"""
import json
import sys
import os
import glob
import math


def compare_stroked_rect(computed, input_data, tolerance=0.5):
    """Compare computed stroked DrawRectOp/DrawRRectOp with expected."""
    diffs = []

    # Handle array output format
    if isinstance(computed, list):
        if len(computed) == 0:
            return ["Empty computed output"]
        computed = computed[0]

    geom = input_data['geometry']
    border_widths = input_data['border_widths']
    border_colors = input_data['border_colors']
    border_radii = input_data.get('border_radii')
    expected_state = input_data.get('state_ids', {})

    stroke_width = border_widths['top']
    inset = stroke_width / 2.0
    expected_rect = [
        geom['x'] + inset,
        geom['y'] + inset,
        geom['x'] + geom['width'] - inset,
        geom['y'] + geom['height'] - inset
    ]

    expected_radii = None
    if border_radii:
        expected_radii = [max(0, r - inset) for r in border_radii]

    # Check type
    computed_type = computed.get('type')
    if border_radii:
        if computed_type != 'DrawRRectOp':
            diffs.append(f"Type: {computed_type} vs DrawRRectOp")
    else:
        if computed_type != 'DrawRectOp':
            diffs.append(f"Type: {computed_type} vs DrawRectOp")

    # Compare rect
    c_rect = computed.get('rect', [0, 0, 0, 0])
    for i, (c_val, e_val) in enumerate(zip(c_rect, expected_rect)):
        if abs(c_val - e_val) > tolerance:
            diffs.append(f"Rect[{i}]: {c_val} vs {e_val}")

    # Compare stroke flags
    c_flags = computed.get('flags', {})

    if c_flags.get('style', 0) != 1:
        diffs.append(f"Style: {c_flags.get('style')} vs 1")

    if abs(c_flags.get('strokeWidth', 0) - stroke_width) > tolerance:
        diffs.append(f"StrokeWidth: {c_flags.get('strokeWidth')} vs {stroke_width}")

    # Compare color
    expected_color = border_colors['top']
    for key in ['r', 'g', 'b', 'a']:
        c_val = c_flags.get(key, 0)
        e_val = expected_color.get(key, 0)
        if abs(c_val - e_val) > 0.01:
            diffs.append(f"Color {key}: {c_val} vs {e_val}")

    # Compare radii
    if expected_radii:
        c_radii = computed.get('radii', [])
        if len(c_radii) != len(expected_radii):
            diffs.append(f"Radii count: {len(c_radii)} vs {len(expected_radii)}")
        else:
            for i, (c_val, e_val) in enumerate(zip(c_radii, expected_radii)):
                if abs(c_val - e_val) > tolerance:
                    diffs.append(f"Radii[{i}]: {c_val} vs {e_val}")

    # Compare property tree state
    for key in ['transform_id', 'clip_id', 'effect_id']:
        c_val = computed.get(key)
        e_val = expected_state.get(key)
        if c_val is not None and e_val is not None and c_val != e_val:
            diffs.append(f"{key}: {c_val} vs {e_val}")

    return diffs


def compare_draw_lines(computed, input_data, tolerance=2.0):
    """Compare computed DrawLineOp with expected per-side borders."""
    diffs = []

    if not isinstance(computed, list):
        return ["Expected list of DrawLineOp"]

    geom = input_data['geometry']
    bw = input_data['border_widths']
    bc = input_data['border_colors']

    # Build expected lines for each side with non-zero border
    expected_sides = {}
    for side in ['top', 'right', 'bottom', 'left']:
        width = bw[side]
        if width == 0:
            continue

        half = width / 2.0
        if side == 'top':
            expected_sides[side] = {
                'x0': geom['x'], 'y0': geom['y'] + half,
                'x1': geom['x'] + geom['width'], 'y1': geom['y'] + half,
                'strokeWidth': width, 'color': bc[side]
            }
        elif side == 'bottom':
            expected_sides[side] = {
                'x0': geom['x'], 'y0': geom['y'] + geom['height'] - half,
                'x1': geom['x'] + geom['width'], 'y1': geom['y'] + geom['height'] - half,
                'strokeWidth': width, 'color': bc[side]
            }
        elif side == 'left':
            expected_sides[side] = {
                'x0': geom['x'] + half, 'y0': geom['y'],
                'x1': geom['x'] + half, 'y1': geom['y'] + geom['height'],
                'strokeWidth': width, 'color': bc[side]
            }
        elif side == 'right':
            expected_sides[side] = {
                'x0': geom['x'] + geom['width'] - half, 'y0': geom['y'],
                'x1': geom['x'] + geom['width'] - half, 'y1': geom['y'] + geom['height'],
                'strokeWidth': width, 'color': bc[side]
            }

    # Check we have the right number of ops
    if len(computed) != len(expected_sides):
        diffs.append(f"Op count: {len(computed)} vs {len(expected_sides)}")

    # Match computed ops with expected sides
    for c_op in computed:
        if c_op.get('type') != 'DrawLineOp':
            diffs.append(f"Unexpected type: {c_op.get('type')}")
            continue

        # Find matching side
        matched = False
        for side, expected in expected_sides.items():
            if (abs(c_op['x0'] - expected['x0']) <= tolerance and
                abs(c_op['y0'] - expected['y0']) <= tolerance and
                abs(c_op['x1'] - expected['x1']) <= tolerance and
                abs(c_op['y1'] - expected['y1']) <= tolerance):

                # Check stroke width
                c_sw = c_op.get('flags', {}).get('strokeWidth', 0)
                if abs(c_sw - expected['strokeWidth']) > 1:
                    diffs.append(f"{side} strokeWidth: {c_sw} vs {expected['strokeWidth']}")

                # Check color
                c_flags = c_op.get('flags', {})
                for key in ['r', 'g', 'b']:
                    c_val = c_flags.get(key, 0)
                    e_val = expected['color'].get(key, 0)
                    if abs(c_val - e_val) > 0.02:
                        diffs.append(f"{side} color {key}: {c_val:.3f} vs {e_val:.3f}")

                matched = True
                break

        if not matched:
            diffs.append(f"Unmatched line: ({c_op['x0']:.1f},{c_op['y0']:.1f})->({c_op['x1']:.1f},{c_op['y1']:.1f})")

    return diffs


def compare_filled_thin_rects(computed, input_data, tolerance=2.0):
    """Compare computed filled thin DrawRectOp with expected per-side borders."""
    diffs = []

    if not isinstance(computed, list):
        return ["Expected list of DrawRectOp"]

    geom = input_data['geometry']
    bw = input_data['border_widths']
    bc = input_data['border_colors']

    # Build expected rects for each side with non-zero border
    expected_sides = {}
    for side in ['top', 'right', 'bottom', 'left']:
        width = bw[side]
        if width == 0:
            continue

        if side == 'top':
            expected_sides[side] = {
                'rect': [geom['x'], geom['y'], geom['x'] + geom['width'], geom['y'] + width],
                'color': bc[side]
            }
        elif side == 'bottom':
            expected_sides[side] = {
                'rect': [geom['x'], geom['y'] + geom['height'] - width,
                        geom['x'] + geom['width'], geom['y'] + geom['height']],
                'color': bc[side]
            }
        elif side == 'left':
            expected_sides[side] = {
                'rect': [geom['x'], geom['y'], geom['x'] + width, geom['y'] + geom['height']],
                'color': bc[side]
            }
        elif side == 'right':
            expected_sides[side] = {
                'rect': [geom['x'] + geom['width'] - width, geom['y'],
                        geom['x'] + geom['width'], geom['y'] + geom['height']],
                'color': bc[side]
            }

    # Check op count
    if len(computed) != len(expected_sides):
        diffs.append(f"Op count: {len(computed)} vs {len(expected_sides)}")

    # Match computed ops with expected sides
    for c_op in computed:
        if c_op.get('type') != 'DrawRectOp':
            diffs.append(f"Unexpected type: {c_op.get('type')}")
            continue

        c_rect = c_op.get('rect', [0, 0, 0, 0])
        c_flags = c_op.get('flags', {})

        # Should be filled (style=0)
        if c_flags.get('style', 0) != 0:
            diffs.append(f"Expected filled style, got {c_flags.get('style')}")

        # Find matching side
        matched = False
        for side, expected in expected_sides.items():
            e_rect = expected['rect']
            if all(abs(c_rect[i] - e_rect[i]) <= tolerance for i in range(4)):
                # Check color
                for key in ['r', 'g', 'b']:
                    c_val = c_flags.get(key, 0)
                    e_val = expected['color'].get(key, 0)
                    if abs(c_val - e_val) > 0.02:
                        diffs.append(f"{side} color {key}: {c_val:.3f} vs {e_val:.3f}")
                matched = True
                break

        if not matched:
            diffs.append(f"Unmatched rect: [{c_rect[0]:.1f},{c_rect[1]:.1f},{c_rect[2]:.1f},{c_rect[3]:.1f}]")

    return diffs


def compare_double_stroked(computed, input_data, tolerance=2.0):
    """Compare computed double border (2 stroked rects) with expected."""
    diffs = []

    if not isinstance(computed, list) or len(computed) < 2:
        return [f"Expected 2 ops for double border, got {len(computed) if isinstance(computed, list) else 0}"]

    geom = input_data['geometry']
    bw = input_data['border_widths']
    bc = input_data['border_colors']
    radii = input_data.get('border_radii')

    border_width = bw['top']
    border_color = bc['top']

    # For double border: strokeWidth = ceil(border_width/3)
    sw = math.ceil(border_width / 3)
    outer_inset = sw / 2.0
    inner_inset = border_width - sw / 2.0

    outer_rect = [
        geom['x'] + outer_inset,
        geom['y'] + outer_inset,
        geom['x'] + geom['width'] - outer_inset,
        geom['y'] + geom['height'] - outer_inset
    ]
    inner_rect = [
        geom['x'] + inner_inset,
        geom['y'] + inner_inset,
        geom['x'] + geom['width'] - inner_inset,
        geom['y'] + geom['height'] - inner_inset
    ]

    outer_found = False
    inner_found = False

    for op in computed:
        if op.get('type') not in ('DrawRectOp', 'DrawRRectOp'):
            continue
        flags = op.get('flags', {})
        if flags.get('style', 0) != 1:  # Must be stroked
            continue
        if abs(flags.get('strokeWidth', 0) - sw) > 1:
            continue

        rect = op.get('rect', [])
        if len(rect) != 4:
            continue

        # Check color
        if not (abs(flags.get('r', 0) - border_color['r']) < 0.05 and
                abs(flags.get('g', 0) - border_color['g']) < 0.05 and
                abs(flags.get('b', 0) - border_color['b']) < 0.05):
            continue

        # Check if outer
        if not outer_found:
            if all(abs(rect[i] - outer_rect[i]) <= tolerance for i in range(4)):
                outer_found = True
                continue

        # Check if inner
        if not inner_found:
            if all(abs(rect[i] - inner_rect[i]) <= tolerance for i in range(4)):
                inner_found = True

    if not outer_found:
        diffs.append(f"Outer rect not found (expected inset {outer_inset})")
    if not inner_found:
        diffs.append(f"Inner rect not found (expected inset {inner_inset})")

    return diffs


def compare_dotted_lines(computed, input_data, tolerance=5.0):
    """Compare computed dotted border (4 DrawLineOp) with expected."""
    diffs = []

    if not isinstance(computed, list):
        return ["Expected list of DrawLineOp"]

    # Use same comparison as draw_lines but with wider tolerance
    return compare_draw_lines(computed, input_data, tolerance)


def compare_groove_ridge(computed, input_data, tolerance=2.0):
    """Compare computed groove/ridge border (paired thin rects) with expected."""
    diffs = []

    if not isinstance(computed, list) or len(computed) < 4:
        return [f"Expected at least 4 ops for groove/ridge, got {len(computed) if isinstance(computed, list) else 0}"]

    geom = input_data['geometry']
    bw = input_data['border_widths']
    bc = input_data['border_colors']

    border_width = bw['top']
    half_width = border_width / 2.0

    # Just check that we have the right number of filled rects
    filled_rects = [op for op in computed if op.get('type') == 'DrawRectOp' and
                    op.get('flags', {}).get('style', 0) == 0]

    # Each side needs 2 rects (outer half, inner half) = 8 total
    if len(filled_rects) < 8:
        diffs.append(f"Expected 8 filled rects, got {len(filled_rects)}")

    return diffs


def compare_ops(computed, input_data, tolerance=0.5):
    """Compare computed output with expected based on match_type."""
    match_type = input_data.get('match_type', 'stroked_rect')

    if match_type == 'stroked_rect':
        return compare_stroked_rect(computed, input_data, tolerance)
    elif match_type == 'draw_line':
        return compare_draw_lines(computed, input_data, tolerance)
    elif match_type == 'filled_thin_rect':
        return compare_filled_thin_rects(computed, input_data, tolerance)
    elif match_type == 'double_stroked':
        return compare_double_stroked(computed, input_data, tolerance)
    elif match_type == 'dotted_lines':
        return compare_dotted_lines(computed, input_data, tolerance)
    elif match_type == 'groove_ridge':
        return compare_groove_ridge(computed, input_data, tolerance)
    else:
        return [f"Unknown match_type: {match_type}"]


def main():
    if len(sys.argv) < 3:
        print("Usage: compare_border_ops.py <raw_paint_ops.json> <border_inputs_dir>")
        sys.exit(1)

    raw_ops_file = sys.argv[1]
    inputs_dir = sys.argv[2]

    with open(raw_ops_file) as f:
        raw_ops = json.load(f)['paint_ops']
    stroked_ops = len([op for op in raw_ops
                       if op.get('type') in ('DrawRectOp', 'DrawRRectOp')
                       and op.get('flags', {}).get('style') == 1])
    draw_lines = len([op for op in raw_ops if op.get('type') == 'DrawLineOp'])
    thin_rects = len([op for op in raw_ops
                      if op.get('type') == 'DrawRectOp'
                      and op.get('flags', {}).get('style') == 0
                      and len(op.get('rect', [])) == 4
                      and ((op['rect'][2] - op['rect'][0] < 10) !=
                           (op['rect'][3] - op['rect'][1] < 10))])

    print(f"  Raw ops: {stroked_ops} stroked rects, {draw_lines} lines, {thin_rects} thin filled rects")

    input_files = sorted(glob.glob(os.path.join(inputs_dir, "input_*.json")))
    if not input_files:
        print("  No input files found in", inputs_dir)
        return

    output_files = sorted(glob.glob(os.path.join(inputs_dir, "output_*.json")))
    print(f"  Found {len(input_files)} input files, {len(output_files)} output files")
    print("")

    # Count by match type
    all_types = ['stroked_rect', 'draw_line', 'filled_thin_rect', 'double_stroked', 'dotted_lines', 'groove_ridge']
    match_type_counts = {t: 0 for t in all_types}
    results_by_type = {t: [0, 0] for t in all_types}

    total = 0
    passed = 0
    failed = 0
    skipped = 0

    for input_file in input_files:
        output_file = input_file.replace("input_", "output_")

        if not os.path.exists(output_file):
            print(f"SKIP: {os.path.basename(input_file)} (no output file)")
            skipped += 1
            continue

        with open(input_file) as f:
            input_data = json.load(f)
        with open(output_file) as f:
            computed = json.load(f)

        match_type = input_data.get('match_type', 'stroked_rect')
        match_type_counts[match_type] = match_type_counts.get(match_type, 0) + 1

        total += 1
        diffs = compare_ops(computed, input_data)

        node_id = input_data.get('node_id', 0)
        if not diffs:
            passed += 1
            results_by_type[match_type][0] += 1
            print(f"PASS: {os.path.basename(input_file)} (node {node_id}, {match_type})")
        else:
            failed += 1
            results_by_type[match_type][1] += 1
            print(f"FAIL: {os.path.basename(input_file)} (node {node_id}, {match_type})")
            for d in diffs[:5]:
                print(f"  - {d}")
            if len(diffs) > 5:
                print(f"  ... and {len(diffs) - 5} more differences")

    print("")
    print("=" * 40)
    print(f"Results: {passed} passed, {failed} failed, {skipped} skipped (total: {total})")
    if total > 0:
        print(f"Pass rate: {passed/total*100:.1f}%")
    print("")
    print("By match type:")
    for mt, (p, f) in results_by_type.items():
        if p + f > 0:
            print(f"  {mt}: {p}/{p+f} passed ({p/(p+f)*100:.1f}%)")


if __name__ == '__main__':
    main()
