#!/usr/bin/env python3
"""Compare block_painter output with expected values from input files.

This is a round-trip test: input data comes from raw_paint_ops.json,
block_painter processes it, and we verify the output matches the input.
"""
import json
import sys
import os
import glob


def compare_ops(computed, input_data, tolerance=0.01):
    """Compare computed DrawRectOp/DrawRRectOp with expected values from input.

    Args:
        computed: Output from block_painter (list of ops)
        input_data: Input JSON that was fed to block_painter
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

    # Expected values from input
    geom = input_data['geometry']
    expected_rect = [geom['x'], geom['y'],
                     geom['x'] + geom['width'],
                     geom['y'] + geom['height']]
    expected_color = input_data['background_color']
    expected_radii = input_data.get('border_radii')
    expected_state = input_data.get('state_ids', {})

    # Check type
    computed_type = computed.get('type')
    if expected_radii:
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

    # Compare color
    c_flags = computed.get('flags', {})
    for key in ['r', 'g', 'b', 'a']:
        c_val = c_flags.get(key, 0)
        e_val = expected_color.get(key, 0)
        if abs(c_val - e_val) > tolerance:
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

    # Compare shadows if present
    expected_shadows = input_data.get('box_shadow', [])
    c_shadows = c_flags.get('shadows', [])

    # Filter out inset shadows (we don't handle those)
    expected_shadows = [s for s in expected_shadows if not s.get('inset', False)]

    if len(c_shadows) != len(expected_shadows):
        diffs.append(f"Shadow count: {len(c_shadows)} vs {len(expected_shadows)}")
    else:
        for i, (c_shadow, e_shadow) in enumerate(zip(c_shadows, expected_shadows)):
            if abs(c_shadow.get('offsetX', 0) - e_shadow.get('offset_x', 0)) > tolerance:
                diffs.append(f"Shadow[{i}] offsetX mismatch")
            if abs(c_shadow.get('offsetY', 0) - e_shadow.get('offset_y', 0)) > tolerance:
                diffs.append(f"Shadow[{i}] offsetY mismatch")
            # blurSigma = blur / 2
            expected_sigma = e_shadow.get('blur', 0) / 2
            if abs(c_shadow.get('blurSigma', 0) - expected_sigma) > tolerance:
                diffs.append(f"Shadow[{i}] blurSigma: {c_shadow.get('blurSigma', 0)} vs {expected_sigma}")

    return diffs


def main():
    if len(sys.argv) < 3:
        print("Usage: compare_block_ops.py <raw_paint_ops.json> <block_inputs_dir>")
        sys.exit(1)

    raw_ops_file = sys.argv[1]
    inputs_dir = sys.argv[2]

    # Count raw ops for info
    with open(raw_ops_file) as f:
        raw_ops = json.load(f)['paint_ops']
    rect_ops = [op for op in raw_ops if op.get('type') in ('DrawRectOp', 'DrawRRectOp')]
    print(f"  Found {len(rect_ops)} DrawRectOp/DrawRRectOp in raw_paint_ops.json")

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
