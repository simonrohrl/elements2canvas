# Border Painter

A standalone C++ implementation that replicates Chromium's BoxBorderPainter logic, converting CSS border properties into paint operations.

## Purpose

The border painter transforms CSS border specifications into low-level paint commands. It supports:

- All CSS border styles (solid, dashed, dotted, double, groove, ridge, inset, outset)
- Per-side border colors and widths
- Border radius (rounded corners)
- Fast path optimization for uniform borders
- 3D border effects (groove, ridge, inset, outset)

This enables analysis and verification of how Chromium renders CSS borders.

## How It Works

### Pipeline

```
JSON Input → Parse → BorderPaintInput → Paint() → PaintOpList → JSON Output
```

### Rendering Strategies

The painter analyzes borders and chooses the optimal rendering approach:

**Fast Path** (uniform borders):
- Single stroked rect/rrect for uniform solid borders
- Filled DRRect for non-uniform width with radii

**Slow Path** (non-uniform borders):
- Each side painted individually
- Thin borders (< 10px) → filled rectangles
- Thick borders (≥ 10px) → stroked lines

**Special Styles**:
- **Double**: Two parallel stroked shapes with stroke_width = ceil(border_width/3)
- **Dotted**: Stroked lines with round dot pattern
- **Dashed**: Stroked lines with dash pattern (3:1 ratio)
- **Groove/Ridge**: Paired thin rects with darkened/lightened colors
- **Inset/Outset**: Color adjustment for 3D pressed/raised effect

## Input Structure

The `BorderPaintInput` contains:

| Field | Description |
|-------|-------------|
| `geometry` | Box position and dimensions (x, y, width, height) |
| `border_widths` | Width for each side (top, right, bottom, left) |
| `border_colors` | RGBA color for each side |
| `border_styles` | CSS style for each side (solid, dashed, dotted, etc.) |
| `border_radii` | 8 corner radius values |
| `visibility` | `visible`, `hidden`, or `collapse` |
| `node_id` | DOM node identifier |
| `state_ids` | Property tree IDs (transform_id, clip_id, effect_id) |
| `render_hint` | Strategy hint to verify Chromium's rendering choice |

### Example Input

```json
{
  "geometry": { "x": 50, "y": 50, "width": 200, "height": 100 },
  "border_widths": { "top": 2, "right": 2, "bottom": 2, "left": 2 },
  "border_colors": {
    "top": { "r": 0, "g": 0, "b": 0, "a": 1 },
    "right": { "r": 0, "g": 0, "b": 0, "a": 1 },
    "bottom": { "r": 0, "g": 0, "b": 0, "a": 1 },
    "left": { "r": 0, "g": 0, "b": 0, "a": 1 }
  },
  "border_styles": {
    "top": "solid",
    "right": "solid",
    "bottom": "solid",
    "left": "solid"
  },
  "visibility": "visible",
  "node_id": 123,
  "state_ids": { "transform_id": 1, "clip_id": 1, "effect_id": 1 }
}
```

## Output

Paint operations generated:

| Operation | Purpose |
|-----------|---------|
| `DrawRectOp` | Stroked/filled rectangle |
| `DrawRRectOp` | Stroked/filled rounded rectangle |
| `DrawLineOp` | Line segment (for individual sides, dotted/dashed) |
| `DrawDRRectOp` | Filled double rounded rect (outer - inner) |

Each operation includes stroke properties (width, cap, join, dash pattern) and property tree IDs.

## Building

```bash
make
./build/border_painter -i test/input.json
```

## Command Line

```
border_painter [-i input.json] [-o output.json]

-i <file>    Input JSON file (default: input.json)
-o <file>    Output JSON file (default: stdout)
-h, --help   Show help message
```

## Border Style Support

| Style | Rendering |
|-------|-----------|
| `solid` | Single uniform stroke |
| `double` | Two parallel stroked shapes |
| `dotted` | Round dot pattern (dash = stroke width) |
| `dashed` | Dash pattern (3x on, 1x off) |
| `groove` | 3D beveled inward effect |
| `ridge` | 3D beveled outward effect |
| `inset` | 3D pressed effect |
| `outset` | 3D raised effect |
| `none`/`hidden` | No rendering |

## Directory Structure

```
border_painter/
├── src/        # Source files
├── test/       # Test JSON inputs
├── reference/  # Chromium reference code
├── docs/       # Documentation
└── build/      # Build outputs (generated)
```
