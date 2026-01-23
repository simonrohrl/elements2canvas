# Skia Paint - Chromium Paint Renderer

## Goal

Render web pages **exactly as Chromium does** by replaying Chromium's paint operations using CanvasKit (Skia compiled to WebAssembly).

The approach: **replicate Chromium's actual source code logic** rather than approximating behavior. Every algorithm in this renderer is derived directly from Chromium's C++ implementation.

---

## How It Works

```
┌─────────────────────────────────────────────────────────────────────────────┐
│                           CHROMIUM BROWSER                                  │
├─────────────────────────────────────────────────────────────────────────────┤
│  HTML/CSS/JS  →  DOM  →  Style  →  Layout  →  PAINT  →  Compositor  →  GPU  │
│                                              ↓                              │
│                                         (LOGGED)                            │
│                                              ↓                              │
│                                    raw_paint_ops.json                       │
└─────────────────────────────────────────────────────────────────────────────┘
                                              │
                                              ↓
┌─────────────────────────────────────────────────────────────────────────────┐
│                         painter.html (CanvasKit)                            │
├─────────────────────────────────────────────────────────────────────────────┤
│  Load raw_paint_ops.json  →  Apply Property Trees  →  Render to <canvas>    │
│                                                                             │
│  Uses same algorithms as Chromium (from chromium/ source files)             │
└─────────────────────────────────────────────────────────────────────────────┘
```

1. **Chromium logs paint operations** at the paint stage before compositing
2. **painter.html loads these operations** and renders them using CanvasKit
3. **Algorithms match Chromium's source** - transforms, shadows, blending all use the same logic

---

## Directory Structure

```
skia_paint/
├── ARCHITECTURE.md           # This documentation
├── painter.html              # The renderer (uses Chromium algorithms)
├── raw_paint_ops.json        # Paint operations exported from Chromium
├── reference_page.html       # Original HTML page for visual comparison
│
└── chromium/                 # Chromium source files (reference)
    ├── transform.cc/h        # 3D transforms, backface visibility
    ├── draw_looper.cc/h      # Shadow rendering
    └── effect_node.h         # Effects (opacity, blend modes)
```

---

## Chromium Algorithms Implemented

### 1. Backface Visibility Detection

**Source**: `transform.cc` lines 424-477

Determines if a transformed plane's back face is visible to the viewer. Uses the cofactor of the inverse-transpose matrix to transform the normal vector (0, 0, 1).

```cpp
// Chromium's approach: compute cofactor33 of inverse-transpose
double cofactor33 = rc(0,0)*rc(1,1)*rc(3,3) + rc(0,1)*rc(1,3)*rc(3,0) + ...
return cofactor33 * determinant < -kEpsilon;
```

### 2. 3D Transform Detection

**Source**: `transform.cc` lines 416-422

Checks if a transform creates 3D depth (affects Z coordinates):

```cpp
bool Transform::Creates3d() const {
  return matrix_.rc(2, 0) != 0 || matrix_.rc(2, 1) != 0 || matrix_.rc(2, 3) != 0;
}
```

### 3. Shadow Rendering (DrawLooper)

**Source**: `draw_looper.cc` lines 22-43

Shadows use a color filter with SrcIn blend mode to apply color while preserving alpha:

```cpp
paint->setColorFilter(SkColorFilters::Blend(color, SkColorSpace::MakeSRGB(),
                                            SkBlendMode::kSrcIn));
```

**Flags**:
- `kPostTransformFlag (1)` - Apply offset after canvas transform
- `kOverrideAlphaFlag (2)` - Set paint alpha to opaque
- `kDontModifyPaintFlag (4)` - Draw original content unmodified

---

## Data Format

### Paint Operations

Each operation references property tree nodes:

```json
{
  "type": "DrawRectOp",
  "rect": [100, 200, 300, 400],
  "flags": {
    "r": 0.2, "g": 0.5, "b": 0.8, "a": 1.0,
    "style": 0,
    "shadows": [{ "offsetX": 2, "offsetY": 2, "blurSigma": 4, ... }]
  },
  "transform_id": 5,
  "clip_id": 3,
  "effect_id": 2
}
```

### Property Trees

Transforms, clips, and effects are organized in trees:

```json
{
  "transform_tree": {
    "nodes": [
      { "id": 0, "parent_id": -1 },
      { "id": 1, "parent_id": 0, "matrix": [...], "origin": [50, 50, 0] }
    ]
  },
  "clip_tree": { "nodes": [...] },
  "effect_tree": { "nodes": [...] },
  "paint_ops": [...]
}
```

---

## Usage

```bash
cd skia_paint
python3 -m http.server 8000
# Open http://localhost:8000/painter.html
```

Enable **Debug Mode** to see detailed logging of shadow application, transform states, and property tree usage.

---

## Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                      painter.html                                │
├─────────────────────────────────────────────────────────────────┤
│                                                                  │
│  ┌─────────────────┐    ┌─────────────────┐    ┌─────────────┐ │
│  │  PropertyTrees  │    │ PaintOpRenderer │    │ Compositor  │ │
│  ├─────────────────┤    ├─────────────────┤    ├─────────────┤ │
│  │ getTransform()  │    │ drawRect()      │    │ drawFrame() │ │
│  │ isBackFaceVis() │◄───│ drawPath()      │◄───│ sortOps3D() │ │
│  │ shouldCull()    │    │ applyShadows()  │    │ applyTrees()│ │
│  └─────────────────┘    └─────────────────┘    └─────────────┘ │
│           │                      │                     │        │
│           ▼                      ▼                     ▼        │
│  ┌─────────────────────────────────────────────────────────────┐│
│  │                    CanvasKit (Skia WASM)                    ││
│  └─────────────────────────────────────────────────────────────┘│
└─────────────────────────────────────────────────────────────────┘

Chromium Source Reference:
┌──────────────┬──────────────┬──────────────┐
│ transform.cc │draw_looper.cc│effect_node.h │
└──────────────┴──────────────┴──────────────┘
```

---

## Extending

To implement additional Chromium rendering features, reference these source files:

| Feature | Chromium Source |
|---------|-----------------|
| Compositing | `cc/trees/layer_tree_impl.cc` |
| Render surfaces | `cc/trees/draw_property_utils.cc` |
| Filters | `cc/paint/paint_filter.cc` |
| Text shaping | `third_party/blink/renderer/platform/fonts/` |
| Borders | `third_party/blink/renderer/core/paint/box_border_painter.cc` |
