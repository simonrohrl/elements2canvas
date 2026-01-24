# Border Painter Hierarchy

## Overview

The border painter group is responsible for rendering CSS borders on box elements. It handles:
- Solid, dashed, dotted, double, groove, ridge, inset, outset border styles
- Border radius (rounded corners)
- Per-side border painting with different colors/widths/styles
- Border-image (delegated to NinePieceImagePainter)
- Custom border-shape (CSS Borders Level 4)

## File Hierarchy

```
paint/
├── box_painter_base.cc          # Entry point: PaintBorder()
├── box_border_painter.cc/.h     # Main border painting logic
├── border_shape_painter.cc/.h   # CSS border-shape support
├── border_shape_utils.cc/.h     # Border shape geometry utilities
├── contoured_border_geometry.cc/.h  # Rounded/contoured rect computation
└── outline_painter.cc           # Uses BoxBorderPainter for outlines
```

## Class Responsibilities

### BoxPainterBase (entry point)
- `PaintBorder()`: Main entry point called from BoxFragmentPainter
- Decides whether to use:
  1. BorderShapePainter (CSS border-shape)
  2. NinePieceImagePainter (border-image)
  3. BoxBorderPainter (standard CSS borders)

### BoxBorderPainter (core logic)
- Static entry points:
  - `PaintBorder()`: 4-sided border painting
  - `PaintSingleRectOutline()`: Outline painting
  - `DrawBoxSide()`: Single side painting
  - `DrawLineWithStyle()`: Line drawing for dashed/dotted

- Internal structure:
  - `ComplexBorderInfo`: Groups edges by opacity for overdraw optimization
  - `OpacityGroup`: Sides sharing same opacity for transparency layer handling

### ContouredBorderGeometry
- Computes pixel-snapped contoured rectangles
- Handles border-radius calculations
- Creates inner and outer border rects

### BorderShapePainter
- Handles CSS border-shape property
- Computes outer/inner paths for arbitrary shapes
- Falls back to stroke-based rendering for single shape

### BorderShapeUtils
- Computes reference rectangles for border-shape
- Handles geometry-box calculations

## Paint Phase

Borders are painted during **`PaintPhase::kSelfBlockBackgroundOnly`**:

```
PaintLayerPainter::Paint()
  └─ PaintWithPhase(PaintPhase::kSelfBlockBackgroundOnly)
      └─ BoxFragmentPainter::Paint()
          └─ PaintBoxDecorationBackground()
              └─ BoxPainterBase::PaintBorder()
                  └─ BoxBorderPainter::PaintBorder()
```

## Context Passed

The following context is passed through the border painting chain:

| Parameter | Type | Description |
|-----------|------|-------------|
| context | GraphicsContext& | Skia drawing context |
| border_rect | PhysicalRect | The outer border box rectangle |
| style | ComputedStyle& | CSS computed style with border properties |
| bleed_avoidance | BackgroundBleedAvoidance | How to handle background bleed at edges |
| sides_to_include | PhysicalBoxSides | Which sides to paint (for tables, etc.) |

## Border Properties from ComputedStyle

```cpp
style.BorderTopWidth()      // Width in pixels (already computed)
style.BorderTopStyle()      // EBorderStyle enum
style.BorderTopColor()      // via VisitedDependentColor()
style.BorderTopLeftRadius() // LengthSize for corner
style.HasBorderRadius()     // Quick check for any radius
style.HasBorder()           // Quick check for any border
```
