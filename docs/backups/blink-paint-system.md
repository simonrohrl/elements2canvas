# Blink Paint System: Inputs and Outputs

## Overview

The paint system in `blink/renderer/core/paint` is **not** a simple function that takes the fragment tree and outputs the layer tree. It's a multi-phase process that produces intermediate representations.

```
Fragment Tree (from LayoutNG)
         ↓
    Pre-Paint Phase
         ↓
    PropertyTrees (transform, clip, effect, scroll)
         +
    PaintChunks (groups of paint items sharing properties)
         ↓
    Paint Phase
         ↓
    DisplayItemList (recorded Skia ops)
         ↓
    PaintArtifact
         ↓
    PaintArtifactCompositor
         ↓
    cc::LayerTree
```

---

## INPUTS

### 1. Fragment Tree (Primary Input)

The fragment tree is the output of LayoutNG and contains the geometry of all elements.

```
PhysicalBoxFragment
├── Size, offset (geometry from layout)
├── BorderEdges
├── Children (nested fragments)
├── LayoutObject* (back-pointer)
└── FragmentItems (for inline content)
    ├── TextItem (text runs with shaping info)
    ├── BoxItem (inline boxes)
    └── LineItem (line boxes)
```

### 2. ComputedStyle (per element)

The resolved CSS properties for each element.

```
ComputedStyle
├── BackgroundColor, BackgroundImage
├── BorderColor, BorderWidth, BorderRadius
├── BoxShadow, TextShadow
├── Opacity, BlendMode
├── Transform, TransformOrigin
├── Filter, BackdropFilter
├── ClipPath, Mask
├── Overflow (clip behavior)
├── ZIndex (stacking)
├── Visibility, Display
├── Color, Font, TextDecoration
└── ... (~500 more properties)
```

### 3. Document/Frame State

Runtime state that affects painting.

```
- Selection (start, end, affinity)
- Focus state (which element is focused)
- Caret position and blink state
- Find-in-page highlights
- Drag image state
- Autofill highlight state
- Scroll positions
- DevTools highlights/overlays
```

### 4. Paint Invalidation Info

Information about what needs repainting.

```
PaintInvalidator output:
├── PaintInvalidationReason (full, geometry, style, scroll, etc.)
├── Dirty rects
└── Subtree flags (needs full repaint, etc.)
```

### 5. Compositing Inputs

Decisions about which elements get their own layers.

```
CompositingRequirementsUpdater output:
├── CompositingReasons per element
│   (has3DTransform, hasWillChange, hasBackdropFilter, etc.)
├── Squashing decisions
└── Layer assignments
```

---

## OUTPUTS

### 1. PropertyTrees (from PrePaint)

Four trees that store transform, clip, effect, and scroll state. Layers reference nodes in these trees by index rather than storing the data directly.

```cpp
PropertyTrees
├── TransformTree
│   └── TransformNode[]
│       ├── gfx::Transform local
│       ├── gfx::Transform to_screen (cached)
│       ├── parent_id
│       ├── scroll_id (if scroll-induced)
│       └── flattens_inherited_transform
│
├── ClipTree
│   └── ClipNode[]
│       ├── gfx::RectF clip_rect
│       ├── transform_id (space of clip)
│       ├── parent_id
│       └── ClipType (normal, rounded, path)
│
├── EffectTree
│   └── EffectNode[]
│       ├── float opacity
│       ├── SkBlendMode blend_mode
│       ├── FilterOperations filters
│       ├── FilterOperations backdrop_filters
│       ├── gfx::MaskFilterInfo (rounded corners)
│       ├── RenderSurfaceReason
│       ├── transform_id, clip_id
│       └── parent_id
│
└── ScrollTree
    └── ScrollNode[]
        ├── gfx::PointF scroll_offset
        ├── gfx::Size container_bounds
        ├── gfx::Size scroll_bounds
        ├── bool user_scrollable_horizontal/vertical
        └── transform_id
```

### 2. DisplayItemList (from Paint phase)

A serialized list of Skia drawing commands. No pixels are produced yet.

```cpp
DisplayItemList (cc::PaintOpBuffer internally)
└── PaintOp[] (serialized Skia commands)
    ├── DrawRectOp { rect, flags }
    ├── DrawRRectOp { rrect, flags }
    ├── DrawTextBlobOp { blob, x, y, flags }
    ├── DrawImageOp { image, x, y, sampling, flags }
    ├── DrawPathOp { path, flags, use_cache }
    ├── ClipRectOp { rect, antialias }
    ├── ClipRRectOp { rrect, antialias }
    ├── SaveOp { }
    ├── RestoreOp { }
    ├── TranslateOp { dx, dy }
    ├── ConcatOp { matrix }
    ├── SaveLayerOp { bounds, flags } (for opacity groups)
    └── ... many more
```

### 3. PaintChunks

Groups of display items that share the same property tree state.

```cpp
PaintChunk[]
├── begin_index (into DisplayItemList)
├── end_index
├── PropertyTreeState
│   ├── transform_id
│   ├── clip_id
│   └── effect_id
├── bounds (accumulated from items)
├── hit_test_opaqueness
└── effectively_invisible (optimization)
```

### 4. PaintArtifact (combined output)

The combination of DisplayItemList and PaintChunks.

```cpp
PaintArtifact
├── DisplayItemList
├── PaintChunks[]
└── (references PropertyTrees)
```

### 5. Final Output: cc::LayerTreeHost (via PaintArtifactCompositor)

The layer tree that gets sent to the compositor.

```cpp
cc::LayerTreeHost
├── cc::Layer tree
│   └── cc::PictureLayer[]
│       ├── DisplayItemList (or portion of it)
│       ├── bounds
│       ├── transform_tree_index
│       ├── clip_tree_index
│       ├── effect_tree_index
│       └── scroll_tree_index
│
├── PropertyTrees (copied/synced from Blink's)
│
└── cc::LayerTreeImpl (pending tree for compositor thread)
```

---

## Visual Summary

```
┌─────────────────────────────────────────────────────────────────┐
│                         INPUTS                                   │
├─────────────────────────────────────────────────────────────────┤
│  FragmentTree    ComputedStyle    Selection/Focus    Invalidation│
│  (geometry)      (appearance)     (doc state)        (dirty info)│
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                      PRE-PAINT PHASE                             │
│  PrePaintTreeWalk                                                │
│  - Builds PropertyTrees (transform, clip, effect, scroll)        │
│  - Assigns property tree node IDs to fragments                   │
│  - Determines compositing boundaries                             │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                       PAINT PHASE                                │
│  ObjectPainter, BoxFragmentPainter, TextPainter, etc.           │
│  - Walks fragments in stacking order                            │
│  - Emits DisplayItems (recorded Skia ops)                       │
│  - Groups into PaintChunks by property state                    │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                       OUTPUTS                                    │
├─────────────────────────────────────────────────────────────────┤
│  PropertyTrees          PaintArtifact                            │
│  (4 trees)              (DisplayItemList + PaintChunks)          │
└───────────────────────────┬─────────────────────────────────────┘
                            │
                            ▼
┌─────────────────────────────────────────────────────────────────┐
│                 PAINT ARTIFACT COMPOSITOR                        │
│  - Converts PaintChunks → cc::PictureLayer                      │
│  - Syncs PropertyTrees → cc property trees                      │
│  - Produces cc::LayerTreeHost                                   │
└─────────────────────────────────────────────────────────────────┘
```

---

## Key Insight

**Paint doesn't produce pixels** - it produces a **recording** (DisplayItemList) plus **metadata** (PropertyTrees, PaintChunks) that describes how to composite. The actual rasterization happens later in cc.

This separation allows:
- Compositor-thread scrolling without re-painting
- Efficient partial updates (only repaint changed chunks)
- Layer promotion for animations
- Tile-based rasterization in cc
