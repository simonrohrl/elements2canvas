# Gap Decorations Painter Call Diagram

## Entry Point from BoxFragmentPainter

```
BoxFragmentPainter::PaintBoxDecorationBackground()
|
+-- [Check: GetGapGeometry() exists && !ShouldSkipGapDecorations() && CSSGapDecorationEnabled]
|
+-- PaintGapDecorations(paint_info, paint_offset, background_client, contents_paint_state)
    |
    +-- Get gap_geometry from box_fragment_.GetGapGeometry()
    |
    +-- [Handle overflow:hidden case - create ScopedBoxContentsPaintState if needed]
    |
    +-- [Calculate paint_rect and visual_rect based on scroll container status]
    |
    +-- [Check DrawingRecorder cache: DisplayItem::kColumnRules]
    |
    +-- DrawingRecorder(context, background_client, DisplayItem::kColumnRules, visual_rect)
    |
    +-- [Read gap-rule-overlap from style]
    |
    +-- [If kColumnOverRow:]
    |   +-- GapDecorationsPainter(box_fragment_).Paint(kForRows, ...)
    |   +-- GapDecorationsPainter(box_fragment_).Paint(kForColumns, ...)
    |
    +-- [Else (default: kRowOverColumn):]
        +-- GapDecorationsPainter(box_fragment_).Paint(kForColumns, ...)
        +-- GapDecorationsPainter(box_fragment_).Paint(kForRows, ...)
```

## GapDecorationsPainter::Paint() Method

```
GapDecorationsPainter::Paint(track_direction, paint_info, paint_rect, gap_geometry)
|
+-- Get style from box_fragment_.Style()
|
+-- Determine is_column_gap = (track_direction == kForColumns)
|
+-- Read CSS gap decoration properties:
|   +-- rule_colors (ColumnRuleColor or RowRuleColor)
|   +-- rule_styles (ColumnRuleStyle or RowRuleStyle)
|   +-- rule_widths (ColumnRuleWidth or RowRuleWidth)
|   +-- rule_break (resolved via CSSGapDecorationUtils)
|   +-- rule_visibility (ColumnRuleVisibilityItems or RowRuleVisibilityItems)
|
+-- Create WritingModeConverter(style.GetWritingDirection(), box_fragment_.Size())
|
+-- Create AutoDarkMode for PaintAutoDarkMode
|
+-- Get box_side from CSSGapDecorationUtils::BoxSideFromDirection()
|
+-- Calculate cross_gutter_width (perpendicular gap size)
|
+-- Determine gap_count from main or cross gaps
|
+-- Create iterators for width, style, color:
|   +-- GapDataListIterator<int> width_iterator
|   +-- GapDataListIterator<EBorderStyle> style_iterator
|   +-- GapDataListIterator<StyleColor> color_iterator
|
+-- FOR EACH gap_index in 0..gap_count:
    |
    +-- [Skip if multicol spanner gap]
    |
    +-- Get rule_color, rule_style, rule_thickness from iterators
    |
    +-- Resolve rule_color via style.VisitedDependentGapColor()
    |
    +-- Collapse rule_style via ComputedStyle::CollapsedBorderStyle()
    |
    +-- Get gap center offset from gap_geometry.GetGapCenterOffset()
    |
    +-- Generate intersections list from gap_geometry.GenerateIntersectionListForGap()
    |
    +-- Initialize start = 0, iterate through intersection pairs:
        |
        +-- WHILE start < last_intersection_index:
            |
            +-- AdjustIntersectionIndexPair(track_direction, start, end, ...)
            |   |
            |   +-- [Advance start based on blocked status and visibility]
            |   +-- ShouldMoveIntersectionStartForward()
            |   |   +-- Check rule_break (kNone returns false)
            |   |   +-- GetIntersectionBlockedStatus()
            |   |   +-- IsRuleSegmentVisible()
            |   |
            |   +-- [Advance end based on break rules and visibility]
            |   +-- ShouldMoveIntersectionEndForward()
            |       +-- IsRuleSegmentVisible()
            |       +-- GetIntersectionBlockedStatus()
            |       +-- [For kSpanningItem: check blocked after]
            |       +-- [For kIntersection: check cross-direction blocking]
            |
            +-- [Break if start >= end (no segment to paint)]
            |
            +-- Calculate crossing gap widths:
            |   +-- start_width (0 for edge/multicol, else cross_gutter_width)
            |   +-- end_width (0 for edge/multicol, else cross_gutter_width)
            |
            +-- Compute insets from gap_geometry:
            |   +-- start_inset = ComputeInsetStart()
            |   +-- end_inset = ComputeInsetEnd()
            |
            +-- Calculate decoration offsets:
            |   +-- decoration_start_offset = (start_width / 2) + start_inset
            |   +-- decoration_end_offset = (end_width / 2) + end_inset
            |
            +-- Calculate primary axis values:
            |   +-- primary_start = center - (rule_thickness / 2)
            |   +-- primary_size = rule_thickness
            |
            +-- Calculate secondary axis values:
            |   +-- secondary_start = intersections[start] + decoration_start_offset
            |   +-- secondary_size = intersections[end] - secondary_start - decoration_end_offset
            |
            +-- Compute inline/block coordinates:
            |   +-- [If column gap: primary is inline, secondary is block]
            |   +-- [If row gap: primary is block, secondary is inline]
            |
            +-- Create LogicalRect(inline_start, block_start, inline_size, block_size)
            |
            +-- Convert to PhysicalRect via converter.ToPhysical()
            |
            +-- Add paint_rect.offset
            |
            +-- BoxBorderPainter::DrawBoxSide(
            |       paint_info.context,
            |       ToPixelSnappedRect(gap_rect),
            |       box_side,
            |       resolved_rule_color,
            |       rule_style,
            |       auto_dark_mode)
            |
            +-- start = end [move to next segment]
```

## IsRuleSegmentVisible() Helper

```
IsRuleSegmentVisible(track_direction, gap_index, secondary_index, rule_visibility, gap_geometry)
|
+-- Get gap_state = gap_geometry.GetIntersectionGapSegmentState()
|
+-- SWITCH rule_visibility:
    |
    +-- kAll: return true (always visible)
    |
    +-- kNone: return false (never visible)
    |
    +-- kAround: return !gap_state.IsEmpty()
    |   [Visible if either side is occupied]
    |
    +-- kBetween: return gap_state.status_ == GapSegmentState::kNone
        [Visible only if both sides occupied]
```

## ShouldMoveIntersectionStartForward() Helper

```
ShouldMoveIntersectionStartForward(track_direction, gap_index, start_index, ...)
|
+-- [If rule_break == kNone: return false]
|
+-- Get blocked_status = gap_geometry.GetIntersectionBlockedStatus()
|
+-- [If blocked after OR not visible:]
|   return true (advance start)
|
+-- return false
```

## ShouldMoveIntersectionEndForward() Helper

```
ShouldMoveIntersectionEndForward(track_direction, gap_index, end_index, ...)
|
+-- [If not visible: return false (don't extend)]
|
+-- [If rule_break == kNone: return true (extend)]
|
+-- Get blocked_status = gap_geometry.GetIntersectionBlockedStatus()
|
+-- [If rule_break == kSpanningItem:]
|   return !blocked_after (extend if not blocked)
|
+-- [If rule_break == kIntersection:]
    |
    +-- [For flex: return false (break at each intersection)]
    |
    +-- [If blocked after: return false]
    |
    +-- Get cross_direction blocking
    |
    +-- return (blocked_before AND blocked_after in cross direction)
        [Extend only if cross intersection is flanked by spanners]
```

## BoxBorderPainter::DrawBoxSide() (External)

```
BoxBorderPainter::DrawBoxSide(context, snapped_edge_rect, side, color, style, auto_dark_mode)
|
+-- [Early return if style == kNone or kHidden]
|
+-- DrawLineForBoxSide(
        context,
        rect.x(), rect.y(),
        rect.right(), rect.bottom(),
        side, color, style,
        0, 0,  // adjacent widths
        auto_dark_mode)
```

## Legend

```
[condition]     - Conditional check
+--             - Call or step
...             - Parameters passed
SWITCH          - Switch statement
FOR EACH        - Loop iteration
WHILE           - While loop
```
