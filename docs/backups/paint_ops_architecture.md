# Chromium DOM Tracing - Paint Ops Visualizer

## Project Overview

This project aims to **extract, serialize, and replay Chromium's internal paint operations** outside of the browser. The goal is to emulate Skia-based rendering that Chromium performs internally, allowing visualization and analysis of how web pages are actually painted.

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────────────┐
│                         CHROMIUM BROWSER                                 │
│  ┌─────────────────┐    ┌──────────────────┐    ┌───────────────────┐  │
│  │  Blink Renderer │ -> │  PaintController │ -> │ Paint Ops (Skia)  │  │
│  │  (DOM -> Layout)│    │  (paint_ops.cc)  │    │ DrawRect, etc.    │  │
│  └─────────────────┘    └────────┬─────────┘    └───────────────────┘  │
│                                  │                                       │
│                         Serialization (JSON)                            │
│                                  │                                       │
└──────────────────────────────────┼──────────────────────────────────────┘
                                   │
                                   ▼
                         ┌─────────────────┐
                         │ raw_paint_ops   │
                         │    .json        │
                         └────────┬────────┘
                                  │
                                  ▼
┌─────────────────────────────────────────────────────────────────────────┐
│                          test.html VISUALIZER                           │
│  ┌─────────────────┐    ┌──────────────────┐    ┌───────────────────┐  │
│  │  PropertyTrees  │    │ PaintOpRenderer  │    │   CanvasKit       │  │
│  │  (transforms,   │ -> │ (interprets ops) │ -> │   (Skia WASM)     │  │
│  │  effects, clips)│    │                  │    │                   │  │
│  └─────────────────┘    └──────────────────┘    └───────────────────┘  │
└─────────────────────────────────────────────────────────────────────────┘
```

## Components

### 1. Chromium Paint Instrumentation (`paint_controller.cc`)

The Chromium source code is instrumented to log paint operations as they are recorded. The `PaintController` in Blink is responsible for:

- Recording display items (paint commands) during the paint phase
- Managing paint chunks (groups of operations sharing the same property state)
- Maintaining property trees for transforms, effects, and clips

The instrumentation serializes these paint ops to JSON format including:
- Operation type (`DrawRectOp`, `DrawRRectOp`, `DrawTextBlobOp`, etc.)
- Geometry data (rectangles, paths, radii)
- Paint flags (colors, styles, gradients, shadows)
- Property tree references (`transform_id`, `effect_id`, `clip_id`)

### 2. Serialized Data (`raw_paint_ops.json`)

The JSON file contains:

```json
{
  "paint_ops": [
    {
      "type": "DrawRectOp",
      "rect": [0, 0, 2530, 8900],
      "flags": { "r": 1, "g": 1, "b": 1, "a": 1, "style": 0 },
      "transform_id": 5,
      "clip_id": 1,
      "effect_id": 0
    },
    // ... more ops
  ],
  "transform_tree": {
    "nodes": [
      { "id": 0, "parent_id": -1 },
      { "id": 6, "parent_id": 5, "translation2d": [80, 284] },
      { "id": 7, "parent_id": 6, "matrix": [1.159, 0.310, ...] }
    ]
  },
  "clip_tree": {
    "nodes": [
      { "id": 0, "parent_id": -1, "clip_rect": [...] }
    ]
  },
  "effect_tree": {
    "nodes": [
      { "id": 0, "parent_id": -1, "opacity": 1, "blend_mode": "SrcOver" },
      { "id": 2, "parent_id": 1, "opacity": 0.75, "blend_mode": "SrcOver" }
    ]
  }
}
```

### 3. Visualizer (`test.html`)

The HTML file implements a complete paint ops replay engine using **CanvasKit** (Skia compiled to WebAssembly).

#### Key Classes

**`PropertyTrees`** (lines 44-106)
- Parses and stores the three property trees (transform, effect, clip)
- Provides `getTransformMatrix(nodeId)` - walks the transform tree from node to root, accumulating transformations
- Supports `matrix` (full 4x4), `local`, and `translation2d` transform types
- Provides `getEffect(index)` for opacity and blend mode lookups
- Provides `getClip(index)` for clip rectangles

**`PaintOpRenderer`** (lines 112-700)
- Core rendering engine that interprets and executes paint operations
- Implements all Chromium paint op types:
  - **State ops**: `SaveOp`, `RestoreOp`, `TranslateOp`, `ScaleOp`, `RotateOp`, `ConcatOp`, `SetMatrixOp`
  - **Clip ops**: `ClipRectOp`, `ClipRRectOp`, `ClipPathOp`
  - **Draw ops**: `DrawRectOp`, `DrawRRectOp`, `DrawDRRectOp`, `DrawPathOp`, `DrawColorOp`, `DrawTextBlobOp`, `DrawImageRectOp`, `DrawRecordOp`
  - **Layer ops**: `SaveLayerOp`, `SaveLayerAlphaOp`

- Handles advanced paint features:
  - **Gradients**: Linear and radial gradients with color stops
  - **Shadows**: DrawLooper-based shadow rendering with blur, offset, and color
  - **Stroke styles**: Including dash patterns for dotted/dashed borders
  - **Text rendering**: Glyph-based text with RSXform positioning

**`RawPaintOpsCompositor`** (lines 706-802)
- Orchestrates the rendering of the flat paint ops list
- Manages property tree state changes between operations:
  - When `transform_id` changes: resets canvas matrix and applies new accumulated transform
  - When `effect_id` changes: manages saveLayer/restore for opacity and blend modes
- Efficiently tracks current state to minimize redundant state changes

#### Rendering Flow

1. **Initialization**
   - Load CanvasKit WASM module
   - Fetch `raw_paint_ops.json`
   - Load Arial.ttf for text rendering
   - Create Skia surface sized to fit all paint ops

2. **Property Tree Setup**
   - Parse transform, effect, and clip trees
   - Build node maps for O(1) lookups

3. **Frame Rendering** (`drawFrame`)
   ```
   For each paint op:
     1. Check if transform_id changed → restore/save canvas, apply new transform matrix
     2. Check if effect_id changed → close effect layers, open new ones if needed
     3. Render the operation via PaintOpRenderer
   ```

4. **Op Execution** (example: `DrawRRectOp`)
   - Parse rectangle and corner radii
   - Apply shadows first (if present) using blur filters
   - Set up paint with color/gradient/style
   - Apply dash pattern for stroked borders
   - Draw rounded rectangle

## Property Trees Explained

Chromium uses **property trees** to efficiently manage visual properties that apply to groups of elements:

### Transform Tree
- Each node represents a coordinate space
- Nodes inherit transforms from parents
- Types: identity, 2D translation, full 4x4 matrix
- Used for CSS transforms, scrolling, positioning

### Effect Tree
- Manages opacity and blend modes
- Nodes with opacity < 1.0 require layer isolation
- Blend modes: SrcOver (default), Multiply, Screen, Overlay, etc.
- Used for CSS opacity, mix-blend-mode

### Clip Tree
- Defines clipping regions
- Can be rectangles or complex paths
- Clips are intersected down the tree
- Used for overflow: hidden, clip-path

## Supported Paint Operations

| Operation | Description | Features |
|-----------|-------------|----------|
| `DrawRectOp` | Filled/stroked rectangle | Gradients, shadows, dash patterns |
| `DrawRRectOp` | Rounded rectangle | All of above + corner radii |
| `DrawDRRectOp` | Double rounded rect (ring) | Border rendering |
| `DrawPathOp` | SVG path | Fill types, stroke styles |
| `DrawTextBlobOp` | Glyph-based text | RSXform positioning, font styling |
| `DrawColorOp` | Fill entire canvas | Blend modes |
| `DrawRecordOp` | Nested paint ops | Recursive rendering |
| `SaveLayerOp` | Create compositing layer | Alpha, bounds |
| `ClipRectOp` | Rectangular clip | Anti-aliasing |
| `ClipRRectOp` | Rounded rect clip | Anti-aliasing |
| `ClipPathOp` | Path-based clip | SVG path support |

## Use Cases

1. **Visual Debugging** - See exactly what Chromium paints and in what order
2. **Performance Analysis** - Count operations, identify expensive patterns
3. **Rendering Verification** - Compare Chromium output with replay
4. **Education** - Understand how CSS becomes paint operations
5. **Testing** - Verify paint behavior without running full browser

## Files

| File | Purpose |
|------|---------|
| `test.html` | Main visualizer with CanvasKit rendering |
| `raw_paint_ops.json` | Serialized paint operations and property trees |
| `Arial.ttf` | Font file for text rendering |
| `chromium/...paint_controller.cc` | Chromium source with logging instrumentation |

## Running the Visualizer

1. Serve the directory with a local HTTP server (required for fetch):
   ```bash
   python3 -m http.server 8000
   ```

2. Open `http://localhost:8000/test.html` in a browser

3. The visualizer will:
   - Load CanvasKit WASM
   - Fetch and parse `raw_paint_ops.json`
   - Render all paint operations to the canvas
   - Display statistics in the info panel

4. Enable "Debug Mode" checkbox for detailed operation logging
