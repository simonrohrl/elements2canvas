# Block Painter Hierarchy

This document describes the class hierarchy and relationships between the painters in the block_painter group.

## Class Hierarchy

```
BoxPainterBase (base class - shared utility, not in this group)
    |
    +-- BoxFragmentPainter : BoxPainterBase
    |       Main entry point for painting NG box fragments
    |       Handles block boxes, inline formatting contexts, tables
    |
    +-- BoxModelObjectPainter : BoxPainterBase
    |       Legacy painter for LayoutBoxModelObject
    |       Provides background/border painting helpers
    |
    +-- InlineBoxFragmentPainterBase (not inheriting BoxPainterBase directly)
            |
            +-- InlineBoxFragmentPainter
            |       Paints inline box fragments (span, etc.)
            |
            +-- LineBoxFragmentPainter
                    Paints ::first-line backgrounds

BoxPainter (standalone utility class)
    Provides scroll hit test recording
    Tracks region capture data
```

## File Responsibilities

### box_fragment_painter.cc/h
- **Primary Role**: Main entry point for painting PhysicalBoxFragment objects
- **Key Methods**:
  - `Paint()`: Entry point called by PaintLayerPainter
  - `PaintObject()`: Routes to specific painting based on phase
  - `PaintBoxDecorationBackground()`: Paints backgrounds, borders, shadows
  - `PaintLineBoxes()`: Handles inline formatting context
  - `PaintBlockChildren()`: Recursively paints child block boxes
- **Inheritance**: Extends `BoxPainterBase`

### box_painter.cc/h
- **Primary Role**: Utility class for LayoutBox painting
- **Key Methods**:
  - `RecordScrollHitTestData()`: Records scroll regions for hit testing
  - `RecordTrackedElementAndRegionCaptureData()`: Tracks element positions
  - `VisualRect()`: Computes visual overflow rect
- **Inheritance**: Standalone class (not a painter in the traditional sense)

### box_model_object_painter.cc/h
- **Primary Role**: Base functionality for LayoutBoxModelObject painting
- **Key Methods**:
  - `AdjustRectForScrolledContent()`: Adjusts paint rect for scrolled content
  - `GetFillLayerInfo()`: Analyzes fill layers for background painting
- **Inheritance**: Extends `BoxPainterBase`

### inline_box_fragment_painter.cc/h
- **Primary Role**: Paints inline box fragments (elements with `display: inline`)
- **Key Methods**:
  - `Paint()`: Entry point for inline box painting
  - `PaintBackgroundBorderShadow()`: Paints decorations for inline boxes
  - `PaintMask()`: Paints CSS mask for inline boxes
  - `PaintAllFragments()`: Paints all fragments for self-painting LayoutInline
- **Inheritance**: Extends `InlineBoxFragmentPainterBase`

## Paint Phase Triggers

The block painters are triggered by `PaintLayerPainter` through specific paint phases:

| Phase | Description | Primary Handler |
|-------|-------------|-----------------|
| `kSelfBlockBackgroundOnly` | Background of the element itself | `BoxFragmentPainter::PaintBoxDecorationBackground` |
| `kDescendantBlockBackgroundsOnly` | Backgrounds of descendant blocks | `BoxFragmentPainter::PaintBlockChildren` |
| `kForeground` | Content, text, inline boxes | `BoxFragmentPainter::PaintLineBoxes` |
| `kSelfOutlineOnly` | Outline of the element | `FragmentPainter::PaintOutline` |
| `kFloat` | Floating elements | `BoxFragmentPainter::PaintFloats` |
| `kMask` | CSS masks | `BoxFragmentPainter::PaintMask` |

## Key Relationships

1. **PaintLayerPainter -> BoxFragmentPainter**
   - Called via `PaintFragmentWithPhase()` at line 1308-1309
   - Passes `PaintInfo` with phase, cull rect, and context

2. **BoxFragmentPainter -> BoxPainterBase**
   - Inherits shadow/border/background painting methods
   - Uses `PaintNormalBoxShadow()`, `PaintInsetBoxShadow()`, `PaintBorder()`

3. **BoxFragmentPainter -> InlineBoxFragmentPainter**
   - Called for inline box children via `PaintBoxItem()` at line 2114
   - Used for `display: inline` elements

4. **BoxFragmentPainter -> BoxPainter**
   - Uses `RecordScrollHitTestData()` for scroll containers
   - Records hit test regions for compositor

## Context Passed to Painters

| Context | Type | Purpose |
|---------|------|---------|
| `PaintInfo` | struct | Phase, cull rect, graphics context |
| `PhysicalOffset` | value | Paint offset from layer origin |
| `PhysicalBoxFragment` | reference | The fragment being painted |
| `InlinePaintContext` | pointer | Context for inline painting (line boxes) |
