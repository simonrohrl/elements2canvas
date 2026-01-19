# Simplifying Chromium's Compositor (cc) with Direct Skia Rendering

## Overview

Chromium's `cc` (Compositor) layer is a sophisticated system that converts recorded paint operations into pixels and composites them into the final frame. While highly optimized, its complexity exists primarily for **efficiency** rather than **correctness**. This document explains how the entire system works and how it can be replaced with simple Skia calls.

## The Chromium Rendering Pipeline

```
Blink (DOM/Layout/Paint)
         ↓
    DisplayItemList (recorded paint ops)
         ↓
    cc/raster (tile-based rasterization)
         ↓
    cc/layers (AppendQuads → CompositorFrame)
         ↓
    viz (GPU compositing)
         ↓
    Screen
```

### Stage 1: Paint Recording

Blink records drawing commands into a `DisplayItemList` - a serialized list of Skia operations like "draw rect", "draw text", "draw image". No pixels are produced yet.

### Stage 2: Rasterization (cc/raster)

The `cc/raster` subsystem converts paint operations into GPU textures. This is **not** simple Skia-to-pixels conversion - it includes:

- **Tiling**: Layers are divided into 256x256 tiles for memory efficiency
- **Partial updates**: Only dirty tiles are re-rasterized
- **Multiple backends**: GPU raster, CPU-to-GPU copy, or pure CPU paths
- **Task scheduling**: Complex worker thread coordination with priority management
- **Resource pooling**: Tile textures are reused across frames

Key classes:
- `RasterSource`: Wraps the `DisplayItemList`, provides `PlaybackToCanvas()`
- `TileManager`: Schedules and prioritizes tile rasterization
- `RasterBufferProvider`: Factory for different raster backends (GPU, OneCopy, ZeroCopy)

### Stage 3: Compositing (cc/layers → viz)

After rasterization, each tile has a `TileDrawInfo` containing either:
- `RESOURCE_MODE`: A GPU texture reference
- `SOLID_COLOR_MODE`: Just a color (optimization for solid tiles)
- `OOM_MODE`: Rasterization failed

The `AppendQuads()` method on each layer iterates visible tiles and emits `DrawQuad` objects:

```cpp
for (auto iter = Cover(visible_rect); iter; ++iter) {
    if (iter->draw_info().IsReadyToDraw()) {
        auto* quad = render_pass->CreateAndAppendDrawQuad<TileDrawQuad>();
        quad->SetNew(..., draw_info.resource_id(), texture_rect, ...);
    }
}
```

These quads are collected into a `CompositorFrame` and sent to `viz`, which executes actual GPU draw calls.

## The Four Property Trees

Instead of storing transform/clip/effect data on each layer, Chromium uses four separate trees that layers index into:

### 1. TransformTree

```cpp
struct TransformNode {
    gfx::Transform local;      // Local transform
    gfx::Transform to_parent;  // Combined transform to parent
    int parent_id;             // Parent node in tree
};
```

Provides the 4x4 transformation matrix for positioning layers.

### 2. ClipTree

```cpp
struct ClipNode {
    gfx::RectF clip;           // Clip rect in local space
    int transform_id;          // Which transform space
    int parent_id;             // Parent clip (clips accumulate)
};
```

Defines rectangular clip regions.

### 3. EffectTree

```cpp
struct EffectNode {
    float opacity;
    SkBlendMode blend_mode;
    FilterOperations filters;           // blur, brightness, etc.
    FilterOperations backdrop_filters;  // backdrop-filter CSS
    gfx::MaskFilterInfo mask_filter_info;  // rounded corners
    RenderSurfaceReason render_surface_reason;
};
```

Contains visual effects: opacity, blend modes, filters, rounded corners.

### 4. ScrollTree

```cpp
struct ScrollNode {
    gfx::PointF current_scroll_offset;
    gfx::Size container_bounds;
    gfx::Size bounds;
};
```

Tracks scroll positions (usually baked into transforms).

## Simplification: Direct Skia Rendering

The entire cc infrastructure can be replaced with direct Skia calls if you don't need the efficiency optimizations.

### Simplified Rasterization

Instead of tiling, rasterize each layer as a single image:

```cpp
SkImage* RasterLayer(Layer* layer) {
    DisplayItemList* list = layer->GetDisplayItemList();
    gfx::Size size = layer->bounds();

    auto surface = SkSurfaces::Raster(
        SkImageInfo::MakeN32Premul(size.width(), size.height()));

    list->Raster(surface->getCanvas());

    return surface->makeImageSnapshot().release();
}
```

### Simplified Compositing

Apply the property trees using basic Skia operations:

```cpp
void CompositeLayer(SkCanvas* canvas,
                    LayerImpl* layer,
                    SkImage* rasterized_content,
                    PropertyTrees* trees) {
    canvas->save();

    // 1. TRANSFORM - from TransformTree
    gfx::Transform screen_xform =
        trees->transform_tree().ToScreen(layer->transform_tree_index());
    canvas->concat(ToSkM44(screen_xform));

    // 2. CLIP - from ClipTree
    ClipNode* clip = trees->clip_tree().Node(layer->clip_tree_index());
    if (clip->AppliesLocalClip()) {
        canvas->clipRect(ToSkRect(clip->clip));
    }

    // 3. EFFECTS - from EffectTree
    EffectNode* effect = trees->effect_tree().Node(layer->effect_tree_index());

    // Rounded corners
    if (!effect->mask_filter_info.IsEmpty()) {
        canvas->clipRRect(
            ToSkRRect(effect->mask_filter_info.rounded_corner_bounds()));
    }

    // Opacity + blend mode
    SkPaint paint;
    paint.setAlphaf(effect->opacity);
    paint.setBlendMode(effect->blend_mode);

    // Filters (blur, etc.)
    if (!effect->filters.IsEmpty()) {
        paint.setImageFilter(BuildSkImageFilter(effect->filters));
    }

    // 4. DRAW
    canvas->drawImage(rasterized_content, 0, 0, SkSamplingOptions(), &paint);

    canvas->restore();
}
```

### Handling RenderSurfaces

Some effects require an intermediate surface (when `render_surface_reason != kNone`):

- **Group opacity**: Children must blend together before applying parent opacity
- **Filters**: Need the composited result as input
- **Backdrop filters**: Need to capture what's behind
- **Masks**: Need intermediate for masking operation

```cpp
void CompositeWithSurface(SkCanvas* output, Layer* layer) {
    EffectNode* effect = GetEffectNode(layer);

    if (effect->render_surface_reason != RenderSurfaceReason::kNone) {
        // Create offscreen surface
        auto offscreen = SkSurfaces::Raster(
            SkImageInfo::MakeN32Premul(layer->width(), layer->height()));

        // Composite all children to offscreen (at full opacity)
        for (Layer* child : layer->children()) {
            CompositeLayer(offscreen->getCanvas(), child, ...);
        }

        // Draw offscreen to output with effects
        SkPaint paint;
        paint.setAlphaf(effect->opacity);
        output->drawImage(offscreen->makeImageSnapshot(), 0, 0,
                          SkSamplingOptions(), &paint);
    } else {
        // Direct compositing
        CompositeLayer(output, layer, ...);
    }
}
```

### Complete Simple Compositor

```cpp
void SimpleCompositor::DrawFrame(SkCanvas* output, LayerTreeImpl* tree) {
    PropertyTrees* trees = tree->property_trees();

    output->clear(SK_ColorWHITE);

    // Pre-rasterize all layers (could be cached)
    std::map<LayerImpl*, sk_sp<SkImage>> rasterized;
    for (LayerImpl* layer : tree->AllLayers()) {
        rasterized[layer] = RasterLayer(layer);
    }

    // Composite in draw order (back to front)
    for (LayerImpl* layer : tree->LayersInDrawOrder()) {
        CompositeLayer(output, layer, rasterized[layer].get(), trees);
    }
}
```

## What You Lose

| Feature | Tiled cc | Simple Skia |
|---------|----------|-------------|
| Partial updates | Only dirty tiles | Full re-raster |
| Memory efficiency | ~256KB per visible tile | Full layer allocation |
| Async rasterization | Worker threads | Single-threaded |
| Progressive loading | High-priority first | All or nothing |
| GPU acceleration | GPU raster path | CPU only (unless GPU canvas) |
| Scale handling | Multiple tilings | Re-raster on zoom |

## What You Gain

- **~200 lines** instead of tens of thousands in cc/tiles, cc/layers, viz
- **Easier to understand** and modify
- **Direct Skia access** for custom rendering
- **No complex synchronization** between threads
- **Same visual output** - the pixel results are identical

## Summary

Chromium's cc compositor is essentially:

1. **Raster**: `DisplayItemList` → pixels (with tiling optimization)
2. **Composite**: Apply transforms, clips, effects, and blend layers together

Both operations are fundamentally simple Skia operations. The complexity in cc exists for performance optimization (tiling, partial updates, GPU acceleration, async scheduling), not correctness. For simpler use cases, the entire system can be replaced with direct `SkCanvas` calls.
