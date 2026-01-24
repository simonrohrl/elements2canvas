# SVG Painter Hierarchy

This document describes the class hierarchy and relationships between the SVG painting classes in Chromium's Blink rendering engine.

## Overview

SVG painting in Blink follows a specialized pipeline that differs from regular HTML painting. The SVG painters handle:
- SVG coordinate transforms (user space vs. viewport)
- SVG-specific paint operations (fill, stroke, markers)
- SVG effects (masks, clip paths, filters)
- Context paints (fill/stroke inheritance from use elements)

## Class Hierarchy

```
PaintLayerPainter
    |
    +-- [calls SVG painters via Paint() virtual dispatch]
    |
    +-- SVGMaskPainter (static Paint() for mask effects)

SVGRootPainter
    |
    +-- Entry point from BoxFragmentPainter
    +-- Paints children via child->Paint()
    +-- Special handling for foreignObject

SVGContainerPainter
    |
    +-- Paints SVG container elements (<g>, <svg>, <use>, etc.)
    +-- Iterates children and dispatches to appropriate painters
    +-- Handles SVG viewport clipping

SVGShapePainter
    |
    +-- Paints SVG shape elements (<rect>, <circle>, <path>, etc.)
    +-- Handles fill, stroke, markers paint order
    +-- Uses SVGObjectPainter for paint preparation

SVGImagePainter
    |
    +-- Paints <image> elements
    +-- Handles preserveAspectRatio
    +-- Image timing and decoding

SVGForeignObjectPainter
    |
    +-- Bridges SVG and HTML painting
    +-- Delegates to PaintLayerPainter

SVGObjectPainter
    |
    +-- Utility class for fill/stroke paint preparation
    +-- Resolves context paints (contextFill, contextStroke)
    +-- Applies paint servers (gradients, patterns)

SVGModelObjectPainter
    |
    +-- Base utility class for SVG model objects
    +-- Hit test data recording
    +-- Outline painting
    +-- Cull rect optimization decisions

SVGMaskPainter
    |
    +-- Paints CSS mask layers for SVG elements
    +-- Handles both image masks and SVG mask references
    +-- Luminance and alpha mask modes
```

## Key Helper Classes

### ScopedSVGTransformState
Manages SVG transform state for painting:
- Hooks up the correct paint property transform node
- Updates context paints with accumulated transform
- Provides transformed `ContentPaintInfo()` for child painting

### ScopedSVGPaintState
Manages SVG paint state including effects:
- Applies filter, effect, and clip paint properties
- Schedules mask and clip-path painting on destruction
- Determines paint behavior (content vs. reference filter only)

### SVGDrawingRecorder
Specialized DrawingRecorder for SVG children:
- Uses `VisualRectInLocalSVGCoordinates()` for visual rect
- Only for leaf objects, not containers

## Painter Responsibilities

| Painter | Target Elements | Key Operations |
|---------|-----------------|----------------|
| SVGRootPainter | `<svg>` (root) | Coordinate space setup, child iteration |
| SVGContainerPainter | `<g>`, `<use>`, nested `<svg>` | Transform, clip, child iteration |
| SVGShapePainter | `<rect>`, `<circle>`, `<path>`, etc. | Fill, stroke, markers |
| SVGImagePainter | `<image>` | Image loading, aspect ratio |
| SVGForeignObjectPainter | `<foreignObject>` | HTML content embedding |
| SVGMaskPainter | (utility) | Mask layer composition |
| SVGObjectPainter | (utility) | Paint flag preparation |
| SVGModelObjectPainter | (utility) | Hit testing, outlines |

## Entry Points from PaintLayerPainter

SVG elements are integrated into the main paint flow through these entry points:

1. **SVG Root**: `BoxFragmentPainter` calls `SVGRootPainter::PaintReplaced()` for the `<svg>` element

2. **SVG Masks**: `PaintLayerPainter::Paint()` calls `SVGMaskPainter::Paint()` when `properties->Mask()` exists for SVG foreign objects (line 1133):
   ```cpp
   if (object.IsSVGForeignObject()) {
     SVGMaskPainter::Paint(context, object, object);
   }
   ```

3. **Child Dispatch**: Each SVG element's `Paint()` method is called via virtual dispatch, routing to the appropriate painter

## Paint Phase Handling

SVG painters primarily operate in the **Foreground** paint phase:

| Painter | Supported Phases |
|---------|------------------|
| SVGShapePainter | `PaintPhase::kForeground` only |
| SVGImagePainter | `PaintPhase::kForeground` only |
| SVGContainerPainter | `PaintPhase::kForeground` (with outline) |
| SVGForeignObjectPainter | `kForeground`, `kSelectionDragImage` |

## Coordinate Systems

SVG uses multiple coordinate systems:

1. **User Space**: The coordinate system established by viewBox and transforms
2. **Local SVG Coordinates**: Element's local coordinate system
3. **SVG Parent Coordinates**: Parent element's coordinate system
4. **Viewport Coordinates**: The SVG viewport space

Key methods:
- `VisualRectInLocalSVGCoordinates()`: Visual rect in local SVG space
- `LocalSVGTransform()`: Transform from local to parent SVG coordinates
- `LocalToSVGParentTransform()`: Full transform to SVG parent
- `TransformToPixelSnappedBorderBox()`: SVG root to pixel-aligned box
