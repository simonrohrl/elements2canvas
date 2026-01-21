# Chromium DOM Tracing - Paint System Architecture

## Project Overview

This project creates an **alternative rendering path** for Chromium's paint output. The goal is to bypass Chromium's complex compositor and rasterization stages by:

1. **Extracting raw paint operations** from Chromium's paint stage (`raw_paint_ops.json`)
2. **Replaying them in JavaScript** using CanvasKit (Skia's WebAssembly port)
3. **Verifying visual correctness** by comparing the JS-rendered output against Chromium's actual browser output

This enables a fully reproducible rendering pipeline that is much simpler to understand and debug than Chromium's full compositor stack, at the cost of being slower.

---

## High-Level Architecture

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           CHROMIUM BROWSER                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│  HTML/CSS/JS  →  DOM  →  Style  →  Layout  →  PAINT  →  Compositor  →  GPU  │
│                                              ↓                              │
│                                         (LOGGED)                            │
│                                              ↓                              │
│                                    raw_paint_ops.json                       │
│                                    property_trees.json                      │
└─────────────────────────────────────────────────────────────────────────────┘
                                              │
                                              ↓
┌─────────────────────────────────────────────────────────────────────────────┐
│                           test.html (CanvasKit)                             │
├─────────────────────────────────────────────────────────────────────────────┤
│  Load raw_paint_ops.json  →  Apply Property Trees  →  Render to <canvas>    │
│                                                                             │
│  Output: Visual rendering that should match the original browser output     │
└─────────────────────────────────────────────────────────────────────────────┘
```

---

## Why This Approach?

### The Problem

Chromium's rendering pipeline after paint is extremely complex:

```
Paint Stage → PaintArtifact → CC Layer Tree → Compositor Tiles →
  GPU Rasterization → Frame Submission → Display
```

This involves:
- Layer compositing decisions
- Tile management
- GPU texture allocation
- Multi-threaded scheduling
- Async rasterization

Understanding or debugging this is difficult and requires deep knowledge of Chromium internals.

### The Solution

By capturing paint operations at the **earliest point after paint** (in `PaintController::CommitNewDisplayItems()`), we get:

1. **All drawing commands** in their final form
2. **Property trees** (transforms, clips, effects) that define how to compose them
3. **Complete data** before any compositor optimizations

This data is then rendered via CanvasKit, which is Skia compiled to WebAssembly - the same graphics library Chromium uses internally.

**Key insight**: If the test.html output matches browser output, we've proven that:
- The logged data is complete and correct
- The property tree application is correct
- The paint ops interpretation is correct

---

## Data Flow

### 1. Chromium Logging (Input Generation)

**Location**: `paint_controller.cc` → `PaintController::CommitNewDisplayItems()`

```cpp
// After paint chunks are finalized, before compositor
auto raw_paint_ops_json = SerializeRawPaintOps(paint_artifact);
LOG(ERROR) << "RAW_PAINT_OPS: " << raw_paint_ops_json->ToJSONString().Utf8();
```

**What gets logged**:

| File | Description |
|------|-------------|
| `raw_paint_ops.json` | All paint operations with property tree IDs |
| `property_trees.json` | Transform, clip, and effect trees |
| `layout_tree.json` | LayoutObject hierarchy with geometry |

### 2. Data Extraction

**Script**: `eval/extract_json.sh`

Parses Chrome's debug log to extract JSON data:
```bash
# Searches for markers like "RAW_PAINT_OPS: {...}"
# Extracts and saves as separate JSON files
```

### 3. JavaScript Replay (test.html)

**Purpose**: Replay paint operations using CanvasKit

**Key Classes**:

```javascript
// Property tree management - stores transform/clip/effect nodes
class PropertyTrees {
    getTransformMatrix(nodeId)  // Accumulated 4x4 matrix
    getEffect(nodeId)           // Opacity, blend mode
    getClip(nodeId)             // Clip rectangles/paths
}

// Individual paint operation rendering
class PaintOpRenderer {
    renderOp(canvas, op)        // Dispatches to specific handlers
    drawRect(canvas, op)
    drawRRect(canvas, op)
    drawTextBlob(canvas, op)
    drawPath(canvas, op)
    // ... etc
}

// Main compositor - applies property trees before rendering ops
class RawPaintOpsCompositor {
    drawFrame(paintOps, trees) {
        for (op of paintOps) {
            // 1. Apply clip from property tree
            // 2. Apply transform from property tree
            // 3. Apply effect (opacity/blend) from property tree
            // 4. Render the paint operation
        }
    }
}
```

---

## Paint Operations Format

### Operation Types

| Type | Description | Key Fields |
|------|-------------|------------|
| `DrawRectOp` | Filled/stroked rectangle | `rect`, `flags` (color, style) |
| `DrawRRectOp` | Rounded rectangle | `rect`, `radii`, `flags` |
| `DrawDRRectOp` | Double rounded rect (borders) | `outer_rect`, `inner_rect`, `radii` |
| `DrawTextBlobOp` | Text with positioned glyphs | `x`, `y`, `runs[]` with glyph data |
| `DrawPathOp` | SVG-style path | `path` (SVG string), `fillType` |
| `DrawLineOp` | Line segment | `x0`, `y0`, `x1`, `y1` |
| `ClipRectOp` | Rectangular clip | `rect`, `clipOp` |
| `ClipRRectOp` | Rounded rect clip | `rect`, `radii`, `clipOp` |
| `ClipPathOp` | Path-based clip | `path` |
| `SaveOp` / `RestoreOp` | Canvas state management | - |
| `SaveLayerOp` | Isolated layer for effects | `bounds`, `flags` |
| `TranslateOp` / `ConcatOp` | Transform operations | `dx`, `dy` / `matrix` |

### Example Paint Op

```json
{
  "type": "DrawRectOp",
  "rect": [100, 200, 300, 400],
  "flags": {
    "r": 0.2, "g": 0.5, "b": 0.8, "a": 1.0,
    "style": 0,
    "strokeWidth": 0
  },
  "transform_id": 5,
  "clip_id": 3,
  "effect_id": 2
}
```

### Property Tree IDs

Each paint op references nodes in three property trees:

- **`transform_id`**: Points to a transform node (translation, matrix)
- **`clip_id`**: Points to a clip node (rect, path, or CSS clip-path)
- **`effect_id`**: Points to an effect node (opacity, blend mode)

---

## Property Trees

### Transform Tree

```json
{
  "transform_tree": {
    "nodes": [
      { "id": 0, "parent_id": -1 },
      { "id": 1, "parent_id": 0, "translation2d": [100, 50] },
      { "id": 2, "parent_id": 1, "matrix": [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1] }
    ]
  }
}
```

**Application**: Walk from node to root, accumulate matrices.

### Clip Tree

```json
{
  "clip_tree": {
    "nodes": [
      { "id": 0, "parent_id": -1 },
      { "id": 1, "parent_id": 0, "clip_rect": [0, 0, 800, 600] },
      { "id": 2, "parent_id": 1, "clip_path": "M10,10 L100,10 L100,100 Z" }
    ]
  }
}
```

**Application**: `canvas.clipRect()` or `canvas.clipPath()`.

### Effect Tree

```json
{
  "effect_tree": {
    "nodes": [
      { "id": 0, "parent_id": -1, "opacity": 1.0, "blend_mode": "SrcOver" },
      { "id": 1, "parent_id": 0, "opacity": 0.5, "blend_mode": "Multiply" }
    ]
  }
}
```

**Application**: `canvas.saveLayer()` with alpha/blend mode.

---

## Verification Process

### Goal

Verify that `test.html` produces **pixel-identical** (or near-identical) output to the browser.

### Success Criteria

- Visual match confirms the logged data is complete
- Any discrepancy indicates missing or incorrect serialization or false logic in test.html

---

## File Structure

```
chromium-dom-tracing/
├── test.html                 # CanvasKit renderer (main visualizer)
├── raw_paint_ops.json        # Paint operations from Chromium
├── property_trees.json       # Transform/Clip/Effect trees
├── Arial.ttf                 # Font for text rendering
│
├── eval/
│   ├── extract_json.sh       # Parse Chrome debug log
│   ├── verification-pipeline.sh
│   └── compare_paint_ops.py
│
├── docs/
│   ├── PROJECT_ARCHITECTURE.md     # This file
│   └── paint-system-plan.md        # Future paint system implementation
│
└── chromium/
    └── src/                  # Chromium source. Modify here for extended logging.
```

---

## Chromium Patch Overview

The patch modifies several files:

### 1. `paint_controller.cc`
Logs `raw_paint_ops.json` after paint is complete:
```cpp
auto raw_paint_ops_json = SerializeRawPaintOps(paint_artifact);
LOG(ERROR) << "RAW_PAINT_OPS: " << ...;
```

### 2. `paint_artifact_serializer.cc` (new)
Implements serialization for:
- Paint operations (DrawRectOp, DrawTextBlobOp, etc.)
- Property trees (transforms, clips, effects)
- Paint flags (colors, gradients, shadows)

---

## Key Implementation Details

### Text Rendering

Text is the most complex operation. `DrawTextBlobOp` contains:

```json
{
  "type": "DrawTextBlobOp",
  "x": 100.0,
  "y": 200.0,
  "runs": [{
    "glyphCount": 5,
    "glyphs": [72, 101, 108, 108, 111],
    "positions": [{"x": 0, "y": 0}, {"x": 10, "y": 0}, ...],
    "font": {
      "family": "Arial",
      "size": 16,
      "weight": 400
    }
  }],
  "flags": { "r": 0, "g": 0, "b": 0, "a": 1 }
}
```

CanvasKit renders this using `TextBlob.MakeFromRSXformGlyphs()`.

### Shadow Rendering (DrawLooper)

Box shadows and text shadows use Chromium's `DrawLooper`:

```json
{
  "flags": {
    "shadows": [{
      "offsetX": 2, "offsetY": 2,
      "blurSigma": 4,
      "r": 0, "g": 0, "b": 0, "a": 0.5,
      "flags": 0
    }]
  }
}
```

### Gradient Shaders

```json
{
  "flags": {
    "shaderType": "kLinearGradient",
    "gradientColors": [
      {"r": 1, "g": 0, "b": 0, "a": 1},
      {"r": 0, "g": 0, "b": 1, "a": 1}
    ],
    "gradientPositions": [0, 1],
    "startPoint": [0, 0],
    "endPoint": [100, 0]
  }
}
```

---

## Debugging Tips

### Enable Debug Mode

In test.html, check "Debug Mode" checkbox to see:
- Individual operation logs
- Transform/clip/effect state changes
- Shadow application details

### Common Issues

| Issue | Likely Cause |
|-------|--------------|
| Missing elements | Incorrect clip application |
| Wrong position | Transform tree not being accumulated correctly |
| Black rectangles | Missing shader data or unsupported shader type |
| Missing text | Font file not loaded or glyph data incomplete |
| Wrong opacity | Effect tree not being applied |

### Inspecting Data

```javascript
// In browser console on test.html
console.log(paintOps.length);  // Number of operations
console.log(paintOps.filter(op => op.type === 'DrawTextBlobOp'));  // All text ops
console.log(propertyTrees.transformNodes);  // Transform tree
```

## Running the System

```bash
./eval/verification-pipeline.sh
```