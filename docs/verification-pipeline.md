# Verification Pipeline

## Overview

The verification pipeline (`eval/verification-pipeline.sh`) is an end-to-end automation script that builds a patched Chromium, captures its compositor layer tree data, and extracts it for visualization. This enables verification that Chromium's paint operations are being correctly serialized and can be replayed in `test.html`.

## Pipeline Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                        verification-pipeline.sh                              │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  [1/4] Build Chromium                                                        │
│        scripts/build.sh                                                      │
│        - Runs autoninja to compile patched Chromium                         │
│        - Patches include paint op serialization in raster_source.cc         │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  [2/4] Start Chromium (8 seconds default)                                   │
│        scripts/start.sh                                                      │
│        - Opens eval/sample.html in fullscreen                               │
│        - Clears previous chrome_debug.log                                   │
│        - Chromium writes layer tree JSON to debug log                       │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  [3/4] Copy Debug Log                                                        │
│        eval/copy_debug_log.sh                                                │
│        - Copies ~/Library/Application Support/Chromium/chrome_debug.log     │
│        - Places it in project root as chrome_debug.log                      │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  [4/4] Extract JSON                                                          │
│        eval/extract_json.sh                                                  │
│        - Parses chrome_debug.log with Python regex                          │
│        - Extracts property_trees.json (transform/effect/clip trees)         │
│        - Extracts layers.json (LayerTreeImpl with paint operations)         │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  Output Files                                                                │
│        property_trees.json  - Transform, effect, clip node hierarchies      │
│        layers.json          - Layer list with serialized paint ops          │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  Visualization                                                               │
│        test.html                                                             │
│        - Loads layers.json and property_trees.json                          │
│        - Uses CanvasKit (Skia WASM) to replay paint operations              │
│        - Renders the same visual output as Chromium                         │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Usage

```bash
# Run with default 8-second browser duration
./eval/verification-pipeline.sh

# Run with custom duration (e.g., 15 seconds)
./eval/verification-pipeline.sh 15
```

## Input: sample.html

The test page (`eval/sample.html`) contains a variety of CSS features to exercise the paint serialization:

- **Transforms**: `rotate()`, `scale()`, `perspective()`, `skewX()`
- **Gradients**: `linear-gradient()` backgrounds
- **Opacity**: Various opacity levels (100%, 75%, 50%, 25%)
- **Clip paths**: `clip-path: polygon()`
- **Rounded corners**: `border-radius` with nested elements
- **Box shadows**: `box-shadow` on card elements
- **Form elements**: Inputs, selects, checkboxes, radio buttons
- **Overlays**: Semi-transparent positioned elements

## Output Files

### property_trees.json

Contains the compositor property trees that define how layers are transformed, clipped, and composited:

```json
{
  "transform_tree": {
    "nodes": [
      { "id": 0, "parent_id": -1, "local": [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1] },
      { "id": 1, "parent_id": 0, "local": [...] }
    ]
  },
  "effect_tree": {
    "nodes": [
      { "id": 0, "parent_id": -1, "opacity": 1 },
      { "id": 1, "parent_id": 0, "opacity": 0.5 }
    ]
  },
  "clip_tree": {
    "nodes": [...]
  }
}
```

### layers.json

Contains the layer tree with serialized paint operations:

```json
{
  "LayerTreeImpl": [
    {
      "id": 1,
      "bounds": { "width": 800, "height": 600 },
      "transform_id": 1,
      "effect_id": 1,
      "clip_id": 1,
      "paint_ops": [
        {
          "type": "DrawRectOp",
          "rect": [0, 0, 100, 50],
          "flags": { "r": 1, "g": 0, "b": 0, "a": 1 }
        },
        {
          "type": "DrawTextBlobOp",
          "x": 10, "y": 30,
          "runs": [
            {
              "glyphCount": 5,
              "glyphs": [72, 101, 108, 108, 111],
              "positioning": 1,
              "positions": [0, 8, 16, 24, 32]
            }
          ]
        }
      ]
    }
  ]
}
```

## Supported Paint Operations

The serialization (in `cc/raster/raster_source.cc`) supports:

| Operation | Serialized Data |
|-----------|-----------------|
| `DrawRectOp` | rect bounds, color, style, gradient shader |
| `DrawRRectOp` | rect bounds, corner radii, color, style |
| `DrawPathOp` | SVG path string, fill type, color, style |
| `DrawTextBlobOp` | glyph IDs, positions (H/Full/RSXform), font info, color |
| `DrawOvalOp` | oval bounds |
| `DrawLineOp` | start/end points |
| `DrawColorOp` | RGBA color |
| `ClipRectOp` | rect bounds, anti-alias flag |
| `ClipPathOp` | SVG path string, bounds |
| `SaveOp` / `RestoreOp` | canvas state management |
| `SaveLayerAlphaOp` | alpha value, bounds |
| `TranslateOp` | dx, dy |
| `ScaleOp` | sx, sy |
| `RotateOp` | degrees |
| `ConcatOp` | 4x4 matrix |
| `SetMatrixOp` | 4x4 matrix |

## Visualization: test.html

The `test.html` file uses CanvasKit (Skia compiled to WebAssembly) to replay the paint operations:

1. Loads `layers.json` and `property_trees.json`
2. Parses the property trees to build transform/effect/clip hierarchies
3. For each layer:
   - Applies the accumulated transform from the transform tree
   - Applies opacity from the effect tree
   - Applies clipping from the clip tree
   - Replays each paint operation using CanvasKit APIs

This allows visual comparison between the original Chromium rendering and the replayed version to verify serialization accuracy.

## Troubleshooting

### No JSON extracted
- Ensure Chromium built successfully with the patches
- Check that `chrome_debug.log` contains `property_trees:` and `LayerTreeImpl` markers
- Increase browser duration if page hasn't finished loading

### Missing paint operations
- Some operations may not be serialized yet (e.g., `DrawImageRectOp`)
- Check `raster_source.cc` for the `SerializePaintOpRecursive` function

### Visual differences in test.html
- Font rendering may differ (CanvasKit uses bundled fonts)
- Some Skia features may not have exact CanvasKit equivalents
- Check browser console for errors in paint op handling

## Related Documentation

- [paint-op-json-serialization.md](paint-op-json-serialization.md) - Details on paint op serialization format
- [layer-tree-host-architecture.md](layer-tree-host-architecture.md) - Compositor architecture
- [blink-paint-system.md](blink-paint-system.md) - How Blink generates paint ops
