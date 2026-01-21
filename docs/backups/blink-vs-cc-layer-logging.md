# Blink vs CC Layer Logging: Key Differences

This document explains the differences between capturing paint data at the Blink level (via `PaintArtifactCompositor`) versus the CC compositor level (via `RasterSource`).

## Pipeline Overview

```
DOM Tree
    ↓
Layout Tree
    ↓
Paint (Blink)
    ↓
PaintArtifact (PaintChunks + DisplayItems)
    ↓
PaintArtifactCompositor::Update()  ← BLINK LOGGING (blink_layers.json)
    ↓
PendingLayers (Blink's layerization result)
    ↓
cc::LayerTreeHost (handoff to CC)
    ↓
cc::LayerImpl / PictureLayerImpl
    ↓
RasterSource::PlaybackToCanvas()   ← CC LOGGING (layers.json)
    ↓
Tiles / GPU
```

## Logging Points Comparison

| Aspect | Blink Level (`blink_layers.json`) | CC Level (`layers.json`) |
|--------|-----------------------------------|--------------------------|
| **Location** | `PaintArtifactCompositor::Update()` | `RasterSource::PlaybackToCanvas()` |
| **When** | After Blink layerization, before CC handoff | During rasterization |
| **Data Source** | `PendingLayers` + `PaintArtifact` | `cc::LayerImpl` + `DisplayItemList` |
| **Triggered** | Every compositor update | Every raster operation |

## Structural Differences

### 1. Layer Granularity

**Blink Level:**
- Layers are `PendingLayer` objects
- Each contains one or more `PaintChunk`s
- Chunks reference the original `DisplayItem`s
- Layer structure reflects Blink's compositing decisions

**CC Level:**
- Layers are `cc::LayerImpl` objects
- May have been further merged/split by CC
- Contains a flattened `cc::DisplayItemList`
- Layer structure reflects CC's final compositing decisions

### 2. Property Trees

**Blink Level (`blink_property_trees.json`):**
- Uses Blink's `TransformPaintPropertyNode`, `ClipPaintPropertyNode`, `EffectPaintPropertyNode`
- Nodes are garbage-collected objects with parent pointers
- IDs are assigned during serialization (not intrinsic)
- Tree structure matches Blink's property tree representation

**CC Level (`property_trees.json`):**
- Uses CC's `TransformTree`, `ClipTree`, `EffectTree`, `ScrollTree`
- Nodes have integer IDs assigned by CC
- Includes additional CC-specific properties (e.g., scroll offsets, sticky positioning)
- May include nodes that Blink doesn't explicitly create

### 3. Paint Operations

**Blink Level:**
- Paint ops come from `DrawingDisplayItem::GetPaintRecord()`
- Ops are in Blink's original paint order
- May include ops that get culled or optimized away later
- Text operations include Blink's node ID annotations

**CC Level:**
- Paint ops come from `cc::DisplayItemList`
- Ops may have been transformed during CC processing
- Only includes ops that survive to rasterization
- Some Blink-specific annotations may be lost

## Potential Differences in Output

### Layers May Differ

```
Blink: PendingLayer A (chunks 1, 2, 3)
       PendingLayer B (chunks 4, 5)

CC:    Layer X (may combine A + B if compositing allows)
       Layer Y (may split from A if needed for scrolling)
```

Reasons for differences:
- CC may merge layers that Blink kept separate
- CC may create additional layers for scrolling containers
- CC may split layers for tiling purposes
- Compositing reasons may differ between Blink and CC analysis

### Property Tree Node Counts May Differ

Blink property trees are built during paint, while CC property trees are built during commit. CC may:
- Add scroll nodes for overflow containers
- Synthesize transform nodes for layer positioning
- Merge effect nodes that have identical properties
- Add nodes for browser UI elements

### Paint Op Counts May Differ

**More ops at Blink level:**
- Blink may paint content that gets culled by CC
- Blink includes all paint chunks, even those outside viewport
- No dead code elimination has occurred yet

**More ops at CC level:**
- CC may add ops for layer borders (debug)
- CC may add ops for tile boundaries (debug)
- Scrollbar painting may add additional ops

### Coordinate Systems

**Blink Level:**
- Coordinates are in Blink's "paint" coordinate space
- Transforms are relative to the containing stacking context
- Clip rects use Blink's `FloatClipRect` representation

**CC Level:**
- Coordinates are in CC's layer coordinate space
- Transforms are relative to the layer's origin
- Clip rects may be adjusted for layer bounds

## Use Cases

### When to Use Blink-Level Logging

1. **Debugging Blink paint issues** - See exactly what Blink painted
2. **Understanding layerization decisions** - Why did Blink create this layer?
3. **Comparing before/after CC processing** - What did CC change?
4. **DOM-to-paint correlation** - Blink retains more DOM context

### When to Use CC-Level Logging

1. **Debugging rasterization issues** - What actually gets drawn?
2. **Performance analysis** - What ops consume raster time?
3. **Final output verification** - Does the output match expectations?
4. **Tile/GPU debugging** - Understanding the final GPU workload

## File Format Comparison

### blink_layers.json
```json
{
  "BlinkLayers": [
    {
      "layer_id": 0,
      "debug_name": "LayoutView #document",
      "bounds": [0, 0, 800, 600],
      "property_tree_state": {
        "transform_id": 0,
        "clip_id": 0,
        "effect_id": 0
      },
      "paint_chunks": [
        {
          "chunk_id": 0,
          "bounds": [0, 0, 800, 600],
          "paint_ops": [...]
        }
      ]
    }
  ]
}
```

### layers.json (CC)
```json
{
  "LayerTreeImpl": [
    {
      "id": 1,
      "name": "LayoutView #document",
      "bounds": {"width": 800, "height": 600},
      "transform_tree_index": 1,
      "clip_tree_index": 1,
      "effect_tree_index": 1,
      "draws_content": true,
      "paint_ops": [...]
    }
  ]
}
```

### Key Format Differences

| Field | Blink | CC |
|-------|-------|-----|
| Layer ID | `layer_id` (sequential) | `id` (CC-assigned) |
| Name | `debug_name` | `name` |
| Bounds | Array `[x, y, w, h]` | Object `{width, height}` |
| Property refs | `property_tree_state` object | Individual `*_tree_index` fields |
| Paint data | Nested in `paint_chunks` | Direct `paint_ops` array |

## Reconciliation Strategy

To correlate Blink and CC layers:

1. **Match by debug name** - Both include the debug name from `DebugName()`
2. **Compare bounds** - Should be similar (may differ by transform)
3. **Compare paint op counts** - Large differences indicate CC processing
4. **Check property tree mappings** - Follow transform/clip/effect chains

## Summary

| Characteristic | Blink Logging | CC Logging |
|---------------|---------------|------------|
| Timing | Earlier (before CC) | Later (during raster) |
| Fidelity to DOM | Higher | Lower |
| Fidelity to output | Lower | Higher |
| Layer count | May differ | Final count |
| Property trees | Blink's view | CC's view |
| Paint ops | Pre-optimization | Post-optimization |
| Debug use | Paint/layerization bugs | Raster/GPU bugs |
