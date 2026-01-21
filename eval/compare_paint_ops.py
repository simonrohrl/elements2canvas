#!/usr/bin/env python3
"""Compare text_painter output with expected values from input files.

This is a round-trip test: input data comes from raw_paint_ops.json,
text_painter processes it, and we verify the output matches the input.
"""
import json
import sys
import os
import glob


def compare_ops(computed, input_data, tolerance=0.01):
    """Compare computed DrawTextBlobOp with expected values from input.

    Args:
        computed: Output from text_painter (list of DrawTextBlobOp)
        input_data: Input JSON that was fed to text_painter
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
    expected_runs = input_data['fragment']['shape_result']['runs']
    expected_x = input_data['box']['x']
    expected_y = input_data['box']['y']
    expected_state = input_data.get('state_ids', {})

    # Compare runs
    c_runs = computed.get('runs', [])

    if len(c_runs) != len(expected_runs):
        diffs.append(f"Run count mismatch: {len(c_runs)} vs {len(expected_runs)}")
        return diffs

    for run_idx, (c_run, e_run) in enumerate(zip(c_runs, expected_runs)):
        prefix = f"Run[{run_idx}]" if len(c_runs) > 1 else ""

        # Compare glyphs (exact match)
        c_glyphs = c_run.get('glyphs', [])
        e_glyphs = e_run.get('glyphs', [])
        if c_glyphs != e_glyphs:
            diffs.append(f"{prefix}Glyphs mismatch: {len(c_glyphs)} vs {len(e_glyphs)} glyphs")

        # Compare positions (within tolerance)
        c_pos = c_run.get('positions', [])
        e_pos = e_run.get('positions', [])
        if len(c_pos) != len(e_pos):
            diffs.append(f"{prefix}Position count mismatch: {len(c_pos)} vs {len(e_pos)}")
        else:
            pos_diffs = []
            for i, (cp, ep) in enumerate(zip(c_pos, e_pos)):
                if abs(cp - ep) > tolerance:
                    pos_diffs.append((i, cp, ep))
            if pos_diffs:
                if len(pos_diffs) <= 3:
                    for i, cp, ep in pos_diffs:
                        diffs.append(f"{prefix}Position[{i}]: {cp} vs {ep}")
                else:
                    diffs.append(f"{prefix}Position mismatches: {len(pos_diffs)} positions differ")

        # Compare font properties
        c_font = c_run.get('font', {})
        e_font = e_run.get('font', {})
        for key in ['size', 'weight', 'family']:
            if key in e_font:
                c_val = c_font.get(key)
                e_val = e_font.get(key)
                if key == 'size' and c_val is not None and e_val is not None:
                    if abs(c_val - e_val) > tolerance:
                        diffs.append(f"{prefix}Font {key}: {c_val} vs {e_val}")
                elif c_val != e_val:
                    diffs.append(f"{prefix}Font {key}: {c_val} vs {e_val}")

    # Compare x, y coordinates
    c_x = computed.get('x', 0)
    if abs(c_x - expected_x) > tolerance:
        diffs.append(f"X: {c_x} vs {expected_x}")

    c_y = computed.get('y', 0)
    if abs(c_y - expected_y) > tolerance:
        diffs.append(f"Y: {c_y} vs {expected_y}")

    # Compare property tree state
    for key in ['transform_id', 'clip_id', 'effect_id']:
        c_val = computed.get(key)
        e_val = expected_state.get(key)
        if c_val is not None and e_val is not None and c_val != e_val:
            diffs.append(f"{key}: {c_val} vs {e_val}")

    return diffs


def main():
    if len(sys.argv) < 3:
        print("Usage: compare_paint_ops.py <raw_paint_ops.json> <text_inputs_dir>")
        sys.exit(1)

    raw_ops_file = sys.argv[1]
    inputs_dir = sys.argv[2]

    # Count raw ops for info
    with open(raw_ops_file) as f:
        raw_ops = json.load(f)['paint_ops']
    text_ops = [op for op in raw_ops if op.get('type') == 'DrawTextBlobOp']
    print(f"  Found {len(text_ops)} DrawTextBlobOp in raw_paint_ops.json")

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

        text_preview = input_data['fragment']['text'][:25]
        if not diffs:
            passed += 1
            print(f"PASS: {os.path.basename(input_file)} - '{text_preview}'")
        else:
            failed += 1
            print(f"FAIL: {os.path.basename(input_file)} - '{text_preview}'")
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
