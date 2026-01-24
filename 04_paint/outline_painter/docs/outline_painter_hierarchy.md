# OutlinePainter Class Hierarchy

## Overview

The `OutlinePainter` is a static-only utility class in Chromium's Blink rendering engine responsible for painting CSS outlines around elements. Unlike borders, outlines do not take up space in the layout and are drawn outside the element's border box.

## Class Structure

```
OutlinePainter (STATIC_ONLY)
    |
    +-- PaintOutlineRects()      [Main entry point]
    +-- PaintFocusRingPath()     [Focus ring painting for paths]
    +-- OutlineOutsetExtent()    [Calculate outline extent]
    +-- IterateRightAnglePathForTesting()  [Testing utility]
```

## Internal Helper Classes

### ComplexOutlinePainter (anonymous namespace)

A stack-allocated helper class for painting complex multi-rect outlines:

```
ComplexOutlinePainter (STACK_ALLOCATED)
    |
    +-- Paint()                      [Main painting method]
    +-- PaintDoubleOutline()         [EBorderStyle::kDouble]
    +-- PaintDottedOrDashedOutline() [EBorderStyle::kDotted/kDashed]
    +-- PaintGrooveOrRidgeOutline()  [EBorderStyle::kGroove/kRidge]
    +-- PaintInsetOrOutsetOutline()  [EBorderStyle::kInset/kOutset]
    +-- PaintTopLeftOrBottomRight()  [Edge-specific painting]
    +-- PaintStraightEdge()          [Individual edge painting]
    |
    +-- CenterPath()                 [Compute center stroke path]
    +-- ComputeRadii()               [Border radius computation]
    +-- MiterClipPath()              [Corner clipping]
    +-- MakeClipOutPath()            [Inverse clipping]
```

### RoundedEdgePathIterator (anonymous namespace)

Iterates over rounded outline paths, returning edge segments:

```
RoundedEdgePathIterator (STACK_ALLOCATED)
    |
    +-- Next()                       [Get next edge stroke path]
    +-- GenerateEdgeStrokePath()     [Create path for single edge]
```

## Related Classes

### BoxBorderPainter

Used for single-rect outlines via `PaintSingleRectOutline()`:

```
BoxBorderPainter
    |
    +-- PaintSingleRectOutline()     [Delegated from OutlinePainter]
    +-- DrawLineWithStyle()          [Line drawing for dotted/dashed]
```

### Dependencies

| Class | Purpose |
|-------|---------|
| `GraphicsContext` | Drawing operations (fill, stroke, clip) |
| `ComputedStyle` | CSS style properties |
| `ContouredBorderGeometry` | Border radius calculations |
| `StyledStrokeData` | Stroke style configuration |
| `DrawingRecorder` | Display list recording |
| `Path` | Skia path wrapper |

## Paint Phase Integration

OutlinePainter is invoked during specific paint phases:

```
PaintLayerPainter::Paint()
    |
    +-- [PaintPhase::kSelfOutlineOnly]
    |       |
    |       +-- PaintWithPhase()
    |               |
    |               +-- BoxFragmentPainter::Paint()
    |                       |
    |                       +-- OutlinePainter::PaintOutlineRects()
    |
    +-- [PaintPhase::kDescendantOutlinesOnly]
            |
            +-- Paints outlines of descendant elements
```

## Outline vs Border Distinction

| Aspect | Border | Outline |
|--------|--------|---------|
| Layout space | Takes up space | Does not affect layout |
| Position | Inside border-box | Outside border-box (can be offset) |
| Sides | Can be different per side | Same on all sides |
| Corners | Follows border-radius | Follows border-radius + offset |
| Style | Per-side styling | Uniform styling |
| Painting | During background phase | During outline phase |
| Z-order | Part of element | Painted after element |
