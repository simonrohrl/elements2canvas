# WASM Paint Module Specification

## Overview

A WebAssembly module that takes a serialized fragment tree and style information as input and produces a layer tree as output.

```
┌─────────────────┐         ┌─────────────────┐         ┌─────────────────┐
│   PaintInput    │   ───►  │   WASM Paint    │   ───►  │   LayerTree     │
│                 │         │     Module      │         │                 │
│ - Fragments     │         │                 │         │ - Layers[]      │
│ - Styles        │         │ Pure function   │         │ - PropertyTrees │
│ - State         │         │ No side effects │         │ - Draw order    │
└─────────────────┘         └─────────────────┘         └─────────────────┘
```

---

## Input Schema

### PaintInput

```
PaintInput {
    fragments: Fragment[]
    root_fragment_id: u32
    viewport: Rect
    device_pixel_ratio: f32

    // Optional document state
    selection: Selection?
    focus_id: u32?
    caret: CaretState?
}
```

### Fragment

Each fragment represents a box from the layout tree with all paint-relevant properties flattened.

```
Fragment {
    // Identity
    id: u32
    parent_id: u32?
    debug_name: string?

    // Geometry (from layout)
    bounds: Rect                    // content box
    border_box: Rect                // including borders
    padding_box: Rect               // including padding

    // Children
    children: u32[]                 // fragment IDs in DOM order
    stacking_children: u32[]?       // if different from DOM order (z-index)

    // === Paint-relevant style properties ===

    // Background
    background_color: Color
    background_images: BackgroundImage[]

    // Border
    border_widths: [f32; 4]         // top, right, bottom, left
    border_colors: [Color; 4]
    border_styles: [BorderStyle; 4]
    border_radii: [f32; 8]          // 4 corners × 2 (horizontal, vertical)

    // Box decorations
    box_shadows: BoxShadow[]
    outline_width: f32
    outline_color: Color
    outline_style: BorderStyle
    outline_offset: f32

    // Transform
    transform: Matrix4x4?
    transform_origin: Point3D

    // Clip
    overflow_x: Overflow            // visible, hidden, scroll, auto
    overflow_y: Overflow
    clip_path: ClipPath?

    // Effects
    opacity: f32
    blend_mode: BlendMode
    filter: Filter[]
    backdrop_filter: Filter[]
    mask: Mask?

    // Visibility
    visibility: Visibility          // visible, hidden, collapse

    // Compositing hints
    will_change: WillChange[]
    contain: Contain

    // Text (for text fragments)
    text_content: TextRun[]?

    // Image (for replaced elements)
    image_resource_id: u32?
}
```

### Supporting Types

```
Rect {
    x: f32
    y: f32
    width: f32
    height: f32
}

Color {
    r: u8
    g: u8
    b: u8
    a: u8
}

Matrix4x4 {
    m: [f32; 16]                    // column-major
}

BackgroundImage {
    type: "none" | "color" | "linear_gradient" | "radial_gradient" | "image"
    // Gradient-specific
    stops: GradientStop[]?
    angle: f32?
    // Image-specific
    resource_id: u32?
    size: BackgroundSize
    position: Point
    repeat: BackgroundRepeat
}

BoxShadow {
    offset_x: f32
    offset_y: f32
    blur_radius: f32
    spread_radius: f32
    color: Color
    inset: bool
}

Filter {
    type: "blur" | "brightness" | "contrast" | "grayscale" | "saturate" | "sepia" | "drop_shadow" | "opacity"
    value: f32
    // For drop-shadow
    shadow: BoxShadow?
}

TextRun {
    text: string
    font_family: string
    font_size: f32
    font_weight: u16
    font_style: "normal" | "italic" | "oblique"
    color: Color
    decoration: TextDecoration
    offset: Point                   // relative to fragment
    // Pre-shaped glyph positions (optional, for exact rendering)
    glyphs: Glyph[]?
}

ClipPath {
    type: "none" | "inset" | "circle" | "ellipse" | "polygon" | "path"
    // Type-specific data
    inset: Rect?
    radius: f32?
    points: Point[]?
    path: string?                   // SVG path data
}

Overflow = "visible" | "hidden" | "scroll" | "auto" | "clip"
BlendMode = "normal" | "multiply" | "screen" | "overlay" | ...
BorderStyle = "none" | "solid" | "dashed" | "dotted" | "double" | ...
Visibility = "visible" | "hidden" | "collapse"
WillChange = "transform" | "opacity" | "scroll_position" | "contents"
Contain = "none" | "layout" | "paint" | "size" | "strict" | "content"
```

---

## Output Schema

### LayerTree

```
LayerTree {
    layers: Layer[]
    property_trees: PropertyTrees

    // Root layer ID
    root_layer_id: u32

    // Draw order (back to front)
    draw_order: u32[]
}
```

### Layer

```
Layer {
    id: u32
    parent_id: u32?

    // Bounds in layer space
    bounds: Rect

    // Property tree indices
    transform_node_id: u32
    clip_node_id: u32
    effect_node_id: u32
    scroll_node_id: u32

    // Content
    content: LayerContent

    // Layer flags
    is_scrollable: bool
    hit_testable: bool
    draws_content: bool

    // Debug
    debug_name: string?
    compositing_reasons: string[]
}

LayerContent =
    | { type: "picture", display_list: DisplayItem[] }
    | { type: "solid_color", color: Color }
    | { type: "surface", surface_id: u32 }
    | { type: "none" }
```

### PropertyTrees

```
PropertyTrees {
    transform_tree: TransformNode[]
    clip_tree: ClipNode[]
    effect_tree: EffectNode[]
    scroll_tree: ScrollNode[]
}

TransformNode {
    id: u32
    parent_id: u32

    local: Matrix4x4               // local transform
    origin: Point3D                // transform origin

    // Cached
    to_screen: Matrix4x4?          // accumulated to root

    // Flags
    flattens_inherited: bool
    is_animated: bool
}

ClipNode {
    id: u32
    parent_id: u32
    transform_node_id: u32         // space of clip rect

    clip_type: "none" | "rect" | "rounded_rect" | "path"
    clip_rect: Rect
    rounded_corners: [f32; 8]?     // for rounded_rect
    clip_path: string?             // SVG path for path type
}

EffectNode {
    id: u32
    parent_id: u32
    transform_node_id: u32
    clip_node_id: u32

    opacity: f32
    blend_mode: BlendMode
    filters: Filter[]
    backdrop_filters: Filter[]
    mask_layer_id: u32?

    // Whether this node creates a render surface
    has_render_surface: bool
    render_surface_reason: RenderSurfaceReason?
}

ScrollNode {
    id: u32
    parent_id: u32
    transform_node_id: u32         // transform affected by scroll

    scroll_offset: Point           // current scroll position
    scroll_bounds: Size            // scrollable area size
    container_bounds: Size         // visible area size

    user_scrollable_horizontal: bool
    user_scrollable_vertical: bool
}

RenderSurfaceReason =
    | "root"
    | "opacity"
    | "filter"
    | "backdrop_filter"
    | "blend_mode"
    | "mask"
    | "clip_path"
    | "3d_transform"
```

### DisplayItem

Abstract drawing commands (not Skia-specific, can be executed on any canvas).

```
DisplayItem =
    | DrawRect { rect: Rect, color: Color }
    | DrawRRect { rect: Rect, radii: [f32; 8], color: Color }
    | DrawBorder { rect: Rect, widths: [f32; 4], colors: [Color; 4], styles: [BorderStyle; 4], radii: [f32; 8] }
    | DrawBoxShadow { rect: Rect, shadow: BoxShadow, radii: [f32; 8] }
    | DrawImage { rect: Rect, resource_id: u32, src_rect: Rect?, sampling: Sampling }
    | DrawText { runs: TextRun[], bounds: Rect }
    | DrawLinearGradient { rect: Rect, angle: f32, stops: GradientStop[] }
    | DrawRadialGradient { rect: Rect, center: Point, radius: f32, stops: GradientStop[] }
    | DrawPath { path: string, fill_color: Color?, stroke_color: Color?, stroke_width: f32? }
    | PushClipRect { rect: Rect }
    | PushClipRRect { rect: Rect, radii: [f32; 8] }
    | PushClipPath { path: string }
    | PopClip {}
    | PushOpacity { opacity: f32 }
    | PopOpacity {}
    | PushTransform { matrix: Matrix4x4 }
    | PopTransform {}
    | PushBlendMode { mode: BlendMode }
    | PopBlendMode {}

Sampling = "nearest" | "linear" | "cubic"
GradientStop { offset: f32, color: Color }
```

---

## WASM Interface

### C/Rust FFI

```rust
// Rust interface
#[wasm_bindgen]
pub fn paint(input_ptr: *const u8, input_len: usize) -> *mut PaintResult;

#[wasm_bindgen]
pub fn free_result(result: *mut PaintResult);

// Or with typed arrays
#[wasm_bindgen]
pub fn paint(input: &[u8]) -> Vec<u8>;
```

### JavaScript Usage

```javascript
// Load WASM module
const paintModule = await WebAssembly.instantiateStreaming(
    fetch('paint.wasm'),
    imports
);

// Serialize input (using flatbuffers, protobuf, or custom binary)
const input = serializePaintInput({
    fragments: extractFragmentsFromDOM(document.body),
    root_fragment_id: 0,
    viewport: { x: 0, y: 0, width: window.innerWidth, height: window.innerHeight },
    device_pixel_ratio: window.devicePixelRatio,
});

// Call paint
const outputBuffer = paintModule.exports.paint(input);

// Deserialize output
const layerTree = deserializeLayerTree(outputBuffer);

// Use layer tree
renderLayerTree(canvas, layerTree);
```

---

## Serialization Format

Recommended: **FlatBuffers**

- Zero-copy access
- Good WASM support
- Schema evolution
- Efficient for nested structures

Alternative: **Custom binary format**

```
// Simple binary layout for Fragment
[u32 id]
[u32 parent_id]
[f32 x][f32 y][f32 width][f32 height]  // bounds
[u8 r][u8 g][u8 b][u8 a]                // background_color
[f32 × 4]                               // border_widths
[Color × 4]                             // border_colors
[f32 × 8]                               // border_radii
[f32 opacity]
[u8 blend_mode]
[u16 children_count][u32 × N]           // child IDs
...
```

---

## Internal Pipeline (inside WASM)

```
paint(input: PaintInput) -> LayerTree:

    1. Build Fragment Index
       fragments_by_id: Map<u32, &Fragment>

    2. Pre-Paint: Build Property Trees
       for fragment in walk_tree(input.root):
           create_transform_node(fragment)
           create_clip_node(fragment)
           create_effect_node(fragment)
           create_scroll_node(fragment)

    3. Determine Compositing
       for fragment in fragments:
           if needs_composited_layer(fragment):
               assign_layer(fragment)
           else:
               squash_into_parent_layer(fragment)

    4. Paint Each Layer
       for layer in layers:
           display_list = []
           for fragment in layer.fragments:
               paint_fragment(fragment, display_list)
           layer.content = Picture(display_list)

    5. Compute Draw Order
       draw_order = topological_sort(layers, by=effect_tree_order)

    6. Return LayerTree
       return LayerTree { layers, property_trees, draw_order }
```

---

## Compositing Decisions

A fragment gets its own layer when any of these apply:

```rust
fn needs_composited_layer(fragment: &Fragment) -> bool {
    // 3D transform
    fragment.transform.map(|t| !t.is_2d()).unwrap_or(false) ||

    // will-change hints
    fragment.will_change.contains(&WillChange::Transform) ||
    fragment.will_change.contains(&WillChange::Opacity) ||

    // Backdrop filter (needs surface)
    !fragment.backdrop_filter.is_empty() ||

    // Video/canvas (external surface)
    fragment.is_surface_layer ||

    // Scrollable with overflow
    (fragment.overflow_x == Overflow::Scroll ||
     fragment.overflow_y == Overflow::Scroll) ||

    // Fixed position
    fragment.is_fixed_position ||

    // Clip path (may need surface)
    fragment.clip_path.is_some() && fragment.has_descendants
}
```

---

## Example

### Input

```json
{
    "fragments": [
        {
            "id": 0,
            "parent_id": null,
            "bounds": { "x": 0, "y": 0, "width": 800, "height": 600 },
            "background_color": { "r": 255, "g": 255, "b": 255, "a": 255 },
            "children": [1, 2]
        },
        {
            "id": 1,
            "parent_id": 0,
            "bounds": { "x": 10, "y": 10, "width": 200, "height": 100 },
            "background_color": { "r": 255, "g": 0, "b": 0, "a": 255 },
            "border_widths": [2, 2, 2, 2],
            "border_colors": [
                { "r": 0, "g": 0, "b": 0, "a": 255 },
                { "r": 0, "g": 0, "b": 0, "a": 255 },
                { "r": 0, "g": 0, "b": 0, "a": 255 },
                { "r": 0, "g": 0, "b": 0, "a": 255 }
            ],
            "border_radii": [8, 8, 8, 8, 8, 8, 8, 8],
            "opacity": 0.8,
            "children": []
        },
        {
            "id": 2,
            "parent_id": 0,
            "bounds": { "x": 10, "y": 120, "width": 200, "height": 100 },
            "background_color": { "r": 0, "g": 0, "b": 255, "a": 255 },
            "transform": {
                "m": [1,0,0,0, 0,1,0,0, 0,0,1,0, 50,0,0,1]
            },
            "will_change": ["transform"],
            "children": []
        }
    ],
    "root_fragment_id": 0,
    "viewport": { "x": 0, "y": 0, "width": 800, "height": 600 },
    "device_pixel_ratio": 2.0
}
```

### Output

```json
{
    "layers": [
        {
            "id": 0,
            "bounds": { "x": 0, "y": 0, "width": 800, "height": 600 },
            "transform_node_id": 0,
            "clip_node_id": 0,
            "effect_node_id": 0,
            "scroll_node_id": 0,
            "content": {
                "type": "picture",
                "display_list": [
                    { "type": "DrawRect", "rect": { "x": 0, "y": 0, "width": 800, "height": 600 }, "color": { "r": 255, "g": 255, "b": 255, "a": 255 } },
                    { "type": "PushOpacity", "opacity": 0.8 },
                    { "type": "DrawRRect", "rect": { "x": 10, "y": 10, "width": 200, "height": 100 }, "radii": [8,8,8,8,8,8,8,8], "color": { "r": 255, "g": 0, "b": 0, "a": 255 } },
                    { "type": "DrawBorder", "rect": { "x": 10, "y": 10, "width": 200, "height": 100 }, "widths": [2,2,2,2], "colors": [{"r":0,"g":0,"b":0,"a":255},{"r":0,"g":0,"b":0,"a":255},{"r":0,"g":0,"b":0,"a":255},{"r":0,"g":0,"b":0,"a":255}], "radii": [8,8,8,8,8,8,8,8] },
                    { "type": "PopOpacity" }
                ]
            },
            "draws_content": true
        },
        {
            "id": 1,
            "bounds": { "x": 10, "y": 120, "width": 200, "height": 100 },
            "transform_node_id": 1,
            "clip_node_id": 0,
            "effect_node_id": 0,
            "scroll_node_id": 0,
            "content": {
                "type": "picture",
                "display_list": [
                    { "type": "DrawRect", "rect": { "x": 0, "y": 0, "width": 200, "height": 100 }, "color": { "r": 0, "g": 0, "b": 255, "a": 255 } }
                ]
            },
            "draws_content": true,
            "compositing_reasons": ["will-change: transform"]
        }
    ],
    "property_trees": {
        "transform_tree": [
            { "id": 0, "parent_id": null, "local": { "m": [1,0,0,0,0,1,0,0,0,0,1,0,0,0,0,1] } },
            { "id": 1, "parent_id": 0, "local": { "m": [1,0,0,0,0,1,0,0,0,0,1,0,60,120,0,1] } }
        ],
        "clip_tree": [
            { "id": 0, "parent_id": null, "clip_type": "rect", "clip_rect": { "x": 0, "y": 0, "width": 800, "height": 600 } }
        ],
        "effect_tree": [
            { "id": 0, "parent_id": null, "opacity": 1.0, "blend_mode": "normal", "has_render_surface": true, "render_surface_reason": "root" }
        ],
        "scroll_tree": [
            { "id": 0, "parent_id": null, "scroll_offset": { "x": 0, "y": 0 }, "container_bounds": { "width": 800, "height": 600 } }
        ]
    },
    "root_layer_id": 0,
    "draw_order": [0, 1]
}
```

---

## Implementation Language

**Recommended: Rust**

```
paint-wasm/
├── Cargo.toml
├── src/
│   ├── lib.rs              # WASM entry point
│   ├── input.rs            # Input types + deserialization
│   ├── output.rs           # Output types + serialization
│   ├── property_trees.rs   # Tree building
│   ├── compositor.rs       # Layer assignment
│   ├── painter.rs          # Display list generation
│   └── display_items.rs    # Drawing commands
├── schemas/
│   └── paint.fbs           # FlatBuffers schema
└── tests/
    └── test_cases/         # JSON test inputs
```

Rust benefits:
- `wasm-pack` for easy WASM builds
- No GC (predictable performance)
- Strong typing matches the schema
- Good serialization libraries (serde, flatbuffers-rs)
