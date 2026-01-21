# Text Painting Pipeline

## Overview

This document explains how text drawing Skia commands (specifically `DrawTextBlobOp`) are generated in Chromium's Blink rendering engine. Understanding this pipeline is essential for correctly serializing and replaying text paint operations.

## Pipeline Flow

```
┌─────────────────────────────────────────────────────────────────────────────┐
│  Layout Engine                                                               │
│  PhysicalTextFragment                                                        │
│  - Contains text content and positioned fragment info                        │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  TextFragmentPainter::Paint()                                                │
│  third_party/blink/renderer/core/paint/text_fragment_painter.cc             │
│  - Entry point for painting a text fragment from layout                     │
│  - Creates TextPainter with graphics context, font, visual rect, origin     │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  TextPainter::Paint()                                                        │
│  third_party/blink/renderer/core/paint/text_painter.cc                      │
│  - Receives TextFragmentPaintInfo (text, range, shape_result)               │
│  - Applies styling: fill/stroke colors, shadows, decorations                │
│  - Delegates to GraphicsContext::DrawText()                                 │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  GraphicsContext::DrawText()                                                 │
│  third_party/blink/renderer/platform/graphics/graphics_context.cc:429-480  │
│  - Checks for cached text blob (optimization)                               │
│  - Otherwise delegates to Font::DrawText()                                  │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  Font::DrawText()                                                            │
│  third_party/blink/renderer/platform/fonts/font.cc:92-111                   │
│  - Creates ShapeResultBloberizer to convert shaped glyphs to SkTextBlob    │
│  - Calls DrawTextBlobs() to render                                          │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  ShapeResultBloberizer                                                       │
│  third_party/blink/renderer/platform/fonts/shaping/shape_result_bloberizer │
│  - THE CORE TRANSFORMATION LAYER                                            │
│  - Converts HarfBuzz shaping output to SkTextBlob format                    │
│  - Handles horizontal, vertical, and RSXform positioning                    │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  DrawTextBlobs()                                                             │
│  shape_result_bloberizer.cc:611-665                                         │
│  - Iterates through text blobs                                              │
│  - Applies canvas rotation for vertical text                                │
│  - Calls canvas.drawTextBlob() for each blob                                │
└─────────────────────────────────────────────────────────────────────────────┘
                                    │
                                    ▼
┌─────────────────────────────────────────────────────────────────────────────┐
│  cc::PaintCanvas::drawTextBlob()                                             │
│  - Records DrawTextBlobOp in PaintOpBuffer                                  │
│  - This is what gets serialized in raster_source.cc                         │
└─────────────────────────────────────────────────────────────────────────────┘
```

## Key Data Structures

### TextFragmentPaintInfo

Bridge from layout to painting:

```cpp
struct TextFragmentPaintInfo {
  StringView text;              // Full string (may include context)
  unsigned from, to;            // Character range to paint
  ShapeResultView* shape_result;// Shaped glyphs and positions from HarfBuzz
};
```

### ShapeResultView

Wrapper over HarfBuzz shaping results:

```cpp
// Provides ForEachGlyph() callback iteration
// Each callback receives:
struct GlyphData {
  unsigned character_index;     // Original text position
  Glyph glyph;                  // Glyph ID in font file
  gfx::Vector2dF glyph_offset;  // Per-glyph positioning adjustment
  float advance;                // Cumulative horizontal position
  bool is_horizontal;           // Text direction flag
  const SimpleFontData* font_data;  // Font file reference
};
```

### SkTextBlob

Skia's immutable text representation containing:

- **Glyph IDs**: Indices into the font's glyph table
- **Positions**: Where each glyph should be drawn
- **Font**: SkFont object with size, typeface, etc.
- **Optional**: UTF-8 text and cluster mappings (for printing)

## ShapeResultBloberizer: The Core Transformation

This class transforms HarfBuzz shaping output into Skia SkTextBlob objects.

### Internal State

```cpp
struct ShapeResultBloberizer {
  SkTextBlobBuilder builder_;           // Builds SkTextBlob incrementally
  Vector<Glyph> pending_glyphs_;        // Glyph IDs for current run
  Vector<float> pending_offsets_;       // X positions (or X,Y for vertical)
  const SimpleFontData* pending_font_data_;
  CanvasRotationInVertical pending_canvas_rotation_;

  BlobBuffer blobs_;  // Vector of (SkTextBlob, rotation) pairs
  float advance_;     // Total text advance width
};
```

### Glyph Processing

```cpp
// For each glyph from HarfBuzz shaping:
void AddGlyphToBloberizer(glyph, glyph_offset, advance, is_horizontal, font_data) {
  // Calculate position: advance + glyph_offset
  gfx::Vector2dF position = is_horizontal
      ? gfx::Vector2dF(advance, 0)
      : gfx::Vector2dF(0, advance);
  position += glyph_offset;

  bloberizer->Add(glyph, font_data, rotation, position, character_index);
}
```

### Committing Runs to SkTextBlob

When the font or rotation changes, the pending run is committed:

```cpp
void CommitPendingRun() {
  SkFont run_font = pending_font_data_->PlatformData().CreateSkFont();

  // Choose buffer type based on positioning needs
  if (HasPendingVerticalOffsets()) {
    // Vertical or mixed positioning - full (X,Y) per glyph
    buffer = builder_.allocRunPos(run_font, run_size);
  } else {
    // Horizontal only - just X positions (fast path)
    buffer = builder_.allocRunPosH(run_font, run_size, 0);
  }

  // Copy data into SkTextBlob buffer
  std::ranges::copy(pending_glyphs_, buffer.glyphs);
  std::ranges::copy(pending_offsets_, buffer.pos);
}
```

## Glyph Positioning Types

The positioning type determines how glyph positions are stored in the SkTextBlob:

| Type | Value | Description | SkTextBlobBuilder Method |
|------|-------|-------------|-------------------------|
| kDefault | 0 | No positions (use font advances) | `allocRun()` |
| kHorizontal | 1 | X positions only (most common) | `allocRunPosH()` |
| kFull | 2 | Full (X, Y) positions | `allocRunPos()` |
| kRSXform | 3 | Per-glyph rotation/scale/translate | `allocRunRSXform()` |

### When Each Type is Used

- **kHorizontal**: Standard horizontal text (Latin, numbers, etc.)
- **kFull**: Vertical text, or scripts with complex positioning
- **kRSXform**: Per-glyph transformations (rare, used for effects)

## Drawing the TextBlobs

```cpp
void DrawTextBlobs(const BlobBuffer& blobs, cc::PaintCanvas& canvas,
                   const gfx::PointF& point, const cc::PaintFlags& flags,
                   cc::NodeId node_id) {

  for (const auto& blob_info : blobs) {
    cc::PaintCanvasAutoRestore auto_restore(&canvas, false);

    // Handle canvas rotation for vertical text
    switch (blob_info.rotation) {
      case CanvasRotationInVertical::kRegular:
        // No rotation needed
        break;
      case CanvasRotationInVertical::kRotateCanvasUpright:
        // Rotate -90 degrees around text origin
        SkMatrix m;
        m.setSinCos(-1, 0, point.x(), point.y());
        canvas.concat(SkM44(m));
        break;
    }

    // Draw the text blob
    canvas.drawTextBlob(blob_info.blob, point.x(), point.y(), node_id, flags);
  }
}
```

## Data Flow Summary

```
HarfBuzz Shaping                    Layout Engine
      │                                   │
      │ glyph IDs                         │ text_origin (baseline position)
      │ glyph_offset (per-glyph adjust)   │
      │ advance (cumulative position)     │
      │                                   │
      └───────────────┬───────────────────┘
                      │
                      ▼
              ShapeResultBloberizer
                      │
                      │ Combines:
                      │ position = advance + glyph_offset
                      │
                      ▼
                 SkTextBlob
                      │
                      │ Contains:
                      │ - Glyph IDs (font-specific)
                      │ - Pre-computed positions
                      │ - Font reference
                      │
                      ▼
            canvas.drawTextBlob(blob, origin.x, origin.y)
                      │
                      │ Final position = origin + blob_positions
                      │
                      ▼
              DrawTextBlobOp recorded
```

## Serialization Format

The `DrawTextBlobOp` is serialized in `raster_source.cc` with the following structure:

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
      "fontWeight": 400
    }
  ]
}
```

### Positioning-Specific Data

For **kHorizontal** (positioning: 1):
```json
"positions": [0.0, 10.5, 21.0, 31.5, 42.0]  // X values only
```

For **kFull** (positioning: 2):
```json
"positions": [
  {"x": 0.0, "y": 0.0},
  {"x": 10.5, "y": 2.0},
  ...
]
```

For **kRSXform** (positioning: 3):
```json
"rsxforms": [
  {"scos": 1.0, "ssin": 0.0, "tx": 0.0, "ty": 0.0},
  {"scos": 0.99, "ssin": 0.14, "tx": 10.5, "ty": 0.0},
  ...
]
```

RSXform components:
- `scos`: scale * cos(rotation)
- `ssin`: scale * sin(rotation)
- `tx`: translation X
- `ty`: translation Y

## Replaying in test.html

The CanvasKit renderer handles all positioning types:

```javascript
drawTextBlob(canvas, op) {
  for (const run of op.runs) {
    const glyphs = new Uint16Array(run.glyphs);
    const positioning = run.positioning || 0;

    if (positioning === 3 && run.rsxforms) {
      // RSXform: use MakeFromRSXformGlyphs
      const rsxforms = new Float32Array(run.glyphCount * 4);
      for (let i = 0; i < run.rsxforms.length; i++) {
        const xf = run.rsxforms[i];
        rsxforms[i*4] = xf.scos;
        rsxforms[i*4+1] = xf.ssin;
        rsxforms[i*4+2] = xf.tx;
        rsxforms[i*4+3] = xf.ty;
      }
      blob = CanvasKit.TextBlob.MakeFromRSXformGlyphs(glyphs, rsxforms, font);
    } else if (positioning === 2 && run.positions) {
      // Full positioning: convert to RSXform
      // ... convert x,y pairs to identity RSXforms with translation
    } else if (positioning === 1 && run.positions) {
      // Horizontal: convert to RSXform with just X translation
      // ... convert x values to identity RSXforms
    }

    canvas.drawTextBlob(blob, op.x + offsetX, op.y + offsetY, paint);
  }
}
```

## Key Source Files

| File | Purpose |
|------|---------|
| `text_fragment_painter.cc` | Entry point, creates TextPainter |
| `text_painter.cc` | Applies text styling, delegates to GraphicsContext |
| `graphics_context.cc` | Canvas abstraction, handles caching |
| `font.cc` | Creates bloberizer, calls DrawTextBlobs |
| `shape_result_bloberizer.cc` | Core transformation: HarfBuzz → SkTextBlob |
| `raster_source.cc` | Serializes DrawTextBlobOp to JSON |
| `test.html` | Replays DrawTextBlobOp using CanvasKit |

## Related Documentation

- [blink-paint-system.md](blink-paint-system.md) - Overall Blink paint architecture
- [paint-op-json-serialization.md](paint-op-json-serialization.md) - Serialization format details
- [verification-pipeline.md](verification-pipeline.md) - End-to-end testing workflow
