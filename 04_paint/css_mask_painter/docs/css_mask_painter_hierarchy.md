# CSSMaskPainter Class Hierarchy

## Overview

`CSSMaskPainter` is a static-only utility class in Chromium's Blink rendering engine that calculates mask bounding boxes for CSS mask operations. Unlike most painters, it does not inherit from a base painter class and serves as a computation helper rather than a direct drawing class.

## Class Structure

```
CSSMaskPainter (STATIC_ONLY)
    |
    +-- MaskBoundingBox() [static]
            |
            +-- Delegates to SVGMaskPainter for SVG children
            +-- Computes bounding box for HTML elements
```

## Related Classes

### Direct Dependencies

```
CSSMaskPainter
    |
    +-- SVGMaskPainter (for SVG mask painting)
    |       |
    |       +-- Paint()
    |       +-- PaintSVGMaskLayer()
    |       +-- ResourceBoundsForSVGChild()
    |       +-- MaskIsValid()
    |
    +-- LayoutObject (target element for mask)
    |       |
    |       +-- LayoutBox
    |       +-- LayoutInline
    |       +-- LayoutSVGForeignObject
    |
    +-- ComputedStyle (mask style properties)
    |       |
    |       +-- HasMask()
    |       +-- MaskLayers()
    |       +-- HasMaskBoxImageOutsets()
    |       +-- MaskBoxImageOutsets()
    |
    +-- StyleMaskSourceImage (mask source abstraction)
            |
            +-- HasSVGMask()
            +-- GetSVGResource()
            +-- GetSVGResourceClient()
```

### Consumer Classes

```
PaintPropertyTreeBuilder
    |
    +-- Uses CSSMaskPainter::MaskBoundingBox()
        to compute MaskClip property
```

## Mask Painting Hierarchy

The actual mask painting is done by other classes. Here's the complete hierarchy:

```
BoxFragmentPainter::PaintMask()
    |
    +-- BoxPainterBase::PaintMaskImages()
            |
            +-- PaintFillLayers() [for mask layers]
            +-- NinePieceImagePainter::Paint() [for mask-box-image]

InlineBoxFragmentPainter::PaintMask()
    |
    +-- Uses similar flow

ReplacedPainter::PaintMask()
    |
    +-- ReplacedPainter::PaintMaskImages()
            |
            +-- BoxFragmentPainter::PaintMaskImages()

FieldsetPainter::PaintMask()
    |
    +-- BoxFragmentPainter::PaintMaskImages()
```

## SVG Mask Painting Flow

For SVG elements or elements with SVG mask references:

```
SVGMaskPainter::Paint()
    |
    +-- Creates ScopedPaintChunkProperties for mask effect
    +-- Creates DrawingRecorder
    +-- IterateFillLayersReversed()
            |
            +-- PaintMaskLayer() [for each layer]
                    |
                    +-- If SVG <mask> reference:
                    |       +-- SVGMaskPainter::PaintSVGMaskLayer()
                    |               |
                    |               +-- ResolveElementReference()
                    |               +-- context.Clip()
                    |               +-- context.BeginLayer() [optional, for luminance]
                    |               +-- context.ConcatCTM()
                    |               +-- context.DrawRecord()
                    |               +-- context.EndLayer()
                    |
                    +-- If regular image:
                            +-- BackgroundImageGeometry::Calculate()
                            +-- context.DrawImageTiled()
```

## File Locations

| File | Path |
|------|------|
| css_mask_painter.h | `third_party/blink/renderer/core/paint/css_mask_painter.h` |
| css_mask_painter.cc | `third_party/blink/renderer/core/paint/css_mask_painter.cc` |
| svg_mask_painter.h | `third_party/blink/renderer/core/paint/svg_mask_painter.h` |
| svg_mask_painter.cc | `third_party/blink/renderer/core/paint/svg_mask_painter.cc` |
| box_painter_base.h | `third_party/blink/renderer/core/paint/box_painter_base.h` |
| style_mask_source_image.h | `third_party/blink/renderer/core/style/style_mask_source_image.h` |

## Key Enums and Types

### EFillBox (Mask Clip Values)
```cpp
enum EFillBox {
    kNoClip,      // No clipping
    kContent,     // Content box
    kPadding,     // Padding box
    kBorder,      // Border box
    kText,        // Text
    kFillBox,     // SVG fill-box
    kStrokeBox,   // SVG stroke-box
    kViewBox      // SVG view-box
};
```

### EFillMaskMode (Mask Mode Values)
```cpp
enum EFillMaskMode {
    kMatchSource,  // Match the mask source type (alpha for images, luminance for SVG masks)
    kLuminance,    // Force luminance mode
    kAlpha         // Force alpha mode
};
```

## Design Notes

1. **Separation of Concerns**: `CSSMaskPainter` only handles bounding box computation. Actual painting is delegated to:
   - `BoxPainterBase::PaintMaskImages()` for CSS box elements
   - `SVGMaskPainter::Paint()` for SVG elements

2. **SVG Integration**: The class integrates tightly with SVG mask resources through:
   - `SVGMaskPainter::MaskIsValid()` for validation
   - `SVGMaskPainter::ResourceBoundsForSVGChild()` for SVG bounds

3. **Kludge for Invalid Masks**: The code contains a "kludge" (per spec) where non-existent `<mask>` references yield transparent black, implemented by returning `nullopt` from `MaskBoundingBox()`.
