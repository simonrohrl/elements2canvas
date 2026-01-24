# FieldsetPainter Trace Reference

## Line-by-Line Analysis of PaintBoxDecorationBackground

### Source File: `fieldset_painter.cc`

---

### Lines 75-78: Method Signature

```cpp
void FieldsetPainter::PaintBoxDecorationBackground(
    const PaintInfo& paint_info,
    const PhysicalRect& paint_rect,
    const BoxDecorationData& box_decoration_data) {
```

**Purpose**: Entry point for painting the fieldset's box decorations including background, shadows, and border.

**Parameters**:
- `paint_info`: Contains the graphics context and paint phase information
- `paint_rect`: The rectangle to paint within
- `box_decoration_data`: Decisions about what decorations to paint

---

### Line 79: Assertion

```cpp
DCHECK(box_decoration_data.ShouldPaint());
```

**Purpose**: Debug assertion ensuring we only enter this method when there's something to paint.

---

### Lines 81-84: Setup and Calculations

```cpp
const ComputedStyle& style = fieldset_.Style();
FieldsetPaintInfo fieldset_paint_info = CreateFieldsetPaintInfo();
PhysicalRect contracted_rect(paint_rect);
contracted_rect.Contract(fieldset_paint_info.border_outsets);
```

**Purpose**:
1. Get the computed style for the fieldset
2. Create paint info that includes legend cutout calculations
3. Contract the paint rectangle by the border outsets (accounts for legend extending beyond border)

**Key Insight**: The `border_outsets` shrinks the painting area when the legend is taller than the border. This ensures backgrounds and shadows are painted inside the visual border area, not behind the legend.

---

### Lines 86-93: Outer Box Shadow

```cpp
BoxFragmentPainter fragment_painter(fieldset_);
if (box_decoration_data.ShouldPaintShadow()) {
  std::optional<BorderShapeReferenceRects> border_shape_rects =
      ComputeBorderShapeReferenceRects(contracted_rect, fieldset_.Style(),
                                       *fieldset_.GetLayoutObject());
  fragment_painter.PaintNormalBoxShadow(paint_info, contracted_rect, style,
                                        border_shape_rects);
}
```

**Purpose**: Paint the outer (normal) box-shadow using the contracted rectangle.

**Delegation**: Uses `BoxFragmentPainter::PaintNormalBoxShadow()`

---

### Lines 95-111: Bleed Avoidance Clipping

```cpp
GraphicsContext& graphics_context = paint_info.context;
GraphicsContextStateSaver state_saver(graphics_context, false);
bool needs_end_layer = false;
if (BleedAvoidanceIsClipping(
        box_decoration_data.GetBackgroundBleedAvoidance())) {
  state_saver.Save();

  ContouredRect border = ContouredBorderGeometry::PixelSnappedContouredBorder(
      style, contracted_rect, fieldset_.SidesToInclude());
  graphics_context.ClipContouredRect(border);

  if (box_decoration_data.GetBackgroundBleedAvoidance() ==
      kBackgroundBleedClipLayer) {
    graphics_context.BeginLayer();
    needs_end_layer = true;
  }
}
```

**Purpose**: Handle background bleed avoidance by clipping to the border shape.

**GraphicsContext Operations**:
1. `ClipContouredRect(border)` - Clips drawing to the rounded border rectangle
2. `BeginLayer()` - Starts a compositing layer if needed for bleed avoidance

---

### Lines 113-120: Background Painting

```cpp
if (box_decoration_data.ShouldPaintBackground()) {
  // TODO(eae): Switch to LayoutNG version of BoxBackgroundPaintContext.
  BoxBackgroundPaintContext bg_paint_context(
      *static_cast<const LayoutBoxModelObject*>(fieldset_.GetLayoutObject()));
  fragment_painter.PaintFillLayers(
      paint_info, box_decoration_data.BackgroundColor(),
      style.BackgroundLayers(), contracted_rect, bg_paint_context);
}
```

**Purpose**: Paint background color and background images.

**Delegation**: Uses `BoxFragmentPainter::PaintFillLayers()`

---

### Lines 121-124: Inset Box Shadow

```cpp
if (box_decoration_data.ShouldPaintShadow()) {
  fragment_painter.PaintInsetBoxShadowWithBorderRect(
      paint_info, contracted_rect, fieldset_.Style());
}
```

**Purpose**: Paint the inner (inset) box-shadow.

**Delegation**: Uses `BoxFragmentPainter::PaintInsetBoxShadowWithBorderRect()`

---

### Lines 125-143: Border Painting with Legend Cutout

```cpp
if (box_decoration_data.ShouldPaintBorder()) {
  // Create a clipping region around the legend and paint the border as
  // normal.
  PhysicalRect legend_cutout_rect = fieldset_paint_info.legend_cutout_rect;
  legend_cutout_rect.Move(paint_rect.offset);
  graphics_context.ClipOut(ToPixelSnappedRect(legend_cutout_rect));

  const LayoutObject* layout_object = fieldset_.GetLayoutObject();
  Node* node = layout_object->GeneratingNode();
  std::optional<BorderShapeReferenceRects> border_shape_rects =
      ComputeBorderShapeReferenceRects(contracted_rect, fieldset_.Style(),
                                       *layout_object);
  fragment_painter.PaintBorder(
      *fieldset_.GetLayoutObject(), layout_object->GetDocument(), node,
      paint_info, contracted_rect, fieldset_.Style(),
      box_decoration_data.GetBackgroundBleedAvoidance(),
      fieldset_.SidesToInclude(),
      border_shape_rects ? &*border_shape_rects : nullptr);
}
```

**Purpose**: Paint the border while excluding the legend area.

**KEY OPERATION - Legend Cutout**:
```cpp
graphics_context.ClipOut(ToPixelSnappedRect(legend_cutout_rect));
```

This is the critical line that creates the characteristic fieldset appearance. `ClipOut()` excludes the legend_cutout_rect from subsequent drawing operations, so when `PaintBorder()` is called, the border is not drawn behind the legend.

**Delegation**: Uses `BoxFragmentPainter::PaintBorder()`

---

### Lines 145-147: Layer Cleanup

```cpp
if (needs_end_layer)
  graphics_context.EndLayer();
```

**Purpose**: End the compositing layer if one was started for bleed avoidance.

---

## Legend Handling Deep Dive

### How the Legend Position is Calculated

From `CreateFieldsetPaintInfo()` (lines 25-71):

1. **Find the legend**: Check if the first child `IsRenderedLegend()`

2. **Unapply relative positioning**: Per HTML spec, the border cutout should be at the legend's static position:
   ```cpp
   LogicalOffset relative_offset = ComputeRelativeOffset(
       (*legend)->Style(), writing_direction, logical_fieldset_content_size);
   LogicalOffset legend_logical_offset =
       WritingModeConverter(writing_direction, fieldset_size)
           .ToLogical(legend->Offset(), (*legend)->Size()) -
       relative_offset;
   ```

3. **Convert to physical coordinates**:
   ```cpp
   legend_border_box.offset = legend_logical_offset.ConvertToPhysical(
       writing_direction, fieldset_size, legend_border_box.size);
   ```

### FieldsetPaintInfo Calculations

From `fieldset_paint_info.cc`:

**Horizontal Writing Mode (horizontal-tb)**:
```cpp
LayoutUnit legend_size = legend_border_box.size.height;
LayoutUnit border_size = fieldset_borders.top;
LayoutUnit legend_excess_size = legend_size - border_size;
if (legend_excess_size > LayoutUnit())
  border_outsets.top = legend_excess_size / 2;
legend_cutout_rect = PhysicalRect(legend_border_box.X(), LayoutUnit(),
                                  legend_border_box.Width(),
                                  std::max(legend_size, border_size));
```

- `border_outsets.top`: Half the excess height pushes content down
- `legend_cutout_rect`: Spans from top edge, width of legend, height is max of legend or border

**Vertical Writing Modes**:
- `vertical-rl`: Uses right border, outsets right side
- `vertical-lr`: Uses left border, outsets left side

---

## GraphicsContext Draw Operations Summary

| Line | Operation | Description |
|------|-----------|-------------|
| 100 | `state_saver.Save()` | Save graphics state |
| 104 | `ClipContouredRect(border)` | Clip to rounded border for bleed avoidance |
| 108 | `BeginLayer()` | Start compositing layer |
| **130** | **`ClipOut(legend_cutout_rect)`** | **Exclude legend from border painting** |
| 146 | `EndLayer()` | End compositing layer |

---

## Visual Representation of Border Cutout

```
+----------------------------------------------------------+
|                                                          |
|   +------------------+                                   |
|   |     LEGEND       |   <- Legend sits in the border    |
+---+------------------+-----------------------------------+
|                                                          |
|                          ClipOut() creates a "hole"      |
|                          where the border isn't drawn    |
|                                                          |
|              FIELDSET CONTENT                            |
|                                                          |
|                                                          |
+----------------------------------------------------------+

The ClipOut(legend_cutout_rect) call at line 130 creates the gap
in the border where the legend appears.
```

---

## Relevant Spec Reference

From HTML Living Standard:
> "If the element has a rendered legend, then the border is expected to not be painted behind the rectangle defined as follows, using the writing mode of the fieldset: ... at its static position (ignoring transforms)..."

This is why `CreateFieldsetPaintInfo()` carefully unapplies `position:relative` offsets - the spec requires the cutout to be at the static position.
