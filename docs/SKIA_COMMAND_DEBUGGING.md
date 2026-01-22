# Skia Command Debugging System

This document describes a systematic approach to debugging rendering differences between Chromium and the test.html CanvasKit renderer by comparing actual Skia commands.

## Overview

The debugging system has three components:

1. **Chromium-side Skia command logging** - Logs actual `SkCanvas` calls during paint
2. **test_debug.html** - CanvasKit renderer with equivalent command logging
3. **skia_diff.py** - Tool to compare and analyze the two command streams

## How It Works

```
┌─────────────────────────────────────────────────────────────────┐
│                      CHROMIUM                                   │
│  Paint Ops → SkCanvas calls → chromium_skia_commands.json       │
└─────────────────────────────────────────────────────────────────┘
                              ↓ compare
┌─────────────────────────────────────────────────────────────────┐
│                      test_debug.html                            │
│  raw_paint_ops.json → CanvasKit → canvaskit_skia_commands.json  │
└─────────────────────────────────────────────────────────────────┘
```

## Part 1: Chromium-side Logging

You need to add a logging wrapper around SkCanvas in Chromium. The key location is in `cc/raster/raster_source.cc` where paint operations are rasterized.

### Implementation Guide

Create a `LoggingCanvas` class that wraps the real `SkCanvas`:

```cpp
// In cc/raster/raster_source.cc or a new file

#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/utils/SkParsePath.h"
#include "base/json/json_writer.h"
#include "base/values.h"

class SkiaCommandLogger {
 public:
  static SkiaCommandLogger& GetInstance() {
    static SkiaCommandLogger instance;
    return instance;
  }

  void SetEnabled(bool enabled) { enabled_ = enabled; }
  bool IsEnabled() const { return enabled_; }

  void Clear() {
    commands_.clear();
    command_index_ = 0;
  }

  void LogCommand(base::Value::Dict cmd) {
    if (!enabled_) return;
    cmd.Set("index", command_index_++);
    commands_.Append(std::move(cmd));
  }

  void WriteToFile(const std::string& path) {
    base::Value::Dict root;
    root.Set("type", "chromium_skia_commands");
    root.Set("version", 1);
    root.Set("commands", std::move(commands_));

    std::string json;
    base::JSONWriter::WriteWithOptions(
        root, base::JSONWriter::OPTIONS_PRETTY_PRINT, &json);

    // Write to file...
  }

 private:
  bool enabled_ = false;
  int command_index_ = 0;
  base::Value::List commands_;
};

// Wrapper canvas that logs all operations
class LoggingCanvas : public SkCanvas {
 public:
  explicit LoggingCanvas(SkCanvas* target) : target_(target) {}

  int save() override {
    base::Value::Dict cmd;
    cmd.Set("type", "save");
    SkiaCommandLogger::GetInstance().LogCommand(std::move(cmd));
    return target_->save();
  }

  void restore() override {
    base::Value::Dict cmd;
    cmd.Set("type", "restore");
    SkiaCommandLogger::GetInstance().LogCommand(std::move(cmd));
    target_->restore();
  }

  void translate(SkScalar dx, SkScalar dy) override {
    base::Value::Dict cmd;
    cmd.Set("type", "translate");
    cmd.Set("dx", dx);
    cmd.Set("dy", dy);
    SkiaCommandLogger::GetInstance().LogCommand(std::move(cmd));
    target_->translate(dx, dy);
  }

  void concat(const SkM44& m44) override {
    base::Value::Dict cmd;
    cmd.Set("type", "concat44");
    base::Value::List matrix;
    for (int row = 0; row < 4; row++) {
      for (int col = 0; col < 4; col++) {
        matrix.Append(m44.rc(row, col));
      }
    }
    cmd.Set("matrix", std::move(matrix));
    SkiaCommandLogger::GetInstance().LogCommand(std::move(cmd));
    target_->concat(m44);
  }

  void clipRect(const SkRect& rect, SkClipOp op, bool doAntiAlias) override {
    base::Value::Dict cmd;
    cmd.Set("type", "clipRect");
    base::Value::List r;
    r.Append(rect.fLeft);
    r.Append(rect.fTop);
    r.Append(rect.fRight);
    r.Append(rect.fBottom);
    cmd.Set("rect", std::move(r));
    cmd.Set("clipOp", static_cast<int>(op));
    cmd.Set("antiAlias", doAntiAlias);
    SkiaCommandLogger::GetInstance().LogCommand(std::move(cmd));
    target_->clipRect(rect, op, doAntiAlias);
  }

  void drawRect(const SkRect& rect, const SkPaint& paint) override {
    base::Value::Dict cmd;
    cmd.Set("type", "drawRect");
    base::Value::List r;
    r.Append(rect.fLeft);
    r.Append(rect.fTop);
    r.Append(rect.fRight);
    r.Append(rect.fBottom);
    cmd.Set("rect", std::move(r));
    cmd.Set("paint", SerializePaint(paint));
    SkiaCommandLogger::GetInstance().LogCommand(std::move(cmd));
    target_->drawRect(rect, paint);
  }

  void drawRRect(const SkRRect& rrect, const SkPaint& paint) override {
    base::Value::Dict cmd;
    cmd.Set("type", "drawRRect");
    cmd.Set("rrect", SerializeRRect(rrect));
    cmd.Set("paint", SerializePaint(paint));
    SkiaCommandLogger::GetInstance().LogCommand(std::move(cmd));
    target_->drawRRect(rrect, paint);
  }

  void drawPath(const SkPath& path, const SkPaint& paint) override {
    base::Value::Dict cmd;
    cmd.Set("type", "drawPath");
    SkString svg;
    SkParsePath::ToSVGString(path, &svg);
    cmd.Set("path", std::string(svg.c_str()));
    cmd.Set("paint", SerializePaint(paint));
    SkiaCommandLogger::GetInstance().LogCommand(std::move(cmd));
    target_->drawPath(path, paint);
  }

  void drawTextBlob(const SkTextBlob* blob, SkScalar x, SkScalar y,
                    const SkPaint& paint) override {
    base::Value::Dict cmd;
    cmd.Set("type", "drawTextBlob");
    cmd.Set("x", x);
    cmd.Set("y", y);
    cmd.Set("paint", SerializePaint(paint));
    // Add glyph data from blob...
    SkiaCommandLogger::GetInstance().LogCommand(std::move(cmd));
    target_->drawTextBlob(blob, x, y, paint);
  }

  // ... implement other methods similarly

 private:
  base::Value::Dict SerializePaint(const SkPaint& paint) {
    base::Value::Dict obj;
    SkColor4f c = paint.getColor4f();
    base::Value::Dict color;
    color.Set("r", c.fR);
    color.Set("g", c.fG);
    color.Set("b", c.fB);
    color.Set("a", c.fA);
    obj.Set("color", std::move(color));
    obj.Set("style", static_cast<int>(paint.getStyle()));
    obj.Set("strokeWidth", paint.getStrokeWidth());
    obj.Set("antiAlias", paint.isAntiAlias());
    obj.Set("blendMode", static_cast<int>(paint.getBlendMode()));
    return obj;
  }

  base::Value::Dict SerializeRRect(const SkRRect& rr) {
    base::Value::Dict obj;
    base::Value::List rect;
    rect.Append(rr.rect().fLeft);
    rect.Append(rr.rect().fTop);
    rect.Append(rr.rect().fRight);
    rect.Append(rr.rect().fBottom);
    obj.Set("rect", std::move(rect));

    base::Value::List radii;
    for (int i = 0; i < 4; i++) {
      SkVector r = rr.radii(static_cast<SkRRect::Corner>(i));
      radii.Append(r.x());
      radii.Append(r.y());
    }
    obj.Set("radii", std::move(radii));
    return obj;
  }

  SkCanvas* target_;
};
```

### Where to Insert the Wrapper

In `cc/raster/raster_source.cc`, find the `PlaybackToCanvas` function and wrap the canvas:

```cpp
void RasterSource::PlaybackToCanvas(SkCanvas* canvas, ...) {
  // Create logging wrapper
  LoggingCanvas* logCanvas = nullptr;
  if (SkiaCommandLogger::GetInstance().IsEnabled()) {
    logCanvas = new LoggingCanvas(canvas);
    canvas = logCanvas;
  }

  // ... existing paint playback code ...

  if (logCanvas) {
    delete logCanvas;
  }
}
```

### Triggering the Log

Add a command-line flag or DevTools protocol to enable logging:

```cpp
// In chrome's main process
void EnableSkiaLogging() {
  SkiaCommandLogger::GetInstance().SetEnabled(true);
  SkiaCommandLogger::GetInstance().Clear();
}

void SaveSkiaLog(const std::string& path) {
  SkiaCommandLogger::GetInstance().WriteToFile(path);
  SkiaCommandLogger::GetInstance().SetEnabled(false);
}
```

## Part 2: Using test_debug.html

1. Open `test_debug.html` in a browser (needs to be served, not file://)
2. Ensure "Log Skia Commands" checkbox is checked
3. The canvas will render and log all CanvasKit calls
4. Click "Download Command Log (JSON)" to save `canvaskit_skia_commands.json`

The log format matches the Chromium format:

```json
{
  "type": "canvaskit_skia_commands",
  "version": 1,
  "commands": [
    {"index": 0, "type": "clear", "color": ...},
    {"index": 1, "type": "save"},
    {"index": 2, "type": "concat44", "matrix": [...]},
    {"index": 3, "type": "drawRect", "rect": [...], "paint": {...}},
    ...
  ]
}
```

## Part 3: Using the Diff Tool

```bash
# Basic comparison
python tools/skia_diff.py chromium_commands.json canvaskit_commands.json

# With verbose output
python tools/skia_diff.py chromium_commands.json canvaskit_commands.json -v

# Save detailed diff to JSON
python tools/skia_diff.py chromium_commands.json canvaskit_commands.json -o diff_report.json

# Use type-based alignment (better for command order differences)
python tools/skia_diff.py chromium_commands.json canvaskit_commands.json --align-by type

# Adjust numeric tolerance
python tools/skia_diff.py chromium_commands.json canvaskit_commands.json -t 0.01
```

### Example Output

```
============================================================
SKIA COMMAND DIFF REPORT
============================================================

Total commands compared: 1234
  Matches:              1180 (95.6%)
  Mismatches:           42
  Missing in Chromium:  5
  Missing in CanvasKit: 7
  Type mismatches:      0

By command type:
------------------------------------------------------------
  concat44             total= 156  matches= 140 (89.7%)  mismatches=  16
  drawRect             total= 234  matches= 220 (94.0%)  mismatches=  14
  drawTextBlob         total=  89  matches=  77 (86.5%)  mismatches=  12

============================================================
DIFFERENCES
============================================================

[42] mismatch
  Chromium: concat44
  CanvasKit: concat44
    - .matrix[3]: 100.0 vs 100.5 (diff: 0.500000)
    - .matrix[7]: 50.0 vs 50.25 (diff: 0.250000)

[156] mismatch
  Chromium: drawRect
  CanvasKit: drawRect
    - .paint.color.a: 0.8 vs 1.0 (diff: 0.200000)
```

## Interpreting Results

### Common Issues

1. **Matrix differences** - Usually indicates:
   - Different property tree accumulation logic
   - Transform origin handling differences
   - Device pixel ratio handling

2. **Color/alpha differences** - Usually indicates:
   - Effect tree opacity not being applied
   - Blend mode handling differences
   - Shadow color transformation issues

3. **Missing commands** - Usually indicates:
   - Clip operations not being applied
   - Effect layers not being created
   - Save/restore imbalance

4. **Rect/position differences** - Usually indicates:
   - Coordinate space transformation issues
   - Bounds calculation differences

### Debugging Workflow

1. Run the diff tool to identify mismatches
2. Find the first significant mismatch
3. Look at the paint operation that generated it in `raw_paint_ops.json`
4. Check the property tree IDs (transform_id, effect_id, clip_id)
5. Compare how test.html applies those property trees vs Chromium
6. Fix the issue in test.html
7. Re-run and verify the mismatch is resolved

## JSON Format Reference

### Command Types

| Command | Description | Key Properties |
|---------|-------------|----------------|
| `save` | Push canvas state | - |
| `restore` | Pop canvas state | - |
| `saveLayer` | Create isolated layer | `bounds`, `paint` |
| `translate` | Translate transform | `dx`, `dy` |
| `scale` | Scale transform | `sx`, `sy` |
| `rotate` | Rotation | `degrees` |
| `concat` | Concatenate 3x3 matrix | `matrix` (9 elements) |
| `concat44` | Concatenate 4x4 matrix | `matrix` (16 elements) |
| `setMatrix` | Set absolute matrix | `matrix` |
| `clipRect` | Clip to rectangle | `rect`, `clipOp`, `antiAlias` |
| `clipRRect` | Clip to rounded rect | `rrect`, `clipOp`, `antiAlias` |
| `clipPath` | Clip to path | `path`, `clipOp`, `antiAlias` |
| `drawRect` | Draw rectangle | `rect`, `paint` |
| `drawRRect` | Draw rounded rect | `rrect`, `paint` |
| `drawDRRect` | Draw donut shape | `outer`, `inner`, `paint` |
| `drawPath` | Draw SVG path | `path`, `paint` |
| `drawLine` | Draw line | `x0`, `y0`, `x1`, `y1`, `paint` |
| `drawTextBlob` | Draw text | `x`, `y`, `paint`, `runs` |
| `drawColor` | Fill with color | `color`, `blendMode` |
| `clear` | Clear canvas | `color` |

### Paint Properties

```json
{
  "color": {"r": 0.0-1.0, "g": 0.0-1.0, "b": 0.0-1.0, "a": 0.0-1.0},
  "style": 0,        // 0=Fill, 1=Stroke, 2=StrokeAndFill
  "strokeWidth": 1.0,
  "strokeCap": 0,    // 0=Butt, 1=Round, 2=Square
  "strokeJoin": 0,   // 0=Miter, 1=Round, 2=Bevel
  "antiAlias": true,
  "blendMode": 3,    // Skia blend mode enum
  "alpha": 1.0
}
```

### ClipOp Values

- `0` = Difference (clip OUT)
- `1` = Intersect (clip IN)
