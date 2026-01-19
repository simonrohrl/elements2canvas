# Plan: Log Paint Artifact at Blink's Post-Paint Stage

## Goal

Add logging in Blink **after the paint phase but before compositing** to capture the complete `PaintArtifact`. This intermediate representation contains all the data needed to reconstruct the `layers.json` structure (paint operations, property trees, layer boundaries).

## Background

### Current Flow (layers.json captured at CC level)
```
Blink Paint Phase
    ↓
PaintController::CommitNewDisplayItems()
    ↓
PaintArtifact (DisplayItems + PaintChunks + PropertyTreeState)
    ↓
PaintArtifactCompositor::Update()  ←── Layerization happens here
    ↓
cc::LayerTreeHost (cc::Layers + PropertyTrees)
    ↓
raster_source.cc → layers.json  ←── CURRENT CAPTURE POINT
```

### New Flow (capture at Blink post-paint)
```
Blink Paint Phase
    ↓
PaintController::CommitNewDisplayItems()
    ↓
PaintArtifact  ←── NEW CAPTURE POINT (contains all data for layers.json)
    ↓
... rest of pipeline continues unchanged
```

## Key Insight

The `PaintArtifact` is the complete intermediate representation that contains:
- **DisplayItemList**: All paint operations (DrawingDisplayItem contains PaintRecord with paint ops)
- **PaintChunks**: Groups of display items with common properties (become cc::Layers)
- **PropertyTreeState**: Transform, effect, clip, scroll nodes for each chunk

This is exactly what gets converted into the CC layer tree. By logging at this stage, we capture the same data that `raster_source.cc` serializes, but earlier in the pipeline.

## Data Structures

### PaintArtifact
```cpp
// File: third_party/blink/renderer/platform/graphics/paint/paint_artifact.h
class PaintArtifact {
  DisplayItemList& GetDisplayItemList();    // All paint items
  PaintChunks& GetPaintChunks();            // Chunk groupings

  // Built-in JSON serialization:
  std::unique_ptr<JSONArray> ToJSON() const;

  // Debug helpers:
  String ClientDebugName(DisplayItemClientId) const;
  DOMNodeId ClientOwnerNodeId(DisplayItemClientId) const;
};
```

### PaintChunk (becomes cc::Layer)
```cpp
// File: third_party/blink/renderer/platform/graphics/paint/paint_chunk.h
struct PaintChunk {
  wtf_size_t begin_index;         // Start in DisplayItemList
  wtf_size_t end_index;           // End in DisplayItemList
  PropertyTreeState properties;   // Transform, effect, clip, scroll nodes
  gfx::Rect bounds;               // Chunk bounds
  gfx::Rect drawable_bounds;      // Drawable area
  SkColor4f background_color;     // For compositing optimization
  PaintChunkId id;                // Unique identifier
  HitTestData* hit_test_data;     // Hit testing info
};
```

### PropertyTreeState (maps to property_trees.json)
```cpp
struct PropertyTreeState {
  const TransformPaintPropertyNode& Transform();
  const ClipPaintPropertyNode& Clip();
  const EffectPaintPropertyNode& Effect();
};
```

### DisplayItem (contains paint operations)
```cpp
class DrawingDisplayItem : public DisplayItem {
  PaintRecord GetPaintRecord();  // Contains the actual paint ops
  gfx::Rect VisualRect();
};
```

## Interception Point

**File:** `third_party/blink/renderer/core/frame/local_frame_view.cc`

**Function:** `LocalFrameView::PaintTree()` (around line 3088)

**Location:** Immediately after `paint_controller->CommitNewDisplayItems();`

```cpp
void LocalFrameView::PaintTree() {
  // ... painting happens ...

  paint_controller->CommitNewDisplayItems();  // Paint phase complete

  // ← INSERT LOGGING HERE
  // PaintArtifact is now complete and accessible via:
  // paint_controller_persistent_data_->GetPaintArtifact()

  // ... continues to compositor handoff ...
}
```

## Implementation

### Step 1: Create Serialization Module

**File:** `third_party/blink/renderer/platform/graphics/paint/paint_artifact_serializer.h`

```cpp
#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_ARTIFACT_SERIALIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_GRAPHICS_PAINT_PAINT_ARTIFACT_SERIALIZER_H_

#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact.h"

namespace blink {

// Serializes PaintArtifact to JSON format matching layers.json structure
void SerializePaintArtifactToLog(const PaintArtifact& artifact);

}  // namespace blink

#endif
```

**File:** `third_party/blink/renderer/platform/graphics/paint/paint_artifact_serializer.cc`

The implementation will:
1. Iterate through PaintChunks (each becomes a layer entry)
2. For each chunk, extract:
   - Bounds (maps to layer bounds in layers.json)
   - PropertyTreeState (maps to transform_id, effect_id, clip_id)
   - DisplayItems in range [begin_index, end_index]
3. For each DrawingDisplayItem:
   - Extract PaintRecord
   - Iterate paint ops (same as raster_source.cc does)
4. Serialize property tree nodes (transform, effect, clip trees)
5. Output via `LOG(ERROR) << "BLINK_PAINT_ARTIFACT: " << json;`

### Step 2: Add Logging Call

**File:** `third_party/blink/renderer/core/frame/local_frame_view.cc`

```cpp
#include "third_party/blink/renderer/platform/graphics/paint/paint_artifact_serializer.h"

void LocalFrameView::PaintTree() {
  // ... existing paint code ...

  paint_controller->CommitNewDisplayItems();

  // NEW: Log complete paint artifact
  if (paint_controller_persistent_data_) {
    SerializePaintArtifactToLog(
        paint_controller_persistent_data_->GetPaintArtifact());
  }

  // ... rest of function ...
}
```

### Step 3: Update BUILD.gn

**File:** `third_party/blink/renderer/platform/graphics/paint/BUILD.gn`

Add new source files to the build.

### Step 4: Update Extraction Script

**File:** `eval/extract_json.sh`

Add extraction for `BLINK_PAINT_ARTIFACT:` log entries.

## JSON Output Format

The output will mirror `layers.json` structure:

```json
{
  "PaintArtifact": {
    "chunks": [
      {
        "id": 0,
        "bounds": [0, 0, 800, 600],
        "drawable_bounds": [0, 0, 800, 600],
        "background_color": {"r": 1, "g": 1, "b": 1, "a": 1},
        "properties": {
          "transform_id": 1,
          "effect_id": 1,
          "clip_id": 1
        },
        "paint_ops": [
          {
            "type": "DrawRectOp",
            "rect": [0, 0, 100, 50],
            "flags": {"r": 1, "g": 0, "b": 0, "a": 1}
          },
          {
            "type": "DrawTextBlobOp",
            "x": 10, "y": 30,
            "runs": [...]
          }
        ]
      }
    ],
    "property_trees": {
      "transform_tree": {
        "nodes": [
          {"id": 0, "parent_id": -1, "local": [1,0,0,0, 0,1,0,0, 0,0,1,0, 0,0,0,1]},
          {"id": 1, "parent_id": 0, "local": [...]}
        ]
      },
      "effect_tree": {
        "nodes": [
          {"id": 0, "parent_id": -1, "opacity": 1},
          {"id": 1, "parent_id": 0, "opacity": 0.5}
        ]
      },
      "clip_tree": {
        "nodes": [...]
      }
    }
  }
}
```

## Mapping: PaintArtifact → layers.json

| PaintArtifact | layers.json |
|---------------|-------------|
| PaintChunk | Layer in LayerTreeImpl |
| PaintChunk.bounds | layer.bounds |
| PaintChunk.properties.Transform() | layer.transform_id |
| PaintChunk.properties.Effect() | layer.effect_id |
| PaintChunk.properties.Clip() | layer.clip_id |
| DisplayItem.GetPaintRecord() | layer.paint_ops |
| TransformPaintPropertyNode | property_trees.transform_tree |
| EffectPaintPropertyNode | property_trees.effect_tree |
| ClipPaintPropertyNode | property_trees.clip_tree |

## Files to Modify

| File | Action |
|------|--------|
| `platform/graphics/paint/paint_artifact_serializer.h` | CREATE |
| `platform/graphics/paint/paint_artifact_serializer.cc` | CREATE |
| `core/frame/local_frame_view.cc` | MODIFY (add logging call) |
| `platform/graphics/paint/BUILD.gn` | MODIFY (add sources) |
| `eval/extract_json.sh` | MODIFY (extract BLINK_PAINT_ARTIFACT) |

## Existing JSON Support

Note: `PaintArtifact` already has `ToJSON()` method! We may be able to leverage this:

```cpp
std::unique_ptr<JSONArray> PaintArtifact::ToJSON() const;
```

Also, `PaintArtifactCompositor` has debug logging:
```cpp
// paint_artifact_compositor.cc:1158-1162
DVLOG(2) << "PaintArtifactCompositor::Update() done\n"
         << "Composited layers:\n"
         << GetLayersAsJSON(VLOG_IS_ON(3) ? 0xffffffff : 0)
                  ->ToPrettyJSONString()
                  .Utf8();
```

## Verification

1. Build Chromium with changes
2. Run `./eval/verification-pipeline.sh`
3. Extract `BLINK_PAINT_ARTIFACT:` from logs
4. Compare structure with existing `layers.json`:
   - Same number of layers/chunks
   - Same paint operations per layer
   - Same property tree structure
5. Verify `test.html` can render from Blink-captured data

## Next Steps After Implementation

Once this logging is working, we can:
1. Compare Blink PaintArtifact with CC layers.json to verify 1:1 mapping
2. Understand how `PaintArtifactCompositor::Layerize()` groups chunks into layers
3. Potentially reconstruct layers.json entirely from Blink-side logging
4. Remove dependency on CC-level serialization in `raster_source.cc`

## Related Documentation

- [verification-pipeline.md](verification-pipeline.md) - End-to-end testing workflow
- [text-painting-pipeline.md](text-painting-pipeline.md) - Text-specific paint flow
- [paint-op-json-serialization.md](paint-op-json-serialization.md) - Serialization format
- [blink-paint-system.md](blink-paint-system.md) - Overall Blink paint architecture
