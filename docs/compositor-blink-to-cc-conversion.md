# Compositor: Blink to CC Conversion Logic

This document details the exact transformation from `blink_layers.json` to `layers.json`, with the goal of extracting the actual Chromium code and compiling it to WASM for a standalone converter.

## Project Scope

**Goal:** Extract Chromium's compositor conversion logic and compile to WebAssembly.

**Simplification:** Scroll layers are NOT required for our use case. This eliminates:
- `ScrollPaintPropertyNode` handling
- `cc::ScrollTree` construction
- Scroll hit-test layer creation
- Composited scrolling decisions

This significantly reduces complexity.

## Conversion Overview

```
blink_layers.json                    layers.json
┌─────────────────────┐              ┌─────────────────────┐
│ BlinkLayers[]       │              │ LayerTreeImpl[]     │
│  ├─ layer_id        │  ────────►   │  ├─ id              │
│  ├─ debug_name      │              │  ├─ name            │
│  ├─ bounds          │              │  ├─ bounds          │
│  ├─ property_tree_  │              │  ├─ transform_tree_ │
│  │  state           │              │  │  index           │
│  └─ paint_chunks[]  │              │  └─ paint_ops[]     │
└─────────────────────┘              └─────────────────────┘

blink_property_trees.json            property_trees.json
┌─────────────────────┐              ┌─────────────────────┐
│ transform_tree      │              │ transform_tree      │
│ clip_tree           │  ────────►   │ clip_tree           │
│ effect_tree         │              │ effect_tree         │
│                     │              │ scroll_tree (added) │
└─────────────────────┘              └─────────────────────┘
```

## The Transformation Pipeline

### Stage 1: Layer Merging/Splitting

Blink's `PendingLayer` objects may be transformed into CC layers:

```
Blink PendingLayers          CC Layers
┌────────────────────┐
│ Layer A            │       ┌────────────────────┐
│ (chunks 1,2,3)     │ ──►   │ cc::Layer X        │  (merged A+B)
├────────────────────┤       └────────────────────┘
│ Layer B            │
│ (chunks 4,5)       │
└────────────────────┘

OR

┌────────────────────┐       ┌────────────────────┐
│ Layer A            │       │ cc::Layer X        │  (part of A)
│ (scrolling content)│ ──►   ├────────────────────┤
│                    │       │ cc::Layer Y        │  (scroll container)
└────────────────────┘       └────────────────────┘
```

**Key Decision Logic** (from `PaintArtifactCompositor`):

| Condition | Result |
|-----------|--------|
| Two layers have compatible `PropertyTreeState` | May merge |
| Layer needs scroll hit testing | Creates `kScrollHitTestLayer` |
| Layer is externally managed (video, canvas) | Creates `kForeignLayer` |
| Layer is a scrollbar | Creates `kScrollbarLayer` |
| Layer can be solid color optimized | Creates `cc::SolidColorLayer` |
| Default case | Creates `cc::PictureLayer` |

### Stage 2: Property Tree Node Mapping

Blink property tree nodes get assigned CC node IDs:

```
Blink Transform Nodes        CC Transform Nodes
┌─────────────────────┐      ┌─────────────────────┐
│ Node (ptr=0xABC)    │      │ Node (id=1)         │
│   parent: 0xDEF     │ ──►  │   parent_id: 0      │
│   matrix: [...]     │      │   local: [...]      │
└─────────────────────┘      └─────────────────────┘

Mapping stored as: blink_ptr → cc_id
```

**ID Assignment Algorithm**:

```
function EnsureCompositorTransformNode(blink_node):
    if blink_node.cc_id exists:
        return blink_node.cc_id

    parent_cc_id = EnsureCompositorTransformNode(blink_node.parent)

    cc_node = create_cc_transform_node()
    cc_node.parent_id = parent_cc_id
    cc_node.local = blink_node.matrix
    cc_node.flattens_inherited = blink_node.flattens_inherited

    cc_id = transform_tree.insert(cc_node)
    blink_node.cc_id = cc_id

    return cc_id
```

### Stage 3: Transform Decomposition

Simple 2D translations can be "decomposed" from the property tree into layer offsets:

```
Before Decomposition:
┌─────────────────────┐
│ Transform Node      │
│   translate(100,50) │
│         │           │
│    ┌────▼────┐      │
│    │ Layer   │      │
│    └─────────┘      │
└─────────────────────┘

After Decomposition:
┌─────────────────────┐
│ Transform Node      │
│   (identity)        │
│         │           │
│    ┌────▼────┐      │
│    │ Layer   │      │
│    │ offset: │      │
│    │ (100,50)│      │
│    └─────────┘      │
└─────────────────────┘
```

**Decomposition Criteria**:
1. Transform must be 2D translation only (no rotation, scale, 3D)
2. No ScrollNode attached
3. No direct compositing reasons
4. Same flattening behavior as parent

### Stage 4: Paint Chunk Flattening

Blink's nested paint chunks become a flat `cc::DisplayItemList`:

```
Blink paint_chunks:              CC paint_ops:
┌────────────────────┐           ┌────────────────────┐
│ chunk[0]           │           │ SaveOp             │
│   paint_ops: [     │           │ ClipRectOp(...)    │
│     DrawRect,      │    ──►    │ DrawRectOp         │
│     DrawText       │           │ DrawTextOp         │
│   ]                │           │ RestoreOp          │
├────────────────────┤           │ SaveOp             │
│ chunk[1]           │           │ TranslateOp(...)   │
│   paint_ops: [     │           │ DrawRRectOp        │
│     DrawRRect      │           │ RestoreOp          │
│   ]                │           └────────────────────┘
└────────────────────┘
```

**State Machine for Chunk Conversion**:

```
For each paint chunk in layer:
    1. Compute delta from current state to chunk's PropertyTreeState
    2. Emit Save/Restore ops as needed for:
       - Transform changes (Concat/SetMatrix)
       - Clip changes (ClipRect/ClipPath)
       - Effect changes (SaveLayerAlpha)
    3. Emit all paint ops from chunk's display items
    4. Update current state
```

### Stage 5: Coordinate Space Adjustment

Blink coordinates are adjusted for the layer's position:

```
Blink coordinates:           CC layer coordinates:
┌─────────────────────────┐  ┌─────────────────────────┐
│ (0,0)                   │  │ Layer origin (0,0)      │
│   ┌───────────────┐     │  │   ┌───────────────┐     │
│   │ DrawRect      │     │  │   │ DrawRect      │     │
│   │ at (100,100)  │ ──► │   │ at (0,0)        │     │
│   └───────────────┘     │  │   └───────────────┘     │
│                         │  │                         │
│ Page coordinates        │  │ Layer-local coords     │
└─────────────────────────┘  └─────────────────────────┘

layer.offset = (100,100)  // Stored separately
```

## Data Structure Mapping

### Layer Fields

| blink_layers.json | layers.json | Transformation |
|-------------------|-------------|----------------|
| `layer_id` | `id` | Reassigned by CC |
| `debug_name` | `name` | Direct copy |
| `bounds[x,y,w,h]` | `bounds{width,height}` | Format change, origin stored in offset |
| `property_tree_state.transform_id` | `transform_tree_index` | Remapped via ID lookup |
| `property_tree_state.clip_id` | `clip_tree_index` | Remapped via ID lookup |
| `property_tree_state.effect_id` | `effect_tree_index` | Remapped via ID lookup |
| `paint_chunks[].paint_ops` | `paint_ops` | Flattened with state machine |

### Property Tree Node Fields

#### Transform Nodes

| Blink | CC | Notes |
|-------|-----|-------|
| `id` (assigned) | `id` (tree index) | New ID in CC |
| `parent_id` | `parent_id` | Remapped |
| `matrix` / `translation2d` | `local` | Same data, different name |
| - | `post_translation` | Added for layer offset |
| - | `sorting_context_id` | From `RenderingContextId()` |
| - | `flattens_inherited_transform` | From Blink node |

#### Clip Nodes

| Blink | CC | Notes |
|-------|-----|-------|
| `id` | `id` | Remapped |
| `parent_id` | `parent_id` | Remapped |
| `clip_rect` | `clip` | Same rect |
| - | `transform_id` | References transform tree |

#### Effect Nodes

| Blink | CC | Notes |
|-------|-----|-------|
| `id` | `id` | Remapped |
| `parent_id` | `parent_id` | Remapped |
| `opacity` | `opacity` | Direct copy |
| `blend_mode` | `blend_mode` | Direct copy |
| - | `has_render_surface` | Computed by CC |
| - | `mask_layer_id` | For synthesized clips |

## Functions Required for WASM Converter

To build a standalone converter from `blink_layers.json` to `layers.json`:

### Core Conversion Functions

```cpp
// 1. Layer conversion
cc::Layer* ConvertPendingLayerToCcLayer(
    const BlinkLayer& blink_layer,
    PropertyTreeIdMapper& id_mapper);

// 2. Property tree building
int MapTransformNode(
    const BlinkTransformNode& blink_node,
    cc::TransformTree& cc_tree,
    std::map<int, int>& id_map);

int MapClipNode(
    const BlinkClipNode& blink_node,
    cc::ClipTree& cc_tree,
    std::map<int, int>& id_map);

int MapEffectNode(
    const BlinkEffectNode& blink_node,
    cc::EffectTree& cc_tree,
    std::map<int, int>& id_map);

// 3. Paint op conversion
cc::DisplayItemList ConvertPaintChunks(
    const std::vector<BlinkPaintChunk>& chunks,
    const PropertyTreeState& layer_state);

// 4. Coordinate adjustment
void AdjustCoordinatesForLayerOffset(
    cc::DisplayItemList& ops,
    const gfx::Vector2dF& offset);
```

### Simplified Conversion Algorithm

```python
def convert_blink_to_cc(blink_layers_json, blink_property_trees_json):
    # Step 1: Build property tree ID mappings
    transform_map = {}  # blink_id -> cc_id
    clip_map = {}
    effect_map = {}

    cc_transform_tree = TransformTree()
    cc_clip_tree = ClipTree()
    cc_effect_tree = EffectTree()
    cc_scroll_tree = ScrollTree()  # May be empty

    for blink_node in blink_property_trees["transform_tree"]["nodes"]:
        cc_id = cc_transform_tree.create_node(
            parent_id=transform_map.get(blink_node["parent_id"], 0),
            local_matrix=blink_node.get("matrix") or identity_with_translation(blink_node.get("translation2d"))
        )
        transform_map[blink_node["id"]] = cc_id

    # Similar for clip and effect trees...

    # Step 2: Convert layers
    cc_layers = []
    for blink_layer in blink_layers["BlinkLayers"]:
        cc_layer = {
            "id": len(cc_layers) + 1,
            "name": blink_layer["debug_name"],
            "bounds": {
                "width": blink_layer["bounds"][2],
                "height": blink_layer["bounds"][3]
            },
            "offset": {
                "x": blink_layer["bounds"][0],
                "y": blink_layer["bounds"][1]
            },
            "transform_tree_index": transform_map[blink_layer["property_tree_state"]["transform_id"]],
            "clip_tree_index": clip_map[blink_layer["property_tree_state"]["clip_id"]],
            "effect_tree_index": effect_map[blink_layer["property_tree_state"]["effect_id"]],
            "paint_ops": flatten_paint_chunks(blink_layer["paint_chunks"])
        }
        cc_layers.append(cc_layer)

    return {
        "LayerTreeImpl": cc_layers,
        "property_trees": {
            "transform_tree": cc_transform_tree.to_json(),
            "clip_tree": cc_clip_tree.to_json(),
            "effect_tree": cc_effect_tree.to_json(),
            "scroll_tree": cc_scroll_tree.to_json()
        }
    }

def flatten_paint_chunks(chunks):
    """Flatten nested paint chunks into a single op list with state management."""
    result = []
    current_state = PropertyTreeState.root()

    for chunk in chunks:
        # Emit state transition ops
        result.extend(emit_state_transition(current_state, chunk["property_tree_state"]))

        # Copy paint ops (adjusting coordinates if needed)
        for op in chunk["paint_ops"]:
            result.append(op)

        current_state = chunk["property_tree_state"]

    return result
```

## What CC Adds That Blink Doesn't Have

| Feature | Description | Needed for WASM? |
|---------|-------------|------------------|
| `scroll_tree` | Complete scroll hierarchy | **NO** - not needed |
| `sticky_position_constraint` | Sticky positioning data | No |
| `has_render_surface` | Whether effect creates a render surface | Yes |
| `rounded_corner_bounds` | Optimized rounded corner clipping | Optional |
| `mask_layer_id` | References to mask layers for complex clips | Optional |
| Layer tiling | Subdivision into tiles for GPU upload | No |

## What Gets Lost in Conversion

| Blink Data | Why Lost |
|------------|----------|
| `paint_chunks` structure | Flattened into single op list |
| Chunk-level `bounds` | Merged into layer bounds |
| Display item boundaries | Ops are concatenated |
| Node ID stability | IDs are reassigned by CC |
| Some debug annotations | CC doesn't preserve all metadata |

---

## WASM Compilation: Extracting Chromium Code Directly

The recommended approach is to **extract the actual Chromium code** and compile it to WASM, rather than reimplementing the logic. This ensures correctness and maintains compatibility.

### Core Chromium Files to Extract

#### Primary Files (~4,500 lines total)

| File | Lines | Purpose |
|------|-------|---------|
| `paint_artifact_compositor.cc/h` | 1599 + 363 | Main orchestrator, `Update()` entry point, `Layerizer` class |
| `property_tree_manager.cc/h` | 1436 + 418 | Blink→CC property tree conversion |
| `pending_layer.cc/h` | 853 + 260 | Layer merging, property decomposition |

#### Supporting Files (~2,000 lines)

| File | Lines | Purpose |
|------|-------|---------|
| `paint_chunks_to_cc_layer.cc/h` | 1686 + 81 | Paint chunk → cc DisplayItemList |
| `content_layer_client_impl.cc/h` | 245 + 73 | cc::ContentLayerClient implementation |

### Key Functions to Extract

```cpp
// Main entry point - orchestrates the entire conversion
void PaintArtifactCompositor::Update(
    const PaintArtifact& artifact,
    const ViewportProperties& viewport_properties,
    ...);

// Layerization decisions
void Layerizer::Layerize();
void Layerizer::LayerizeGroup();
void Layerizer::DecompositeEffect();

// Layer merging
bool PendingLayer::Merge(const PendingLayer& guest);
std::optional<PropertyTreeState> PendingLayer::CanUpcastWith(...);

// Property tree construction
int PropertyTreeManager::EnsureCompositorTransformNode(...);
int PropertyTreeManager::EnsureCompositorClipNode(...);
int PropertyTreeManager::EnsureCompositorEffectNode(...);
// NOTE: EnsureCompositorScrollNode() NOT NEEDED - no scrolling
```

### Dependencies to Stub

#### 1. Blink Garbage Collection (CRITICAL)

The code uses Blink's GC extensively:

```cpp
// CURRENT (Blink GC)
class PaintArtifactCompositor : public GarbageCollected<PaintArtifactCompositor> {
    Member<ContentLayerClientImpl> content_layer_client_;
    HeapVector<PendingLayer> pending_layers_;
    HeapHashMap<Member<const TransformPaintPropertyNode>, int> transform_ids_;
};

// STUB FOR WASM (standard C++)
class PaintArtifactCompositor {
    std::unique_ptr<ContentLayerClientImpl> content_layer_client_;
    std::vector<PendingLayer> pending_layers_;
    std::unordered_map<const TransformPaintPropertyNode*, int> transform_ids_;
};
```

**Stub file: `blink_gc_stub.h`**
```cpp
#define GarbageCollected  // no-op
#define Member std::shared_ptr
#define HeapVector std::vector
#define HeapHashMap std::unordered_map
#define Trace(...) // no-op
```

#### 2. Base Library (PARTIAL)

| Dependency | Strategy |
|------------|----------|
| `base/logging.h` (DCHECK, LOG) | Stub to no-op or assert() |
| `base/memory/scoped_refptr.h` | Keep (header-only) |
| `base/containers/` | Keep or use std:: equivalents |
| `base/compiler_specific.h` | Minimal stub |

**Stub file: `base_stub.h`**
```cpp
#define DCHECK(x) assert(x)
#define DCHECK_EQ(a, b) assert((a) == (b))
#define LOG(level) if(false) std::cerr
#define NOTREACHED() assert(false)
```

#### 3. Threading (REMOVE)

All thread-related code can be removed since WASM is single-threaded:
- Remove `base::WeakPtr` usage (use raw pointers)
- Remove thread checks
- Remove task posting

#### 4. Scroll-Related Code (REMOVE)

Since scrolling is not needed, remove:
```cpp
// DELETE these functions entirely
EnsureCompositorScrollNode()
EnsureCompositorScrollAndTransformNode()
NeedsCompositedScrolling()
// DELETE scroll_tree_ member
// DELETE kScrollHitTestLayer handling
```

### Data Structures That Work Directly

These are pure value types with no platform dependencies:

#### From `cc/` (keep as-is)
```cpp
cc::PropertyTrees           // Container for transform/clip/effect trees
cc::TransformNode           // 4x4 matrix + metadata
cc::ClipNode                // Clip rect + transform reference
cc::EffectNode              // Opacity, blend mode, filters
// cc::ScrollNode           // NOT NEEDED
```

#### From `ui/gfx/` (keep as-is)
```cpp
gfx::Transform              // 4x4 matrix (header-only)
gfx::Rect, gfx::RectF       // Rectangles
gfx::Point, gfx::PointF     // 2D points
gfx::Size, gfx::SizeF       // Dimensions
gfx::Vector2d, gfx::Vector2dF  // 2D vectors
```

#### From Blink paint (keep structures, stub GC)
```cpp
TransformPaintPropertyNode  // Transform node (stub GC)
ClipPaintPropertyNode       // Clip node (stub GC)
EffectPaintPropertyNode     // Effect node (stub GC)
// ScrollPaintPropertyNode  // NOT NEEDED
PropertyTreeState           // (transform, clip, effect) triple
PaintChunk                  // Atomic paint unit
```

### Extraction File Structure

```
chromium-compositor-wasm/
├── src/
│   ├── extracted/                    # Direct copies from Chromium
│   │   ├── paint_artifact_compositor.cc
│   │   ├── paint_artifact_compositor.h
│   │   ├── property_tree_manager.cc
│   │   ├── property_tree_manager.h
│   │   ├── pending_layer.cc
│   │   ├── pending_layer.h
│   │   └── ...
│   ├── stubs/                        # Stub implementations
│   │   ├── blink_gc_stub.h
│   │   ├── base_stub.h
│   │   └── platform_stub.h
│   ├── wasm/                         # WASM interface
│   │   ├── compositor_wasm.cc        # Emscripten bindings
│   │   └── json_interface.cc         # JSON I/O
│   └── cc/                           # CC data structures (copied)
│       ├── property_trees.h
│       ├── transform_node.h
│       └── ...
├── include/
│   └── gfx/                          # Geometry types (copied)
├── CMakeLists.txt
└── build_wasm.sh
```

### Build Configuration (Emscripten)

```cmake
cmake_minimum_required(VERSION 3.15)
project(chromium_compositor_wasm)

set(CMAKE_CXX_STANDARD 17)

# Emscripten settings
if(EMSCRIPTEN)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s WASM=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s EXPORTED_FUNCTIONS='[_convert_blink_to_cc]'")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s EXPORTED_RUNTIME_METHODS='[ccall,cwrap]'")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -s ALLOW_MEMORY_GROWTH=1")
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3")
endif()

add_executable(compositor
    src/extracted/paint_artifact_compositor.cc
    src/extracted/property_tree_manager.cc
    src/extracted/pending_layer.cc
    src/wasm/compositor_wasm.cc
    src/wasm/json_interface.cc
)

target_include_directories(compositor PRIVATE
    src/stubs
    src/extracted
    include
)
```

### WASM Interface

```cpp
// compositor_wasm.cc
#include <emscripten/emscripten.h>
#include "paint_artifact_compositor.h"

extern "C" {

EMSCRIPTEN_KEEPALIVE
char* convert_blink_to_cc(const char* blink_layers_json,
                           const char* blink_property_trees_json) {
    // 1. Parse JSON inputs
    auto blink_layers = ParseBlinkLayers(blink_layers_json);
    auto blink_props = ParseBlinkPropertyTrees(blink_property_trees_json);

    // 2. Run conversion (using extracted Chromium code)
    PaintArtifactCompositor compositor;
    compositor.Update(blink_layers, blink_props);

    // 3. Extract results
    auto cc_layers = compositor.GetCcLayers();
    auto cc_props = compositor.GetPropertyTrees();

    // 4. Serialize to JSON
    return SerializeToJson(cc_layers, cc_props);
}

}
```

### Estimated Effort

| Phase | Effort | Description |
|-------|--------|-------------|
| **Phase 1: Stub Layer** | 1 week | Create GC stubs, base stubs, remove scroll code |
| **Phase 2: Core Extraction** | 2 weeks | Extract and adapt compositor files |
| **Phase 3: WASM Build** | 1 week | Emscripten setup, JSON interface |
| **Phase 4: Testing** | 1 week | Verify output matches Chromium |
| **Total** | ~5 weeks | |

### Code Size Estimates

| Component | Lines |
|-----------|-------|
| Extracted Chromium code | ~6,500 |
| Stubs | ~500 |
| WASM interface | ~300 |
| JSON I/O | ~400 |
| **Total C++ code** | ~7,700 |
| **WASM binary size** | ~600KB - 1.2MB |

### Advantages of Direct Extraction

1. **Correctness** - Uses actual Chromium logic, not a reimplementation
2. **Maintainability** - Can sync with upstream Chromium changes
3. **Completeness** - Handles edge cases that a reimplementation might miss
4. **Testability** - Can compare output directly with Chromium's output

### Simplifications Due to No Scrolling

By removing scroll support, we eliminate:
- `cc::ScrollTree` (~300 lines)
- `ScrollPaintPropertyNode` handling (~200 lines)
- `EnsureCompositorScrollNode()` (~150 lines)
- Scroll hit-test layer creation (~100 lines)
- Composited scrolling decisions (~200 lines)

**Total reduction: ~950 lines** (about 12% of the codebase)
