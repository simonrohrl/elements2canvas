#!/usr/bin/env python3
"""
Skia Command Diff Tool

Compares Skia command logs from Chromium and CanvasKit to identify rendering differences.

Usage:
    python skia_diff.py chromium_commands.json canvaskit_commands.json [options]

Options:
    --output FILE       Write detailed diff to JSON file
    --verbose           Show all commands, not just differences
    --tolerance FLOAT   Numeric tolerance for floating point comparison (default: 0.001)
    --align-by TYPE     Align commands by type sequence (default: index)
    --summary           Only show summary statistics
"""

import json
import sys
import argparse
from typing import List, Dict, Any, Tuple, Optional
from dataclasses import dataclass
from enum import Enum


class DiffType(Enum):
    MATCH = "match"
    MISMATCH = "mismatch"
    MISSING_CHROMIUM = "missing_chromium"
    MISSING_CANVASKIT = "missing_canvaskit"
    TYPE_MISMATCH = "type_mismatch"


@dataclass
class CommandDiff:
    index: int
    diff_type: DiffType
    chromium_cmd: Optional[Dict]
    canvaskit_cmd: Optional[Dict]
    differences: List[str]


class SkiaCommandDiffer:
    def __init__(self, tolerance: float = 0.001):
        self.tolerance = tolerance
        self.diffs: List[CommandDiff] = []

    def load_commands(self, filepath: str) -> List[Dict]:
        """Load commands from JSON file."""
        with open(filepath, 'r') as f:
            data = json.load(f)

        if 'commands' in data:
            return data['commands']
        elif isinstance(data, list):
            return data
        else:
            raise ValueError(f"Unknown format in {filepath}")

    def normalize_command(self, cmd: Dict) -> Dict:
        """Normalize a command for comparison."""
        normalized = dict(cmd)

        # Remove index as it's used for alignment
        normalized.pop('index', None)

        # Normalize type names
        type_mapping = {
            'concat': 'concat',
            'concat44': 'concat44',
            'setMatrix': 'setMatrix',
        }
        if 'type' in normalized:
            normalized['type'] = type_mapping.get(normalized['type'], normalized['type'])

        return normalized

    def compare_values(self, path: str, v1: Any, v2: Any) -> List[str]:
        """Compare two values and return list of differences."""
        differences = []

        if type(v1) != type(v2):
            # Allow int/float comparison
            if isinstance(v1, (int, float)) and isinstance(v2, (int, float)):
                pass
            else:
                differences.append(f"{path}: type mismatch ({type(v1).__name__} vs {type(v2).__name__})")
                return differences

        if isinstance(v1, dict):
            all_keys = set(v1.keys()) | set(v2.keys())
            for key in all_keys:
                if key not in v1:
                    differences.append(f"{path}.{key}: missing in chromium")
                elif key not in v2:
                    differences.append(f"{path}.{key}: missing in canvaskit")
                else:
                    differences.extend(self.compare_values(f"{path}.{key}", v1[key], v2[key]))

        elif isinstance(v1, list):
            if len(v1) != len(v2):
                differences.append(f"{path}: array length mismatch ({len(v1)} vs {len(v2)})")
            else:
                for i, (item1, item2) in enumerate(zip(v1, v2)):
                    differences.extend(self.compare_values(f"{path}[{i}]", item1, item2))

        elif isinstance(v1, (int, float)) and isinstance(v2, (int, float)):
            if abs(float(v1) - float(v2)) > self.tolerance:
                differences.append(f"{path}: {v1} vs {v2} (diff: {abs(float(v1) - float(v2)):.6f})")

        elif isinstance(v1, str):
            if v1 != v2:
                # For long strings (like paths), show truncated comparison
                if len(v1) > 50 or len(v2) > 50:
                    differences.append(f"{path}: strings differ (len {len(v1)} vs {len(v2)})")
                else:
                    differences.append(f"{path}: '{v1}' vs '{v2}'")

        elif isinstance(v1, bool):
            if v1 != v2:
                differences.append(f"{path}: {v1} vs {v2}")

        return differences

    def compare_commands(self, cmd1: Dict, cmd2: Dict) -> Tuple[bool, List[str]]:
        """Compare two commands and return (match, differences)."""
        norm1 = self.normalize_command(cmd1)
        norm2 = self.normalize_command(cmd2)

        differences = self.compare_values("", norm1, norm2)
        return len(differences) == 0, differences

    def align_by_index(self, chromium: List[Dict], canvaskit: List[Dict]) -> List[Tuple[Optional[Dict], Optional[Dict]]]:
        """Align commands by their index."""
        max_len = max(len(chromium), len(canvaskit))
        aligned = []

        for i in range(max_len):
            c = chromium[i] if i < len(chromium) else None
            k = canvaskit[i] if i < len(canvaskit) else None
            aligned.append((c, k))

        return aligned

    def align_by_type(self, chromium: List[Dict], canvaskit: List[Dict]) -> List[Tuple[Optional[Dict], Optional[Dict]]]:
        """Align commands by matching types using LCS-like algorithm."""
        # Simple type-based alignment
        aligned = []
        ci, ki = 0, 0

        while ci < len(chromium) or ki < len(canvaskit):
            if ci >= len(chromium):
                aligned.append((None, canvaskit[ki]))
                ki += 1
            elif ki >= len(canvaskit):
                aligned.append((chromium[ci], None))
                ci += 1
            elif chromium[ci].get('type') == canvaskit[ki].get('type'):
                aligned.append((chromium[ci], canvaskit[ki]))
                ci += 1
                ki += 1
            else:
                # Look ahead to find matching type
                chromium_type = chromium[ci].get('type')
                canvaskit_type = canvaskit[ki].get('type')

                # Check if chromium type appears soon in canvaskit
                found_in_canvaskit = False
                for j in range(ki, min(ki + 10, len(canvaskit))):
                    if canvaskit[j].get('type') == chromium_type:
                        # Fill in missing canvaskit commands
                        for k in range(ki, j):
                            aligned.append((None, canvaskit[k]))
                        ki = j
                        found_in_canvaskit = True
                        break

                if not found_in_canvaskit:
                    # Check if canvaskit type appears soon in chromium
                    found_in_chromium = False
                    for j in range(ci, min(ci + 10, len(chromium))):
                        if chromium[j].get('type') == canvaskit_type:
                            # Fill in missing chromium commands
                            for k in range(ci, j):
                                aligned.append((chromium[k], None))
                            ci = j
                            found_in_chromium = True
                            break

                    if not found_in_chromium:
                        # No match found, emit both as mismatched
                        aligned.append((chromium[ci], canvaskit[ki]))
                        ci += 1
                        ki += 1

        return aligned

    def diff(self, chromium: List[Dict], canvaskit: List[Dict], align_by: str = 'index') -> List[CommandDiff]:
        """Perform diff between two command lists."""
        self.diffs = []

        if align_by == 'type':
            aligned = self.align_by_type(chromium, canvaskit)
        else:
            aligned = self.align_by_index(chromium, canvaskit)

        for i, (c_cmd, k_cmd) in enumerate(aligned):
            if c_cmd is None:
                self.diffs.append(CommandDiff(
                    index=i,
                    diff_type=DiffType.MISSING_CHROMIUM,
                    chromium_cmd=None,
                    canvaskit_cmd=k_cmd,
                    differences=[f"Command only in CanvasKit: {k_cmd.get('type')}"]
                ))
            elif k_cmd is None:
                self.diffs.append(CommandDiff(
                    index=i,
                    diff_type=DiffType.MISSING_CANVASKIT,
                    chromium_cmd=c_cmd,
                    canvaskit_cmd=None,
                    differences=[f"Command only in Chromium: {c_cmd.get('type')}"]
                ))
            elif c_cmd.get('type') != k_cmd.get('type'):
                self.diffs.append(CommandDiff(
                    index=i,
                    diff_type=DiffType.TYPE_MISMATCH,
                    chromium_cmd=c_cmd,
                    canvaskit_cmd=k_cmd,
                    differences=[f"Type mismatch: {c_cmd.get('type')} vs {k_cmd.get('type')}"]
                ))
            else:
                match, differences = self.compare_commands(c_cmd, k_cmd)
                self.diffs.append(CommandDiff(
                    index=i,
                    diff_type=DiffType.MATCH if match else DiffType.MISMATCH,
                    chromium_cmd=c_cmd,
                    canvaskit_cmd=k_cmd,
                    differences=differences
                ))

        return self.diffs

    def get_summary(self) -> Dict:
        """Get summary statistics of the diff."""
        summary = {
            'total': len(self.diffs),
            'matches': sum(1 for d in self.diffs if d.diff_type == DiffType.MATCH),
            'mismatches': sum(1 for d in self.diffs if d.diff_type == DiffType.MISMATCH),
            'missing_chromium': sum(1 for d in self.diffs if d.diff_type == DiffType.MISSING_CHROMIUM),
            'missing_canvaskit': sum(1 for d in self.diffs if d.diff_type == DiffType.MISSING_CANVASKIT),
            'type_mismatches': sum(1 for d in self.diffs if d.diff_type == DiffType.TYPE_MISMATCH),
        }

        # Command type breakdown
        type_stats = {}
        for d in self.diffs:
            cmd_type = None
            if d.chromium_cmd:
                cmd_type = d.chromium_cmd.get('type')
            elif d.canvaskit_cmd:
                cmd_type = d.canvaskit_cmd.get('type')

            if cmd_type:
                if cmd_type not in type_stats:
                    type_stats[cmd_type] = {'total': 0, 'matches': 0, 'mismatches': 0}
                type_stats[cmd_type]['total'] += 1
                if d.diff_type == DiffType.MATCH:
                    type_stats[cmd_type]['matches'] += 1
                elif d.diff_type == DiffType.MISMATCH:
                    type_stats[cmd_type]['mismatches'] += 1

        summary['by_type'] = type_stats
        return summary

    def print_report(self, verbose: bool = False, summary_only: bool = False):
        """Print a human-readable diff report."""
        summary = self.get_summary()

        print("=" * 60)
        print("SKIA COMMAND DIFF REPORT")
        print("=" * 60)
        print()
        print(f"Total commands compared: {summary['total']}")
        print(f"  Matches:              {summary['matches']} ({100*summary['matches']/max(1,summary['total']):.1f}%)")
        print(f"  Mismatches:           {summary['mismatches']}")
        print(f"  Missing in Chromium:  {summary['missing_chromium']}")
        print(f"  Missing in CanvasKit: {summary['missing_canvaskit']}")
        print(f"  Type mismatches:      {summary['type_mismatches']}")
        print()

        print("By command type:")
        print("-" * 60)
        for cmd_type, stats in sorted(summary['by_type'].items(), key=lambda x: -x[1]['mismatches']):
            match_pct = 100 * stats['matches'] / max(1, stats['total'])
            if stats['mismatches'] > 0 or verbose:
                print(f"  {cmd_type:20} total={stats['total']:4}  matches={stats['matches']:4} ({match_pct:5.1f}%)  mismatches={stats['mismatches']:4}")
        print()

        if summary_only:
            return

        # Show differences
        mismatches = [d for d in self.diffs if d.diff_type != DiffType.MATCH]

        if mismatches:
            print("=" * 60)
            print("DIFFERENCES")
            print("=" * 60)
            print()

            for diff in mismatches[:50]:  # Limit to first 50 differences
                print(f"[{diff.index}] {diff.diff_type.value}")

                if diff.chromium_cmd:
                    print(f"  Chromium: {diff.chromium_cmd.get('type')}")
                if diff.canvaskit_cmd:
                    print(f"  CanvasKit: {diff.canvaskit_cmd.get('type')}")

                for d in diff.differences[:10]:  # Limit differences per command
                    print(f"    - {d}")

                if len(diff.differences) > 10:
                    print(f"    ... and {len(diff.differences) - 10} more differences")
                print()

            if len(mismatches) > 50:
                print(f"... and {len(mismatches) - 50} more differences")

    def to_json(self) -> Dict:
        """Export diff as JSON."""
        return {
            'summary': self.get_summary(),
            'diffs': [
                {
                    'index': d.index,
                    'diff_type': d.diff_type.value,
                    'chromium_cmd': d.chromium_cmd,
                    'canvaskit_cmd': d.canvaskit_cmd,
                    'differences': d.differences
                }
                for d in self.diffs
            ]
        }


def main():
    parser = argparse.ArgumentParser(
        description='Compare Skia command logs from Chromium and CanvasKit'
    )
    parser.add_argument('chromium', help='Chromium Skia commands JSON file')
    parser.add_argument('canvaskit', help='CanvasKit Skia commands JSON file')
    parser.add_argument('--output', '-o', help='Write detailed diff to JSON file')
    parser.add_argument('--verbose', '-v', action='store_true', help='Show all commands')
    parser.add_argument('--tolerance', '-t', type=float, default=0.001,
                        help='Numeric tolerance (default: 0.001)')
    parser.add_argument('--align-by', choices=['index', 'type'], default='index',
                        help='Command alignment method (default: index)')
    parser.add_argument('--summary', '-s', action='store_true',
                        help='Only show summary statistics')

    args = parser.parse_args()

    differ = SkiaCommandDiffer(tolerance=args.tolerance)

    try:
        chromium = differ.load_commands(args.chromium)
        canvaskit = differ.load_commands(args.canvaskit)
    except Exception as e:
        print(f"Error loading files: {e}", file=sys.stderr)
        sys.exit(1)

    print(f"Loaded {len(chromium)} Chromium commands, {len(canvaskit)} CanvasKit commands")
    print()

    differ.diff(chromium, canvaskit, align_by=args.align_by)
    differ.print_report(verbose=args.verbose, summary_only=args.summary)

    if args.output:
        with open(args.output, 'w') as f:
            json.dump(differ.to_json(), f, indent=2)
        print(f"Detailed diff written to: {args.output}")


if __name__ == '__main__':
    main()
