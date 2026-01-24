# ScrollableAreaPainter Trace Reference

## GraphicsContext Draw Operations

This document catalogs all direct `GraphicsContext` draw operations in `ScrollableAreaPainter` and related scrollbar painting code.

---

## 1. DrawPlatformResizerImage (scrollable_area_painter.cc:111-173)

### Operation 1: Dark Grip Lines
```cpp
// Location: scrollable_area_painter.cc:151-158
const SkPath dark_line_path = SkPathBuilder()
    .moveTo(points[0].x(), points[0].y())
    .lineTo(points[1].x(), points[1].y())
    .moveTo(points[2].x(), points[2].y())
    .lineTo(points[3].x(), points[3].y())
    .detach();
paint_flags.setColor(SkColorSetARGB(153, 0, 0, 0));
context.DrawPath(dark_line_path, paint_flags, auto_dark_mode);
```

**Parameters:**
- `dark_line_path`: SkPath with two diagonal line segments
- `paint_flags`: Stroke style, width = `ceil(paint_scale)`
- Color: ARGB(153, 0, 0, 0) - semi-transparent black

### Operation 2: Light Grip Lines
```cpp
// Location: scrollable_area_painter.cc:164-172
const SkPath light_line_path = SkPathBuilder()
    .moveTo(points[0].x(), points[0].y() + v_offset)
    .lineTo(points[1].x() + h_offset, points[1].y())
    .moveTo(points[2].x(), points[2].y() + v_offset)
    .lineTo(points[3].x() + h_offset, points[3].y())
    .detach();
paint_flags.setColor(SkColorSetARGB(153, 255, 255, 255));
context.DrawPath(light_line_path, paint_flags, auto_dark_mode);
```

**Parameters:**
- `light_line_path`: SkPath offset from dark lines by v_offset/h_offset
- `paint_flags`: Stroke style, width = `ceil(paint_scale)`
- Color: ARGB(153, 255, 255, 255) - semi-transparent white

---

## 2. PaintResizer Frame (scrollable_area_painter.cc:75-89)

### Operation: Stroke Rect for Resizer Frame
```cpp
// Location: scrollable_area_painter.cc:75-89
if (scrollable_area_.NeedsScrollCorner()) {
  GraphicsContextStateSaver state_saver(context);
  context.Clip(visual_rect);
  gfx::Rect larger_corner = visual_rect;
  larger_corner.set_size(gfx::Size(larger_corner.width() + 1, larger_corner.height() + 1));
  context.SetStrokeColor(Color(217, 217, 217));
  context.SetStrokeThickness(1);
  gfx::RectF corner_outline(larger_corner);
  corner_outline.Inset(0.5f);
  context.StrokeRect(corner_outline, PaintAutoDarkMode(...));
}
```

**Operations:**
1. `context.Clip(visual_rect)` - Clips to resizer bounds
2. `context.SetStrokeColor(Color(217, 217, 217))` - Light grey
3. `context.SetStrokeThickness(1)` - 1px stroke
4. `context.StrokeRect(corner_outline, ...)` - Draws frame outline

---

## 3. CustomScrollbarTheme::PaintScrollCorner (custom_scrollbar_theme.cc:138-151)

### Operation: Fill Rect for Scroll Corner
```cpp
// Location: custom_scrollbar_theme.cc:147-150
DrawingRecorder recorder(context, display_item_client,
                         DisplayItem::kScrollCorner, corner_rect);
context.FillRect(corner_rect, Color::kWhite, AutoDarkMode::Disabled());
```

**Parameters:**
- `corner_rect`: gfx::Rect defining scroll corner area
- Color: `Color::kWhite`
- AutoDarkMode: Disabled

---

## 4. Display Item Recording Operations

### 4.1 DrawingRecorder Usage

#### PaintResizer
```cpp
// Location: scrollable_area_painter.cc:65-69
if (DrawingRecorder::UseCachedDrawingIfPossible(context, client, DisplayItem::kResizer))
  return;
DrawingRecorder recorder(context, client, DisplayItem::kResizer, visual_rect);
```

#### PaintScrollCorner (via theme)
```cpp
// Location: custom_scrollbar_theme.cc:143-148
if (DrawingRecorder::UseCachedDrawingIfPossible(context, display_item_client,
                                                DisplayItem::kScrollCorner))
  return;
DrawingRecorder recorder(context, display_item_client,
                         DisplayItem::kScrollCorner, corner_rect);
```

### 4.2 ScrollbarDisplayItem Recording

```cpp
// Location: scrollable_area_painter.cc:348-351
auto delegate = base::MakeRefCounted<ScrollbarLayerDelegate>(scrollbar);
ScrollbarDisplayItem::Record(context, scrollbar, type, std::move(delegate),
                             visual_rect, scroll_translation,
                             scrollbar.GetElementId(), hit_test_opaqueness);
```

**Parameters:**
- `context`: GraphicsContext
- `scrollbar`: Scrollbar reference (display item client)
- `type`: `kScrollbarHorizontal` or `kScrollbarVertical`
- `delegate`: ScrollbarLayerDelegate for compositor
- `visual_rect`: Scrollbar bounds
- `scroll_translation`: TransformPaintPropertyNode (for compositing)
- `element_id`: Element ID for layer binding
- `hit_test_opaqueness`: Hit test behavior

### 4.3 Hit Test Data Recording

#### Resizer Hit Test
```cpp
// Location: scrollable_area_painter.cc:104-108
context.GetPaintController().RecordScrollHitTestData(
    scrollable_area_.GetScrollCornerDisplayItemClient(),
    DisplayItem::kResizerScrollHitTest, nullptr, touch_rect,
    cc::HitTestOpaqueness::kMixed);
```

#### Custom Scrollbar Hit Test
```cpp
// Location: scrollable_area_painter.cc:301-304
context.GetPaintController().RecordScrollHitTestData(
    scrollbar, DisplayItem::kScrollbarHitTest, nullptr, visual_rect,
    cc::HitTestOpaqueness::kMixed);
```

---

## 5. ScopedPaintChunkProperties Usage

### 5.1 Overflow Controls Chunk
```cpp
// Location: scrollable_area_painter.cc:231-242
std::optional<ScopedPaintChunkProperties> scoped_paint_chunk_properties;
if (clip || transform) {
  PropertyTreeStateOrAlias modified_properties(
      paint_controller.CurrentPaintChunkProperties());
  if (clip)
    modified_properties.SetClip(*clip);
  if (transform)
    modified_properties.SetTransform(*transform);
  scoped_paint_chunk_properties.emplace(paint_controller, modified_properties,
                                        box, DisplayItem::kOverflowControls);
}
```

### 5.2 Scrollbar Effect Chunk
```cpp
// Location: scrollable_area_painter.cc:288-294
std::optional<ScopedPaintChunkProperties> chunk_properties;
if (const auto* effect = scrollbar.Orientation() == kHorizontalScrollbar
                             ? properties->HorizontalScrollbarEffect()
                             : properties->VerticalScrollbarEffect()) {
  chunk_properties.emplace(context.GetPaintController(), *effect, scrollbar, type);
}
```

### 5.3 Scroll Corner Effect Chunk
```cpp
// Location: scrollable_area_painter.cc:369-375
std::optional<ScopedPaintChunkProperties> chunk_properties;
const auto* properties = scrollable_area_.GetLayoutBox()->FirstFragment().PaintProperties();
if (const auto* effect = properties->ScrollCornerEffect()) {
  chunk_properties.emplace(context.GetPaintController(), *effect, client,
                           DisplayItem::kScrollCorner);
}
```

---

## 6. GraphicsContextStateSaver Usage

### Resizer Frame Drawing
```cpp
// Location: scrollable_area_painter.cc:76
GraphicsContextStateSaver state_saver(context);
```
Saves and restores context state around clip and stroke operations.

---

## Summary Table: All GraphicsContext Operations

| Method | Operation | Type | Location |
|--------|-----------|------|----------|
| `DrawPlatformResizerImage` | `DrawPath` (dark lines) | Path stroke | L158 |
| `DrawPlatformResizerImage` | `DrawPath` (light lines) | Path stroke | L172 |
| `PaintResizer` | `Clip` | Clipping | L77 |
| `PaintResizer` | `SetStrokeColor` | State | L81 |
| `PaintResizer` | `SetStrokeThickness` | State | L82 |
| `PaintResizer` | `StrokeRect` | Rect stroke | L85 |
| `CustomScrollbarTheme::PaintScrollCorner` | `FillRect` | Rect fill | L150 |

---

## Paint Phase Mapping

| Control Type | Paint Phase | Condition |
|--------------|-------------|-----------|
| Overlay + Self-painting | `kOverlayOverflowControls` | `HasSelfPaintingLayer()` |
| Overlay + Reordered | `kOverlayOverflowControls` | `NeedsReorderOverlayOverflowControls()` |
| Overlay + Non-self-painting | `kForeground` | Not self-painting |
| Non-overlay | `kBackground` | `ShouldPaintSelfBlockBackground()` |

---

## Caching Strategy

1. **DrawingRecorder** - Used for resizer and scroll corner
   - Checks `UseCachedDrawingIfPossible()` before recording
   - Records to `DisplayItem::kResizer` or `DisplayItem::kScrollCorner`

2. **ScrollbarDisplayItem** - Used for native scrollbars
   - Checks `UseCachedItemIfPossible()` before recording
   - Records to `DisplayItem::kScrollbarHorizontal` or `kScrollbarVertical`
   - Supports compositor integration via `scroll_translation`

3. **ScopedPaintChunkProperties** - Property tree isolation
   - Creates separate paint chunks for different property states
   - Enables compositor optimization and layer management
