# Plan: Add Text Painting Logging in Blink (Earlier Pipeline Stage)

## Goal

Add logging in Blink's `DrawTextBlobs()` function to capture text painting data at an earlier stage than the current `raster_source.cc` serialization. The logging will output JSON in the same format as `layers.json`, allowing reconstruction of `DrawTextBlobOp` data from Blink instead of the compositor.

## Background

### Current Flow
```
Blink: DrawTextBlobs() → canvas.drawTextBlob()
   ↓
CC: PaintOpBuffer records DrawTextBlobOp
   ↓
CC: raster_source.cc → SerializeTextBlob() → layers.json
```

### New Flow (this plan)
```
Blink: DrawTextBlobs() → NEW: LogTextBlobOp() → canvas.drawTextBlob()
   ↓
Output: Same JSON format as DrawTextBlobOp in layers.json
```

## Implementation

### Step 1: Create New Serialization File

Create a new file for text blob serialization logic that can be shared/reused:

**File:** `chromium/src/third_party/blink/renderer/platform/fonts/shaping/text_blob_serializer.h`

```cpp
#ifndef THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_TEXT_BLOB_SERIALIZER_H_
#define THIRD_PARTY_BLINK_RENDERER_PLATFORM_FONTS_SHAPING_TEXT_BLOB_SERIALIZER_H_

#include "third_party/skia/include/core/SkTextBlob.h"
#include "ui/gfx/geometry/point_f.h"
#include "cc/paint/paint_flags.h"
#include "cc/paint/node_id.h"

namespace blink {

// Serializes text blob data to JSON format matching DrawTextBlobOp in layers.json
void SerializeTextBlobToLog(const SkTextBlob* blob,
                            const gfx::PointF& point,
                            const cc::PaintFlags& flags,
                            cc::NodeId node_id);

}  // namespace blink

#endif
```

**File:** `chromium/src/third_party/blink/renderer/platform/fonts/shaping/text_blob_serializer.cc`

This file will contain the serialization logic, identical to `SerializeTextBlob()` in `raster_source.cc` but outputting via `LOG(ERROR)` with a marker prefix for extraction.

Key includes needed:
- `SkTextBlobPriv.h` for `SkTextBlobRunIterator`
- `SkRSXform.h` for RSXform data
- `base/json/json_writer.h` or `base/trace_event/traced_value.h` for JSON

### Step 2: Implement SerializeTextBlobToLog

The function will:
1. Create a JSON structure matching the `DrawTextBlobOp` format in `layers.json`
2. Use `SkTextBlobRunIterator` to iterate through runs (same as `raster_source.cc`)
3. Serialize:
   - `type`: "DrawTextBlobOp"
   - `x`, `y`: from `point` parameter
   - `nodeId`: from `node_id` parameter
   - `flags`: color (r, g, b, a) from `flags.getColor4f()`
   - `bounds`: from `blob->bounds()`
   - `runs[]`: array of run data
     - `glyphCount`, `glyphs[]`
     - `positioning` (0-3)
     - `positions[]` or `rsxforms[]` depending on positioning type
     - `offsetX`, `offsetY`
     - `fontSize`, `fontFamily`, `fontWeight`
4. Output via `LOG(ERROR) << "BLINK_TEXT_BLOB: " << json_string;`

### Step 3: Add Logging Call to DrawTextBlobs

**File:** `chromium/src/third_party/blink/renderer/platform/fonts/shaping/shape_result_bloberizer.cc`

In `DrawTextBlobs()` function (line 611-665), add logging before each `canvas.drawTextBlob()` call:

```cpp
void DrawTextBlobs(const ShapeResultBloberizer::BlobBuffer& blobs,
                   cc::PaintCanvas& canvas,
                   const gfx::PointF& point,
                   const cc::PaintFlags& flags,
                   cc::NodeId node_id) {
  for (const auto& blob_info : blobs) {
    DCHECK(blob_info.blob);

    // NEW: Log text blob data before drawing
    SerializeTextBlobToLog(blob_info.blob.get(), point, flags, node_id);

    // ... rest of existing code (handles rotation, then calls canvas.drawTextBlob)
  }
}
```

### Step 4: Update BUILD.gn

Add the new source files to the appropriate BUILD.gn:

**File:** `chromium/src/third_party/blink/renderer/platform/fonts/BUILD.gn`

Add to sources:
```
"shaping/text_blob_serializer.cc",
"shaping/text_blob_serializer.h",
```

### Step 5: Update Extraction Script

Modify `eval/extract_json.sh` to also extract `BLINK_TEXT_BLOB:` log entries and merge them into layers.json or a separate file (e.g., `blink_text.json`).

## Files to Modify

| File | Action |
|------|--------|
| `third_party/blink/renderer/platform/fonts/shaping/text_blob_serializer.h` | CREATE |
| `third_party/blink/renderer/platform/fonts/shaping/text_blob_serializer.cc` | CREATE |
| `third_party/blink/renderer/platform/fonts/shaping/shape_result_bloberizer.cc` | MODIFY (add include + logging call) |
| `third_party/blink/renderer/platform/fonts/BUILD.gn` | MODIFY (add new sources) |
| `eval/extract_json.sh` | MODIFY (extract BLINK_TEXT_BLOB entries) |

## JSON Output Format

The logged JSON will match the existing `DrawTextBlobOp` format:

```json
{
  "type": "DrawTextBlobOp",
  "x": 100.0,
  "y": 50.0,
  "nodeId": 42,
  "flags": {
    "r": 0.0, "g": 0.0, "b": 0.0, "a": 1.0
  },
  "bounds": [95.0, 35.0, 200.0, 55.0],
  "runs": [
    {
      "glyphCount": 5,
      "glyphs": [43, 72, 79, 79, 82],
      "positioning": 1,
      "positions": [0.0, 10.5, 21.0, 31.5, 42.0],
      "offsetX": 0.0,
      "offsetY": 0.0,
      "fontSize": 16.0,
      "fontFamily": "Arial",
      "fontWeight": 400
    }
  ]
}
```

## Data Available at DrawTextBlobs()

At this point in the pipeline, we have access to:

| Data | Source |
|------|--------|
| SkTextBlob | `blob_info.blob` - contains all glyph data |
| Canvas position (x, y) | `point` parameter |
| Paint color | `flags.getColor4f()` |
| DOM Node ID | `node_id` parameter |
| Rotation info | `blob_info.rotation` (for vertical text) |

The SkTextBlob internally contains (accessible via `SkTextBlobRunIterator`):
- Glyph IDs
- Positioning type (horizontal, full, RSXform)
- Position data
- Font (size, typeface with family and weight)
- Run offsets

## Comparison: Blink vs CC Logging

| Aspect | Blink (DrawTextBlobs) | CC (raster_source.cc) |
|--------|----------------------|----------------------|
| Stage | Before rasterization | During layer tree dump |
| Data source | SkTextBlob + params | DrawTextBlobOp |
| Rotation handling | See `blob_info.rotation` | Not visible |
| Call frequency | Per text draw | Per layer serialize |

## Verification

1. Build Chromium with changes: `./scripts/build.sh`
2. Run `./eval/verification-pipeline.sh`
3. Check `chrome_debug.log` for `BLINK_TEXT_BLOB:` entries
4. Verify JSON structure matches existing `DrawTextBlobOp` in `layers.json`
5. Compare Blink-logged data with raster_source.cc-logged data for same text elements

## Related Documentation

- [text-painting-pipeline.md](text-painting-pipeline.md) - Full text rendering pipeline
- [verification-pipeline.md](verification-pipeline.md) - End-to-end testing workflow
- [paint-op-json-serialization.md](paint-op-json-serialization.md) - Serialization format details
