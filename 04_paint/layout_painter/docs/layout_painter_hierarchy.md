# Layout Painter Hierarchy

## Overview

The `layout_painter` group is responsible for painting the layout tree in Chromium's Blink rendering engine. The central class is `PaintLayerPainter`, which orchestrates the painting of self-painting `PaintLayer` objects.

## Class Hierarchy

```
PaintLayerPainter (top-level orchestrator)
|
+-- FramePainter (frame-level entry point)
|   |
|   +-- PaintLayerPainter (for root layer)
|
+-- ObjectPainter (generic LayoutObject painting utilities)
|   |
|   +-- PaintOutline()
|   +-- PaintInlineChildrenOutlines()
|   +-- PaintAllPhasesAtomically()
|   +-- RecordHitTestData()
|
+-- ViewPainter (LayoutView background painting)
|   |
|   +-- PaintBoxDecorationBackground()
|   +-- PaintRootGroup()
|   +-- PaintRootElementGroup()
|
+-- FragmentPainter (NG fragment outline/URL painting)
|   |
|   +-- PaintOutline()
|   +-- AddURLRectIfNeeded()
|
+-- FrameSetPainter (HTML frameset painting)
    |
    +-- PaintObject()
    +-- PaintChildren()
    +-- PaintBorders()
```

## Key Classes

### PaintLayerPainter

**File:** `paint_layer_painter.cc/h`
**Role:** Main orchestrator for painting self-painting PaintLayers

This is the top-level entry point for the paint system. It:
- Manages paint phases (background, foreground, outline, mask)
- Traverses the paint layer tree in stacking order
- Handles subsequence caching for performance
- Records hit test data

### FramePainter

**File:** `frame_painter.cc/h`
**Role:** Frame-level paint entry point

Entry point for painting a LocalFrameView. It:
- Checks if rendering should be throttled
- Verifies layout is up to date
- Delegates to PaintLayerPainter for the root layer

### ObjectPainter

**File:** `object_painter.cc/h`
**Role:** Generic LayoutObject painting utilities

Provides common painting functionality:
- `PaintOutline()` - Paints element outlines
- `PaintInlineChildrenOutlines()` - Paints outlines for inline children
- `PaintAllPhasesAtomically()` - Paints all phases for atomic inline-level elements
- `RecordHitTestData()` - Records hit test data for compositing

### ViewPainter

**File:** `view_painter.cc/h`
**Role:** LayoutView background painting

Handles the special case of painting the viewport/document background:
- `PaintBoxDecorationBackground()` - Main entry for view background
- `PaintRootGroup()` - Paints the root backdrop (base background color)
- `PaintRootElementGroup()` - Paints the root element's background

### FragmentPainter

**File:** `fragment_painter.cc/h`
**Role:** LayoutNG fragment painting utilities

NG version of ObjectPainter for PhysicalBoxFragment:
- `PaintOutline()` - Paints fragment outlines
- `AddURLRectIfNeeded()` - Records URL rects for printing

### FrameSetPainter

**File:** `frame_set_painter.cc/h`
**Role:** HTML frameset painting

Handles painting of `<frameset>` elements:
- `PaintObject()` - Main paint entry point
- `PaintChildren()` - Paints child frames
- `PaintBorders()` - Paints frameset borders

## Related Classes (Not in Group)

These classes are called by the layout painter group but belong to other groups:

- **BoxFragmentPainter** - Paints NG box fragments (box_painter group)
- **InlineBoxFragmentPainter** - Paints inline boxes (inline_painter group)
- **ScrollableAreaPainter** - Paints scrollbars (scrollable_painter group)
- **OutlinePainter** - Actual outline rendering (decoration_painter group)

## Self-Painting Layers

A key concept in this hierarchy is "self-painting layers":

- **Self-painting layer**: A PaintLayer that paints through PaintLayerPainter
- **Non-self-painting layer**: A PaintLayer painted as normal children via BoxFragmentPainter

The `IsSelfPaintingLayer()` flag on PaintLayer determines which path is taken.

## Stacking Context Handling

The paint layer hierarchy interacts closely with stacking contexts:

- `PaintLayerStackingNode` maintains z-order lists (pos/neg)
- `PaintLayerPaintOrderIterator` iterates children in paint order
- Paint order: NegativeZOrderChildren -> NormalFlowChildren -> PositiveZOrderChildren

## Integration Points

1. **Layout to Paint**: LayoutObject::Paint() is the entry for legacy painting
2. **NG to Paint**: BoxFragmentPainter::Paint() is the entry for NG fragments
3. **Layer to Paint**: PaintLayerPainter::Paint() handles layer tree traversal
4. **Frame to Paint**: FramePainter::Paint() is the document-level entry
