#!/usr/bin/env python3
"""Compare border_painter output with expected values from input files.

This is a round-trip test: input data comes from raw_paint_ops.json,
border_painter processes it, and we verify the output matches the input.
"""
import json
import sys
import os
import glob


def compare_ops(computed, input_data, tolerance=0.5):
    """Compare computed DrawRectOp/DrawRRectOp with expected values from input.

    Args:
        computed: Output from border_painter (list of ops)
        input_data: Input JSON that was fed to border_painter
        tolerance: Float comparison tolerance

    Returns:
        List of difference strings (empty if all match)
    """
    diffs = []

    # Handle array output format
    if isinstance(computed, list):
        if len(computed) == 0:
            return ["Empty computed output"]
        computed = computed[0]

    # Input data
    geom = input_data['geometry']
    border_widths = input_data['border_widths']
    border_colors = input_data['border_colors']
    border_radii = input_data.get('border_radii')
    expected_state = input_data.get('state_ids', {})

    # Computed stroke rect should be inset by strokeWidth/2
    stroke_width = border_widths['top']  # Assuming uniform
    inset = stroke_width / 2.0
    expected_rect = [
        geom['x'] + inset,
        geom['y'] + inset,
        geom['x'] + geom['width'] - inset,
        geom['y'] + geom['height'] - inset
    ]

    # Expected radii (adjusted for stroke)
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

    # Style should be 1 (stroke)
    if c_flags.get('style', 0) != 1:
        diffs.append(f"Style: {c_flags.get('style')} vs 1")

    # Stroke width
    if abs(c_flags.get('strokeWidth', 0) - stroke_width) > tolerance:
        diffs.append(f"StrokeWidth: {c_flags.get('strokeWidth')} vs {stroke_width}")

    # Compare color (using top border color)
    expected_color = border_colors['top']
    for key in ['r', 'g', 'b', 'a']:
        c_val = c_flags.get(key, 0)
        e_val = expected_color.get(key, 0)
        if abs(c_val - e_val) > 0.01:
            diffs.append(f"Color {key}: {c_val} vs {e_val}")

    # Compare radii if present
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


def main():
    if len(sys.argv) < 3:
        print("Usage: compare_border_ops.py <raw_paint_ops.json> <border_inputs_dir>")
        sys.exit(1)

    raw_ops_file = sys.argv[1]
    inputs_dir = sys.argv[2]

    # Count raw ops for info
    with open(raw_ops_file) as f:
        raw_ops = json.load(f)['paint_ops']
    stroked_ops = [op for op in raw_ops
                   if op.get('type') in ('DrawRectOp', 'DrawRRectOp')
                   and op.get('flags', {}).get('style') == 1]
    print(f"  Found {len(stroked_ops)} stroked DrawRectOp/DrawRRectOp in raw_paint_ops.json")

    input_files = sorted(glob.glob(os.path.join(inputs_dir, "input_*.json")))

    if not input_files:
        print("  No input files found in", inputs_dir)
        return

    output_files = sorted(glob.glob(os.path.join(inputs_dir, "output_*.json")))
    print(f"  Found {len(input_files)} input files, {len(output_files)} output files")
    print("")

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

        total += 1
        diffs = compare_ops(computed, input_data)

        node_id = input_data.get('node_id', 0)
        if not diffs:
            passed += 1
            print(f"PASS: {os.path.basename(input_file)} (node {node_id})")
        else:
            failed += 1
            print(f"FAIL: {os.path.basename(input_file)} (node {node_id})")
            for d in diffs[:5]:  # Limit to first 5 diffs
                print(f"  - {d}")
            if len(diffs) > 5:
                print(f"  ... and {len(diffs) - 5} more differences")

    print("")
    print("=" * 40)
    print(f"Results: {passed} passed, {failed} failed, {skipped} skipped (total: {total})")
    if total > 0:
        print(f"Pass rate: {passed/total*100:.1f}%")


if __name__ == '__main__':
    main()
