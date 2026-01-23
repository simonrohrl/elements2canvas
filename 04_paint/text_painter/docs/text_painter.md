# Text Painter

A standalone C++ implementation that reproduces Chromium's text rendering logic, enabling analysis and testing of text painting behavior in isolation from the full browser.

## Purpose

The text painter serves as a **pure functional text rendering system** that mimics Chromium's `TextFragmentPainter`. It takes structured input describing text, styling, and layout information, and produces a sequence of paint operations that would be executed by a graphics backend.

This allows:
- Testing text rendering logic without a full browser environment
- Analyzing how Chromium transforms text data into draw calls
- Validating rendering behavior with known inputs/outputs
- Running in WebAssembly for browser-based tooling

## How It Works

### Pipeline

```
JSON Input → Parse → TextPaintInput → Paint() → PaintOpList → JSON Output
```

1. **Input Parsing** - Reads JSON into a `TextPaintInput` struct containing text content, glyph data from HarfBuzz shaping, styling, and layout boxes
2. **Paint Execution** - `TextPainter::Paint()` processes the input and generates paint operations
3. **Output Serialization** - Paint operations are serialized to JSON for inspection or further processing

### Paint Order

The `Paint()` method follows Chromium's rendering order:

1. Visibility and flow control checks
2. Symbol marker rendering (bullets, disclosure triangles)
3. SVG transform and writing mode handling
4. Text origin computation (baseline positioning)
5. **Decorations** (underlines, overlines) - painted *before* text
6. **Text glyphs** via `DrawTextBlobOp`
7. **Line-through decoration** - painted *after* text for correct overlap
8. Emphasis marks (Japanese/Chinese text)

## Input Structure

The primary input is a `TextPaintInput` containing:

| Field | Description |
|-------|-------------|
| `fragment` | Text string, character range, and shaped glyph data |
| `box` | Physical layout rectangle (x, y, width, height) |
| `style` | Colors (fill, stroke, emphasis), shadows, stroke width |
| `decorations` | Underline/overline/line-through with style and color |
| `state_ids` | Property tree IDs (transform, clip, effect) |
| `writing_mode` | Horizontal or vertical text direction |
| `visibility` | Visible, hidden, or collapsed |

See `test/input.json` for a complete example.

## Output

The output is an array of paint operations:

| Operation | Purpose |
|-----------|---------|
| `DrawTextBlobOp` | Renders text glyphs |
| `DrawLineOp` | Solid/double decoration lines |
| `DrawStrokeLineOp` | Dotted/dashed decoration lines |
| `DrawWavyLineOp` | Wavy decorations (spelling/grammar errors) |
| `FillEllipseOp` | Disc bullet markers |
| `FillPathOp` | Disclosure triangle markers |

## Building

Native build:
```bash
make
./build/text_painter -i test/input.json
```

WebAssembly build (requires Emscripten):
```bash
make -f Makefile.wasm
```

## Directory Structure

```
text_painter/
├── src/        # Source files
├── test/       # Test JSON inputs
├── reference/  # Original Chromium source for reference
├── docs/       # Documentation
└── build/      # Build outputs (generated)
```
