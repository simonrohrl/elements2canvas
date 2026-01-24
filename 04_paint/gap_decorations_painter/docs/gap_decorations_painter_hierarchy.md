# Gap Decorations Painter Hierarchy

## Overview

The `GapDecorationsPainter` is responsible for painting decorations within gaps in CSS Grid, Flexbox, and MultiColumn layouts as described by the CSS Gap Decorations spec (https://www.w3.org/TR/css-gaps-1/).

Gap decorations are visual rules (lines) painted in the gaps between grid/flex items, similar to the column-rule property in multi-column layouts but extended to support both row and column gaps.

## Class Hierarchy

```
BoxFragmentPainter (caller)
|
+-- GapDecorationsPainter (gap decoration painting)
    |
    +-- GapGeometry (gap layout information provider)
    |   |
    |   +-- GetMainGaps() / GetCrossGaps()
    |   +-- GenerateIntersectionListForGap()
    |   +-- GetGapCenterOffset()
    |   +-- GetIntersectionBlockedStatus()
    |   +-- GetIntersectionGapSegmentState()
    |   +-- ComputeInsetStart() / ComputeInsetEnd()
    |   +-- IsEdgeIntersection()
    |
    +-- BoxBorderPainter::DrawBoxSide() (actual rendering)
    |
    +-- WritingModeConverter (logical to physical conversion)
```

## Key Classes

### GapDecorationsPainter

**File:** `gap_decorations_painter.cc/h`
**Role:** Main painter for CSS gap decorations in Grid/Flex/MultiColumn

This class:
- Iterates through gaps in a given track direction (rows or columns)
- Determines which gap segments are visible based on rule-visibility
- Handles rule-break behavior at intersections
- Computes gap decoration geometry (position, size, insets)
- Delegates actual drawing to BoxBorderPainter

### GapGeometry

**File:** `layout/gap/gap_geometry.h`
**Role:** Provides geometric information about gaps in layout containers

GapGeometry encapsulates:
- Gap positions and sizes for main and cross directions
- Intersection points where row and column gaps meet
- Blocked status information for spanners
- Edge detection for container boundaries
- Inset computations for decoration endpoints

### BoxBorderPainter

**File:** `box_border_painter.cc/h`
**Role:** Low-level drawing of box sides/borders

The static method `DrawBoxSide()` is used to render the actual gap decoration line with:
- Position and dimensions (snapped to pixels)
- Border style (solid, dashed, dotted, etc.)
- Color (with dark mode support)

## CSS Properties Supported

| Property | Description |
|----------|-------------|
| `column-rule-color` | Color of column gap decorations |
| `column-rule-style` | Style of column gap decorations (solid, dashed, etc.) |
| `column-rule-width` | Width/thickness of column gap decorations |
| `row-rule-color` | Color of row gap decorations |
| `row-rule-style` | Style of row gap decorations |
| `row-rule-width` | Width/thickness of row gap decorations |
| `column-rule-inset` | Inset from intersection points for column rules |
| `row-rule-inset` | Inset from intersection points for row rules |
| `column-rule-visibility` | Visibility based on cell occupancy (all, none, around, between) |
| `row-rule-visibility` | Visibility based on cell occupancy |
| `column-rule-break` | Break behavior at intersections (none, spanning-item, intersection) |
| `row-rule-break` | Break behavior at intersections |
| `gap-rule-overlap` | Paint order (column-over-row or row-over-column) |

## Container Types

GapDecorationsPainter supports three container types:

1. **Grid Containers**
   - Both row and column gaps
   - Complex intersection handling with spanning items
   - Full rule-break support (spanning-item, intersection)

2. **Flex Containers**
   - Main axis gaps (between flex items)
   - Cross axis gaps (between flex lines in wrap mode)
   - Simpler intersection handling (no spanners)

3. **MultiColumn Containers**
   - Column gaps between columns
   - No cross gaps (row rules not applicable)
   - Special handling for spanning elements

## Rule Visibility Modes

The `RuleVisibilityItems` enum controls when gap segments are painted:

| Mode | Description |
|------|-------------|
| `kAll` | Paint all gap segments |
| `kNone` | Paint no gap segments |
| `kAround` | Paint if at least one side of the segment is occupied |
| `kBetween` | Paint only if both sides of the segment are occupied |

## Rule Break Modes

The `RuleBreak` enum controls where gap decorations break:

| Mode | Description |
|------|-------------|
| `kNone` | No breaking, continuous decoration |
| `kSpanningItem` | Break at "T" intersections (spanning items) |
| `kIntersection` | Break at both "T" and cross intersections |

## Related Classes (Not in Group)

- **PhysicalBoxFragment** - Source of gap geometry and style information
- **ComputedStyle** - CSS property values for gap decorations
- **GraphicsContext** - Drawing context passed through PaintInfo
- **PaintInfo** - Paint phase and context information
- **GapDataList** - Iterator over gap-specific CSS values with repeat patterns

## Integration Points

1. **From BoxFragmentPainter**: `PaintGapDecorations()` creates `GapDecorationsPainter` and calls `Paint()`
2. **To BoxBorderPainter**: `DrawBoxSide()` performs the actual line rendering
3. **From PhysicalBoxFragment**: `GetGapGeometry()` provides layout information
4. **From ComputedStyle**: Gap decoration CSS properties are read

## Feature Flag

Gap decorations are behind the `CSSGapDecorationEnabled` runtime feature flag. The feature must be enabled for gap decorations to be painted.
