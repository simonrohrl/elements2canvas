# Block Painter

A standalone C++ implementation that reproduces Chromium's block rendering logic, converting block layout information into paint operations.

## Purpose

The block painter is a **pure functional paint operation generator** that transforms high-level DOM block styling information into low-level paint commands. It handles:

- Basic rectangular fills
- Rounded rectangles with border radius
- Box shadows (outer shadows)
- CSS visibility states
- Property tree integration (transforms, clips, effects)

This allows analysis of how Chromium renders CSS backgrounds and box shadows without running a full browser.

## How It Works

### Pipeline

```
JSON Input → Parse → BlockPaintInput → Paint() → PaintOpList → JSON Output
```

### Processing Flow

1. **Visibility Check** - Hidden/collapsed blocks produce no paint operations
2. **Color Check** - No background color means nothing to paint
3. **Build Paint Flags** - Convert colors and shadows to drawing flags
4. **Shape Selection**:
   - Border radii present → `DrawRRectOp` (rounded rectangle)
   - No radii → `DrawRectOp` (simple rectangle)
5. **Output** - Embeds property tree IDs for composition

## Input Structure

The `BlockPaintInput` contains:

| Field | Description |
|-------|-------------|
| `geometry` | Box position and dimensions (x, y, width, height) |
| `background_color` | RGBA fill color |
| `border_radii` | 8 corner radius values [tl_x, tl_y, tr_x, tr_y, br_x, br_y, bl_x, bl_y] |
| `box_shadow` | Array of shadows with offset, blur, spread, color |
| `visibility` | `visible`, `hidden`, or `collapse` |
| `node_id` | DOM node identifier |
| `state_ids` | Property tree IDs (transform_id, clip_id, effect_id) |

### Example Input

```json
{
  "geometry": { "x": 100, "y": 100, "width": 200, "height": 150 },
  "background_color": { "r": 0.2, "g": 0.4, "b": 0.8, "a": 1.0 },
  "border_radii": [10, 10, 10, 10, 10, 10, 10, 10],
  "box_shadow": [{
    "offset_x": 4,
    "offset_y": 4,
    "blur": 8,
    "spread": 0,
    "color": { "r": 0, "g": 0, "b": 0, "a": 0.3 },
    "inset": false
  }],
  "visibility": "visible",
  "node_id": 42,
  "state_ids": { "transform_id": 1, "clip_id": 1, "effect_id": 1 }
}
```

## Output

Paint operations generated:

| Operation | Purpose |
|-----------|---------|
| `DrawRectOp` | Simple rectangle fill |
| `DrawRRectOp` | Rounded rectangle fill |

Each operation includes color, shadows (as blur sigma = blur/2), and property tree IDs.

## Building

```bash
make
./build/block_painter -i test/input.json
```

## Command Line

```
block_painter [-i input.json] [-o output.json]

-i <file>    Input JSON file (default: input.json)
-o <file>    Output JSON file (default: stdout)
-h, --help   Show help message
```

## Limitations

- Inset shadows are currently skipped (require different rendering approach)
- Only handles background fills, not borders (see border_painter)

## Directory Structure

```
block_painter/
├── src/        # Source files
├── test/       # Test JSON inputs
├── docs/       # Documentation
└── build/      # Build outputs (generated)
```
