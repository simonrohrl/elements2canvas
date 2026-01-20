# Paint System Implementation Plan

## Overview

Build a paint system that replicates Chromium's rendering pipeline to produce `raw_paint_ops.json` from input data.

```
                         INPUTS (from Chromium logging)
┌─────────────────────────────────────────────────────────────────┐
│  layout_tree.json      - LayoutObject tree with geometry        │
│  property_trees.json   - Transform/Clip/Effect trees (NEW)      │
└─────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────┐
│  compute_layer_tree.js      ──►  layer_tree.json                │
│  compute_stacking_nodes.js  ──►  stacking_nodes.json            │
└─────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────┐
│                        paint_system.js                          │
│  - Traverses layer tree in stacking order                       │
│  - Generates paint ops for each element                         │
│  - Attaches property tree IDs to each op                        │
└─────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
                         OUTPUT (to verify against reference)
┌─────────────────────────────────────────────────────────────────┐
│  computed_paint_ops.json   (compare with raw_paint_ops.json)    │
└─────────────────────────────────────────────────────────────────┘
```

---

## Current State

### Completed Components
1. **compute_layer_tree.js** - Transforms LayoutObject tree (807 nodes) to PaintLayer tree (81 nodes)
2. **compute_stacking_nodes.js** - Computes stacking node z-order lists for paint ordering
3. **Chromium logging** - Generates reference files with computed styles

### Input Data Available
- **layout_tree.json** (766KB): 807 LayoutObjects with:
  - `id`, `name`, `z_index`, `is_stacking_context`, `is_stacked`
  - `has_layer`, `is_self_painting`
  - `computed_style`: display, position, opacity, transforms, filters, etc.
  - `children` array, `depth`

### Target Output
- **raw_paint_ops.json** (866KB): Contains:
  - `paint_ops` array with operations like DrawRectOp, DrawRRectOp, DrawTextBlobOp, etc.
  - `transform_tree` - node id → matrix/translation mapping
  - `clip_tree` - node id → clip_rect mapping
  - `effect_tree` - node id → opacity/blend_mode mapping

---

## Required Input Files

The paint system needs these inputs to produce `raw_paint_ops.json`:

### Currently Available
- **layout_tree.json** - LayoutObject tree with computed styles
- **layer_tree.json** - PaintLayer tree (computed)
- **stacking_nodes.json** - Z-order lists for paint ordering (computed)

### Need to Generate as Input
- **property_trees.json** - Transform, Clip, Effect trees (NEW)

The serialization for property trees **already exists** in `paint_artifact_serializer.cc`:
- `SerializeTransformTree()` - lines 640-689
- `SerializeClipTree()` - lines 692-731
- `SerializeEffectTree()` - lines 734-776
- `PropertyTreeIdMapper` class - lines 564-637

We need to call this serialization and output it as a **separate input file**, not embedded in raw_paint_ops.json (which is the output).

---

## Missing Data for Paint Replication

### 1. Property Trees as Input (CRITICAL)
Need `property_trees.json` with:
- **Transform tree**: id → matrix/translation mapping
- **Clip tree**: id → clip_rect mapping
- **Effect tree**: id → opacity/blend_mode mapping

### 2. Geometry Data (CRITICAL)
- **Box geometry**: x, y, width, height for each LayoutObject
- **Border radii**: corner radius values

### 3. Mapping LayoutObjects to Property Tree Nodes
- Which transform_id, clip_id, effect_id applies to each LayoutObject
- Text data already available in DrawTextBlobOp format

---

## Implementation Phases

### Phase 1: Generate property_trees.json Input File

The serialization already exists in `paint_artifact_serializer.cc`. We need to output it as a separate file.

**Option A: Add a separate logging call in paint_layer_painter.cc**

```cpp
// After logging layout_tree.json, also log property trees
void LogPropertyTrees(const LocalFrameView& frame_view) {
  const auto& paint_artifact = frame_view.GetPaintArtifact();
  // Reuse existing SerializeTransformTree, SerializeClipTree, SerializeEffectTree
  auto json = std::make_unique<JSONObject>();
  // ... build property tree JSON
  // Write to property_trees.json
}
```

**Option B: Extract from raw_paint_ops.json post-hoc**

Since raw_paint_ops.json already contains the property trees embedded, we can extract them:
```bash
cat raw_paint_ops.json | jq '{transform_tree, clip_tree, effect_tree}' > property_trees.json
```

### Phase 2: Extend Chromium Logging - Geometry Data

Add geometry data to layout_tree.json.

**File:** `chromium/src/third_party/blink/renderer/core/paint/paint_layer_painter.cc`

```cpp
// Add to SerializeLayoutObject():

// 1. Box geometry (absolute coordinates)
if (object.IsBox()) {
  const auto& box = To<LayoutBox>(object);
  auto geometry = std::make_unique<JSONObject>();

  // Get absolute position
  PhysicalRect rect = box.PhysicalBorderBoxRect();
  // Transform to absolute coordinates
  rect = box.LocalToAbsoluteRect(rect);

  geometry->SetDouble("x", rect.X().ToDouble());
  geometry->SetDouble("y", rect.Y().ToDouble());
  geometry->SetDouble("width", rect.Width().ToDouble());
  geometry->SetDouble("height", rect.Height().ToDouble());
  json->SetObject("geometry", std::move(geometry));
}

// 2. Border radii (already have style.HasBorderRadius() in computed_style)
if (object.StyleRef().HasBorderRadius()) {
  const auto& r = object.StyleRef().BorderRadii();
  auto radii = std::make_unique<JSONArray>();
  // 8 values: [tl_x, tl_y, tr_x, tr_y, br_x, br_y, bl_x, bl_y]
  radii->PushDouble(r.TopLeft().Width().ToDouble());
  radii->PushDouble(r.TopLeft().Height().ToDouble());
  radii->PushDouble(r.TopRight().Width().ToDouble());
  radii->PushDouble(r.TopRight().Height().ToDouble());
  radii->PushDouble(r.BottomRight().Width().ToDouble());
  radii->PushDouble(r.BottomRight().Height().ToDouble());
  radii->PushDouble(r.BottomLeft().Width().ToDouble());
  radii->PushDouble(r.BottomLeft().Height().ToDouble());
  json->SetArray("border_radii", std::move(radii));
}

// 3. Background color (resolved)
const ComputedStyle& style = object.StyleRef();
if (style.HasBackground()) {
  Color bg = style.VisitedDependentColor(GetCSSPropertyBackgroundColor());
  auto bg_color = std::make_unique<JSONObject>();
  bg_color->SetDouble("r", bg.Red() / 255.0);
  bg_color->SetDouble("g", bg.Green() / 255.0);
  bg_color->SetDouble("b", bg.Blue() / 255.0);
  bg_color->SetDouble("a", bg.Alpha() / 255.0);
  json->SetObject("background_color", std::move(bg_color));
}
```

### Phase 2: Link Paint Ops to LayoutObjects

Currently raw_paint_ops.json has all paint operations but limited linkage to LayoutObjects.
We need to track which LayoutObject produced which paint operations.

**Option A: Add layout_object_id to paint ops during serialization**

In `paint_artifact_serializer.cc`, when serializing display items:
```cpp
// DisplayItem has a client ID that links to the LayoutObject
op_json->SetInteger("layout_object_id", item.ClientId());
```

**Option B: Add paint_op_refs to layout_tree.json**

During paint, track which paint ops each LayoutObject generates:
```json
{
  "id": 42,
  "name": "LayoutBlockFlow DIV",
  "paint_op_indices": [15, 16, 17]
}
```

**Text Data**: DrawTextBlobOp already contains complete text data:
- `runs[].glyphs` - glyph IDs
- `runs[].positions` - x positions (horizontal positioning)
- `runs[].font` - family, size, weight, metrics
- `bounds` - text bounding box
- `nodeId` - links to DOM node (partial LayoutObject mapping)

### Phase 3: Paint Traversal Algorithm

Implement CSS 2.1 Appendix E paint order in JavaScript:

**File:** `paint_system.js`

```javascript
/**
 * Main paint traversal following stacking order
 */
function paintLayer(layer, ops) {
  // 1. If layer is not visible, skip
  if (!isVisible(layer)) return;

  // 2. Paint background and borders of the stacking context
  if (layer.is_stacking_context) {
    paintBackgroundAndBorder(layer, ops);
  }

  // 3. Paint negative z-index children (from stacking_nodes.json)
  const stackingNode = findStackingNode(layer.id);
  if (stackingNode) {
    for (const child of stackingNode.neg_z_order_list) {
      paintLayer(getLayer(child.id), ops);
    }
  }

  // 4. Paint self (block background, floats, inline content)
  paintSelf(layer, ops);

  // 5. Paint positive z-index children (includes z-index: 0)
  if (stackingNode) {
    for (const child of stackingNode.pos_z_order_list) {
      paintLayer(getLayer(child.id), ops);
    }
  }
}
```

### Phase 4: Paint Operation Generators

Implement individual paint operations:

```javascript
/**
 * Paint operations for a single element
 */
function paintElement(layoutObject, ops) {
  const style = layoutObject.computed_style;
  const geometry = layoutObject.geometry;

  // Background
  if (hasBackground(style)) {
    ops.push(createDrawRectOp(geometry, style));
  }

  // Border
  if (hasBorder(style)) {
    ops.push(createDrawBorderOp(geometry, style));
  }

  // Text
  if (layoutObject.name.includes('Text')) {
    ops.push(createDrawTextBlobOp(layoutObject));
  }
}

function createDrawRectOp(geometry, style) {
  return {
    type: 'DrawRectOp',
    rect: [geometry.x, geometry.y,
           geometry.x + geometry.width,
           geometry.y + geometry.height],
    flags: {
      r: style.background_color.r,
      g: style.background_color.g,
      b: style.background_color.b,
      a: style.background_color.a,
      style: 0,  // Fill
      strokeWidth: 0
    },
    transform_id: getTransformId(layoutObject),
    clip_id: getClipId(layoutObject),
    effect_id: getEffectId(layoutObject)
  };
}
```

### Phase 5: Graphics State Management

Track current transform, clip, and effect state:

```javascript
class GraphicsState {
  constructor(propertyTrees) {
    this.transforms = propertyTrees.transforms;
    this.clips = propertyTrees.clips;
    this.effects = propertyTrees.effects;
  }

  getTransformId(layoutObject) {
    // Find the transform node for this element
    // Walk up tree to find nearest transform
  }

  getClipId(layoutObject) {
    // Find clip region (overflow, clip-path)
  }

  getEffectId(layoutObject) {
    // Find effect (opacity, filter, blend-mode)
  }
}
```

### Phase 6: Special Cases

Handle complex paint scenarios:

1. **Rounded rectangles (DrawRRectOp)**
   - Extract border-radius values
   - Generate 8 radii values [tl, tr, br, bl] x 2

2. **Shadows (DrawLooper)**
   - box-shadow → shadows array in flags
   - text-shadow → similar treatment

3. **Gradients (PaintShader)**
   - linear-gradient, radial-gradient
   - Extract color stops and positions

4. **Clipping (ClipRRectOp, ClipPathOp)**
   - overflow: hidden → ClipRectOp
   - border-radius + overflow → ClipRRectOp
   - clip-path → ClipPathOp

5. **Text painting (DrawTextBlobOp)**
   - Requires full shape results
   - Most complex operation to replicate

---

## File Structure

```
chromium-dom-tracing/
├── layout_tree.json          # Input: LayoutObject tree with geometry
├── property_trees.json       # Input: Transform/Clip/Effect trees (NEW)
├── layer_tree.json           # Computed: PaintLayer tree
├── stacking_nodes.json       # Computed: Z-order lists
├── compute_layer_tree.js     # Existing
├── compute_stacking_nodes.js # Existing
├── paint_system/             # NEW
│   ├── index.js              # Main entry point
│   ├── traversal.js          # Paint order traversal
│   ├── operations.js         # Paint op generators
│   ├── graphics_state.js     # State management
│   ├── text_painter.js       # Text blob generation
│   └── utils.js              # Helpers
└── raw_paint_ops.json        # Output: Paint operations
```

---

## Chromium Logging Extensions Required

### 1. Geometry in LayoutObject Serialization

```cpp
// Add to SerializeLayoutObject() in paint_layer_painter.cc

// Box geometry
if (object.IsBox()) {
  const auto& box = To<LayoutBox>(object);
  auto geom = std::make_unique<JSONObject>();

  PhysicalRect rect = box.PhysicalBorderBoxRect();
  geom->SetDouble("x", rect.X().ToDouble());
  geom->SetDouble("y", rect.Y().ToDouble());
  geom->SetDouble("width", rect.Width().ToDouble());
  geom->SetDouble("height", rect.Height().ToDouble());

  // Also include padding box, content box if needed
  json->SetObject("geometry", std::move(geom));
}

// Border radii
if (object.StyleRef().HasBorderRadius()) {
  auto radii = std::make_unique<JSONArray>();
  const auto& r = object.StyleRef().BorderRadii();
  // Push all 8 values (4 corners x 2 dimensions)
  radii->PushDouble(r.TopLeft().Width().ToDouble());
  radii->PushDouble(r.TopLeft().Height().ToDouble());
  // ... etc
  json->SetArray("border_radii", std::move(radii));
}
```

### 2. Text Shape Results

```cpp
// For LayoutText objects, serialize shape results

void SerializeTextShapeResult(const ShapeResult& result, JSONObject* json) {
  auto runs = std::make_unique<JSONArray>();

  for (const auto& run : result.RunsOrParts()) {
    auto run_json = std::make_unique<JSONObject>();

    // Glyph data
    auto glyphs = std::make_unique<JSONArray>();
    auto positions = std::make_unique<JSONArray>();

    for (const auto& glyph : run.Glyphs()) {
      glyphs->PushInteger(glyph.glyph_id);
      positions->PushDouble(glyph.advance);
    }

    run_json->SetArray("glyphs", std::move(glyphs));
    run_json->SetArray("positions", std::move(positions));

    // Font info
    auto font = std::make_unique<JSONObject>();
    font->SetString("family", run.Font().FamilyName());
    font->SetDouble("size", run.Font().Size());
    // ... etc
    run_json->SetObject("font", std::move(font));

    runs->PushObject(std::move(run_json));
  }

  json->SetArray("shape_runs", std::move(runs));
}
```

### 3. Property Trees

```cpp
// New function to serialize property trees

void SerializePropertyTrees(const LocalFrameView& frame_view, JSONObject* root) {
  const auto& paint_artifact = frame_view.GetPaintArtifact();

  // Transform tree
  auto transforms = std::make_unique<JSONArray>();
  // Walk transform tree and serialize each node with ID and matrix

  // Clip tree
  auto clips = std::make_unique<JSONArray>();
  // Walk clip tree and serialize each node with ID and clip rect

  // Effect tree
  auto effects = std::make_unique<JSONArray>();
  // Walk effect tree and serialize each node with ID, opacity, filter, blend

  root->SetArray("transforms", std::move(transforms));
  root->SetArray("clips", std::move(clips));
  root->SetArray("effects", std::move(effects));
}
```

---

## Verification Strategy

### Unit Tests
1. Test paint traversal order matches Chromium
2. Test individual paint op generation
3. Test graphics state tracking

### Integration Tests
1. Compare computed `raw_paint_ops.json` with reference
2. Visual diff: render both outputs and compare screenshots

### Test Cases
1. Simple box with background
2. Box with border-radius
3. Box with box-shadow
4. Stacked elements with z-index
5. Text with styling
6. Nested transforms
7. Overflow clipping
8. Opacity and blend modes

---

## Implementation Order

1. **Week 1: Chromium Logging**
   - Add geometry to layout_tree.json
   - Add property tree serialization
   - Regenerate reference files

2. **Week 2: Core Paint System**
   - Implement paint traversal
   - Basic operations (DrawRectOp, DrawRRectOp)
   - Graphics state management

3. **Week 3: Advanced Features**
   - Shadows, gradients
   - Clipping operations
   - Save/Restore handling

4. **Week 4: Text Painting**
   - Shape result extraction
   - DrawTextBlobOp generation
   - Font metrics handling

5. **Week 5: Verification & Polish**
   - Diff testing against reference
   - Edge case handling
   - Documentation

---

## Open Questions

1. **Text shaping**: Extract from Chromium vs re-shape in JS?
   - Chromium extraction is more accurate but more work
   - JS reshaping is simpler but may have differences

2. **Property tree IDs**: How are transform_id/clip_id/effect_id assigned?
   - Need to understand Chromium's ID assignment algorithm
   - May need to implement same algorithm in JS

3. **Image painting**: How to handle images?
   - DrawImageOp requires image data
   - May skip initially or use placeholders

4. **Scope**: Which paint operations to support initially?
   - Start with rect, rrect, text
   - Add others as needed based on test coverage
