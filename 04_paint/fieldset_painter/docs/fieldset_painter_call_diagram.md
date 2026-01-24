# FieldsetPainter Call Diagram

## PaintBoxDecorationBackground Flow

This diagram shows the complete call flow when painting a fieldset's box decoration background.

```
PaintBoxDecorationBackground(paint_info, paint_rect, box_decoration_data)
│
├── 1. DCHECK(box_decoration_data.ShouldPaint())
│
├── 2. Get style: fieldset_.Style()
│
├── 3. CreateFieldsetPaintInfo()
│   │
│   ├── Find legend child (first child if IsRenderedLegend())
│   │
│   ├── Get fieldset size and borders
│   │
│   ├── If legend exists:
│   │   ├── Get legend border box
│   │   ├── Compute relative offset (unapply position:relative)
│   │   └── Calculate legend_border_box with static position
│   │
│   └── Return FieldsetPaintInfo(style, size, borders, legend_border_box)
│
├── 4. Contract paint_rect by border_outsets
│       contracted_rect.Contract(fieldset_paint_info.border_outsets)
│
├── 5. Create BoxFragmentPainter(fieldset_)
│
├── 6. If ShouldPaintShadow():
│   │
│   ├── ComputeBorderShapeReferenceRects(contracted_rect, style, layout_object)
│   │
│   └── fragment_painter.PaintNormalBoxShadow(paint_info, contracted_rect,
│                                              style, border_shape_rects)
│
├── 7. GraphicsContextStateSaver state_saver(graphics_context, false)
│
├── 8. If BleedAvoidanceIsClipping():
│   │
│   ├── state_saver.Save()
│   │
│   ├── Create contoured border:
│   │   ContouredBorderGeometry::PixelSnappedContouredBorder(
│   │       style, contracted_rect, fieldset_.SidesToInclude())
│   │
│   ├── graphics_context.ClipContouredRect(border)
│   │
│   └── If kBackgroundBleedClipLayer:
│       ├── graphics_context.BeginLayer()
│       └── needs_end_layer = true
│
├── 9. If ShouldPaintBackground():
│   │
│   ├── Create BoxBackgroundPaintContext
│   │
│   └── fragment_painter.PaintFillLayers(paint_info,
│                                         BackgroundColor(),
│                                         style.BackgroundLayers(),
│                                         contracted_rect,
│                                         bg_paint_context)
│
├── 10. If ShouldPaintShadow():
│   │
│   └── fragment_painter.PaintInsetBoxShadowWithBorderRect(paint_info,
│                                                           contracted_rect,
│                                                           fieldset_.Style())
│
├── 11. If ShouldPaintBorder():
│   │
│   ├── *** LEGEND CUTOUT - Key Step ***
│   │   legend_cutout_rect = fieldset_paint_info.legend_cutout_rect
│   │   legend_cutout_rect.Move(paint_rect.offset)
│   │
│   ├── graphics_context.ClipOut(ToPixelSnappedRect(legend_cutout_rect))
│   │
│   ├── Get layout_object and generating node
│   │
│   ├── ComputeBorderShapeReferenceRects(contracted_rect, style, layout_object)
│   │
│   └── fragment_painter.PaintBorder(layout_object, document, node,
│                                     paint_info, contracted_rect, style,
│                                     GetBackgroundBleedAvoidance(),
│                                     fieldset_.SidesToInclude(),
│                                     border_shape_rects)
│
└── 12. If needs_end_layer:
        graphics_context.EndLayer()
```

## PaintMask Flow

```
PaintMask(paint_info, paint_offset)
│
├── 1. Get layout_object from fieldset_.GetLayoutObject()
│
├── 2. Create BoxFragmentPainter(fieldset_)
│
├── 3. Create DrawingRecorder(paint_info.context, layout_object,
│                              paint_info.phase, VisualRect)
│
├── 4. Create paint_rect(paint_offset, fieldset_.Size())
│
├── 5. Contract paint_rect by CreateFieldsetPaintInfo().border_outsets
│
├── 6. Create BoxBackgroundPaintContext
│
└── 7. ng_box_painter.PaintMaskImages(paint_info, paint_rect, layout_object,
                                       bg_paint_context,
                                       fieldset_.SidesToInclude())
```

## CreateFieldsetPaintInfo Flow

```
CreateFieldsetPaintInfo()
│
├── 1. Initialize legend = nullptr
│
├── 2. If !fieldset_.Children().empty():
│   │
│   └── If first_child->IsRenderedLegend():
│       └── legend = &first_child
│
├── 3. Get fieldset_size, fragment, fieldset_borders
│
├── 4. Get ComputedStyle from fieldset_.Style()
│
├── 5. If legend exists:
│   │
│   ├── legend_border_box.size = (*legend)->Size()
│   │
│   ├── Get writing_direction from style
│   │
│   ├── Calculate logical_fieldset_content_size
│   │   (fieldset_size - borders - padding)
│   │
│   ├── ComputeRelativeOffset for legend
│   │   (to unapply position:relative)
│   │
│   ├── Calculate legend_logical_offset using WritingModeConverter
│   │
│   └── Convert back to physical coordinates
│
└── 6. Return FieldsetPaintInfo(style, fieldset_size,
                                 fieldset_borders, legend_border_box)
```

## FieldsetPaintInfo Constructor Flow

```
FieldsetPaintInfo(fieldset_style, fieldset_size, fieldset_borders, legend_border_box)
│
├── If IsHorizontalWritingMode(): [horizontal-tb]
│   │
│   ├── legend_size = legend_border_box.size.height
│   ├── border_size = fieldset_borders.top
│   ├── legend_excess_size = legend_size - border_size
│   │
│   ├── If legend_excess_size > 0:
│   │   └── border_outsets.top = legend_excess_size / 2
│   │
│   └── legend_cutout_rect = PhysicalRect(
│           legend_border_box.X(),
│           0,
│           legend_border_box.Width(),
│           max(legend_size, border_size))
│
└── Else: [vertical writing modes]
    │
    ├── legend_size = legend_border_box.Width()
    │
    ├── If IsFlippedBlocksWritingMode(): [vertical-rl]
    │   │
    │   ├── border_size = fieldset_borders.right
    │   └── If legend_excess_size > 0:
    │       └── border_outsets.right = legend_excess_size / 2
    │
    ├── Else: [vertical-lr]
    │   │
    │   ├── border_size = fieldset_borders.left
    │   └── If legend_excess_size > 0:
    │       └── border_outsets.left = legend_excess_size / 2
    │
    ├── legend_total_block_size = max(legend_size, border_size)
    │
    ├── legend_cutout_rect = PhysicalRect(
    │       0,
    │       legend_border_box.offset.top,
    │       legend_total_block_size,
    │       legend_border_box.size.height)
    │
    └── If IsFlippedBlocksWritingMode(): [vertical-rl]
        └── Offset cutout to right edge:
            clip_x = fieldset_size.width - legend_total_block_size
            legend_cutout_rect.offset.left += clip_x
```

## GraphicsContext Operations Summary

| Operation | Method | Purpose |
|-----------|--------|---------|
| Save state | `GraphicsContextStateSaver` | Save graphics state for restoration |
| Clip to border | `ClipContouredRect(border)` | Clip to rounded border shape |
| Begin layer | `BeginLayer()` | Start compositing layer for bleed avoidance |
| **Clip out legend** | `ClipOut(legend_cutout_rect)` | **Exclude legend area from border painting** |
| End layer | `EndLayer()` | End compositing layer |
