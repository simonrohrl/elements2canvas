# MathMLPainter Call Diagram

## High-Level Call Flow

```
BoxFragmentPainter::PaintInternal()
    |
    v
[Check: is_visible && fragment.HasExtraMathMLPainting()]
    |
    v
MathMLPainter(fragment).Paint(paint_info, paint_offset)
    |
    +---> [Check DrawingRecorder cache]
    |         |
    |         +---> UseCachedDrawingIfPossible() --> [RETURN if cached]
    |
    +---> DrawingRecorder(info.context, ...)
    |
    +---> [Dispatch based on element type]
              |
              +---> IsMathMLFraction()? ---------> PaintFractionBar()
              |                                         |
              |                                         +---> PaintBar()
              |
              +---> IsRadicalOperator()? --------> PaintRadicalSymbol()
              |                                         |
              |                                         +---> PaintStretchyOrLargeOperator()
              |                                         +---> PaintBar() [for overbar]
              |
              +---> [Default: Operator] ---------> PaintOperator()
                                                        |
                                                        +---> PaintStretchyOrLargeOperator()
```

## Detailed Method Call Diagrams

### Paint() - Main Entry Point

```
Paint(info, paint_offset)
    |
    |  // Get display item client for caching
    +---> box_fragment_.GetLayoutObject()
    |
    |  // Check cache first
    +---> DrawingRecorder::UseCachedDrawingIfPossible(info.context, client, info.phase)
    |         |
    |         +---> [Return early if cached drawing available]
    |
    |  // Create recording scope
    +---> DrawingRecorder recorder(info.context, client, info.phase, visual_rect)
    |         |
    |         +---> BoxFragmentPainter(box_fragment_).VisualRect(paint_offset)
    |
    |  // Dispatch to specific painter
    +---> [if IsMathMLFraction()]
    |         |
    |         +---> PaintFractionBar(info, paint_offset)
    |         +---> return
    |
    +---> [if GetMathMLPaintInfo().IsRadicalOperator()]
    |         |
    |         +---> PaintRadicalSymbol(info, paint_offset)
    |         +---> return
    |
    +---> [default: Operator]
              |
              +---> PaintOperator(info, paint_offset)
```

### PaintFractionBar() Flow

```
PaintFractionBar(info, paint_offset)
    |
    |  // Verify writing mode
    +---> DCHECK(box_fragment_.Style().IsHorizontalWritingMode())
    |
    |  // Get styling and metrics
    +---> box_fragment_.Style()
    +---> FractionLineThickness(style)  --> [Return if 0]
    +---> MathAxisHeight(style)
    |
    |  // Calculate bar position
    +---> box_fragment_.FirstBaseline()
    |         |
    |         +---> [Return if no baseline]
    |
    +---> box_fragment_.Borders()
    +---> box_fragment_.Padding()
    |
    |  // Construct rectangle
    +---> PhysicalRect bar_rect = {
    |         borders.left + padding.left,
    |         *baseline - axis_height,
    |         width - borders.HorizontalSum() - padding.HorizontalSum(),
    |         line_thickness
    |     }
    |
    +---> bar_rect.Move(paint_offset)
    |
    |  // Paint the bar
    +---> PaintBar(info, bar_rect)
```

### PaintRadicalSymbol() Flow

```
PaintRadicalSymbol(info, paint_offset)
    |
    |  // Get base child metrics (if exists)
    +---> [if !box_fragment_.Children().empty()]
    |         |
    |         +---> base_child = box_fragment_.Children()[0]
    |         +---> base_child_width = base_child.Size().width
    |         +---> base_child_ascent = FirstBaseline() or Size().height
    |
    |  // Get paint parameters
    +---> box_fragment_.GetMathMLPaintInfo()
    +---> box_fragment_.Style()
    |
    |  // Check for index (mroot vs msqrt)
    +---> To<MathMLRadicalElement>(box_fragment_.GetNode())->HasIndex()
    +---> GetRadicalVerticalParameters(style, has_index)
    |
    |  // Calculate vertical symbol position
    +---> radical_base_ascent = base_child_ascent + margins.inline_start
    +---> block_offset = FirstBaseline() - vertical_gap - radical_base_ascent
    |
    +---> box_fragment_.Borders()
    +---> box_fragment_.Padding()
    |
    |  // Calculate inline offset
    +---> inline_offset = borders.left + padding.left + radical_operator_inline_offset
    |
    |  // Convert to physical coordinates
    +---> LogicalOffset radical_symbol_offset(inline_offset, block_offset + ascent)
    +---> radical_symbol_offset.ConvertToPhysical(...)
    |
    |  // Paint the vertical radical symbol
    +---> PaintStretchyOrLargeOperator(info, paint_offset + physical_offset)
    |
    |  // Paint horizontal overbar (if rule_thickness > 0)
    +---> [if rule_thickness]
              |
              +---> base_width = base_child_width + margins.InlineSum()
              +---> LogicalOffset bar_offset = (inline_offset, block_offset) + (operator_inline_size, 0)
              +---> bar_offset.ConvertToPhysical(...)
              +---> PhysicalRect bar_rect = {...}
              +---> bar_rect.Move(paint_offset)
              +---> PaintBar(info, bar_rect)
```

### PaintOperator() Flow

```
PaintOperator(info, paint_offset)
    |
    |  // Get style and parameters
    +---> box_fragment_.Style()
    +---> box_fragment_.GetMathMLPaintInfo()
    |
    |  // Calculate logical offset (based on operator ascent)
    +---> LogicalOffset offset(0, parameters.operator_ascent)
    |
    |  // Convert to physical coordinates
    +---> offset.ConvertToPhysical(
    |         style.GetWritingDirection(),
    |         PhysicalSize(fragment_width, fragment_height),
    |         PhysicalSize(operator_inline_size, operator_ascent + operator_descent)
    |     )
    |
    |  // Add borders and padding
    +---> box_fragment_.Borders()
    +---> box_fragment_.Padding()
    +---> physical_offset.left += borders.left + padding.left
    +---> physical_offset.top += borders.top + padding.top
    |
    |  // Adjust for RTL/LTR and size differences
    +---> [Check RuntimeEnabledFeatures::MathMLOperatorRTLMirroringEnabled()]
    +---> physical_offset.left += (width_diff / 2) * direction_multiplier
    |
    |  // Paint the operator glyph
    +---> PaintStretchyOrLargeOperator(info, paint_offset + physical_offset)
```

### PaintStretchyOrLargeOperator() Flow

```
PaintStretchyOrLargeOperator(info, paint_offset)
    |
    |  // Get style and operator parameters
    +---> box_fragment_.Style()
    +---> box_fragment_.GetMathMLPaintInfo()
    |
    |  // Prepare text fragment for rendering
    +---> operator_character = parameters.operator_character
    +---> TextFragmentPaintInfo text_fragment_paint_info = {
    |         StringView(span_from_ref(operator_character)),
    |         0, 1,
    |         parameters.operator_shape_result_view.Get()
    |     }
    |
    |  // Save graphics context state
    +---> GraphicsContextStateSaver state_saver(info.context)
    |
    |  // Set fill color
    +---> info.context.SetFillColor(style.VisitedDependentColor(GetCSSPropertyColor()))
    |
    |  // Configure dark mode
    +---> AutoDarkMode auto_dark_mode(PaintAutoDarkMode(style, kForeground))
    |
    |  // Draw the operator character
    +---> info.context.DrawText(
              *style.GetFont(),
              text_fragment_paint_info,
              gfx::PointF(paint_offset),
              kInvalidDOMNodeId,
              auto_dark_mode
          )
```

### PaintBar() Flow

```
PaintBar(info, bar_rect)
    |
    |  // Snap to pixel grid
    +---> gfx::Rect snapped_bar_rect = ToPixelSnappedRect(bar_rect)
    |
    |  // Check for empty rectangle
    +---> [if snapped_bar_rect.IsEmpty()] --> return
    |
    |  // Adjust vertical position (origin is at midpoint)
    +---> snapped_bar_rect -= gfx::Vector2d(0, snapped_bar_rect.height() / 2)
    |
    |  // Get style for color
    +---> box_fragment_.Style()
    |
    |  // Fill the rectangle
    +---> info.context.FillRect(
              snapped_bar_rect,
              style.VisitedDependentColor(GetCSSPropertyColor()),
              PaintAutoDarkMode(style, kForeground)
          )
```

## GraphicsContext Operations Summary

| Method | GraphicsContext Operation | Parameters |
|--------|---------------------------|------------|
| `PaintBar()` | `FillRect()` | snapped_bar_rect, color, dark_mode |
| `PaintStretchyOrLargeOperator()` | `SetFillColor()` | color |
| `PaintStretchyOrLargeOperator()` | `DrawText()` | font, text_info, point, node_id, dark_mode |

## State Management

| RAII Object | Scope | Purpose |
|-------------|-------|---------|
| `DrawingRecorder` | `Paint()` | Records drawing operations for display list |
| `GraphicsContextStateSaver` | `PaintStretchyOrLargeOperator()` | Saves/restores graphics state |
| `AutoDarkMode` | `PaintStretchyOrLargeOperator()` | Manages dark mode color transformation |
