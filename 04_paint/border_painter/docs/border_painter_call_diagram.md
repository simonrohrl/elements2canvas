# Border Painter Call Diagram

## Main Call Flow

```
BoxFragmentPainter::PaintBoxDecorationBackground()
    │
    ▼
BoxPainterBase::PaintBorder() [box_painter_base.cc:1466]
    │
    ├──► BorderShapePainter::Paint() [border_shape_painter.cc:108]
    │    (if style.HasBorderShape())
    │    └── Returns true if painted
    │
    ├──► NinePieceImagePainter::Paint() [nine_piece_image_painter.cc]
    │    (if border-image present)
    │    └── Returns true if painted
    │
    └──► BoxBorderPainter::PaintBorder() [box_border_painter.h:28]
         │
         ▼
     BoxBorderPainter constructor [box_border_painter.cc:1315]
         │
         ├── GetBorderEdgeInfo(edges_) - extracts 4 border edges
         ├── ComputeBorderProperties() - analyze uniformity
         ├── PixelSnappedContouredBorder() - outer contoured rect
         └── PixelSnappedContouredInnerBorder() - inner contoured rect
         │
         ▼
     BoxBorderPainter::Paint() [box_border_painter.cc:1439]
         │
         ├──► PaintBorderFastPath() [line 1255]
         │    │
         │    ├── [FAST] DrawSolidBorderRect() - uniform solid rectangular
         │    ├── [FAST] DrawBleedAdjustedDRRect() - uniform solid rounded
         │    └── [FAST] DrawDoubleBorder() - uniform double border
         │
         └──► [SLOW PATH] Complex border painting
              │
              ├── ClipContouredRect(outer_) - clip to outer border
              ├── ClipOutContouredRect(inner_) - exclude inner
              │
              └── PaintOpacityGroup() [line 1509]
                   │
                   ├── Recursively process opacity groups (high to low)
                   ├── BeginLayer() for transparency handling
                   │
                   └── PaintSide() [line 1569]
                        │
                        └── PaintOneBorderSide() [line 1696]
                             │
                             ├── [Curved] ClipBorderSidePolygon()
                             │            DrawCurvedBoxSide()
                             │
                             └── [Straight] DrawLineForBoxSide()
```

## Fast Path Decision Tree

```
PaintBorderFastPath()
    │
    ├── is_uniform_color_? ─────────── NO ──► return false (slow path)
    │         │
    │        YES
    │         │
    ├── is_uniform_style_? ─────────── NO ──► return false (slow path)
    │         │
    │        YES
    │         │
    ├── inner_.IsRenderable()? ──────── NO ──► return false (slow path)
    │         │
    │        YES
    │         │
    ├── Style is SOLID or DOUBLE? ──── NO ──► return false (slow path)
    │         │
    │        YES
    │         │
    ├── All 4 sides visible?
    │    │
    │   YES ──► DrawSolidBorderRect() or DrawDoubleBorder()
    │    │
    │   NO ───► translucent + solid + !rounded? ──► FillPath (merged rects)
    │
    └── return true (painted via fast path)
```

## Style-Specific Rendering

```
DrawCurvedBoxSide() [line 1753]
    │
    ├── [DOTTED/DASHED] DrawCurvedDashedDottedBoxSide()
    │    └── StrokePath() with styled stroke
    │
    ├── [DOUBLE] DrawCurvedDoubleBoxSide()
    │    └── FillRect() x2 with clipping
    │
    ├── [RIDGE/GROOVE] DrawCurvedRidgeGrooveBoxSide()
    │    └── FillRect() x2 with light/dark colors
    │
    └── [SOLID/INSET/OUTSET] FillRect()


DrawLineForBoxSide() [line 900]
    │
    ├── [DOTTED/DASHED] DrawDashedOrDottedBoxSide()
    │    └── DrawLineWithStyle() - stroke with pattern
    │
    ├── [DOUBLE] DrawDoubleBoxSide()
    │    └── DrawLineForBoxSide() x2 (recursive)
    │
    ├── [RIDGE/GROOVE] DrawRidgeOrGrooveBoxSide()
    │    └── DrawLineForBoxSide() x2 (inset/outset)
    │
    ├── [INSET/OUTSET] CalculateInsetOutsetColor()
    │    └── DrawSolidBoxSide()
    │
    └── [SOLID] DrawSolidBoxSide()
         └── FillRect() or FillQuad() (with miters)
```

## Corner/Miter Handling

```
PaintOneBorderSide()
    │
    ├── Determine side_type (kCurved or kStraight)
    │
    ├── [kCurved]
    │    ├── Compute miter types based on color matching
    │    ├── ClipBorderSidePolygon() - clip to side region
    │    └── DrawCurvedBoxSide()
    │
    └── [kStraight]
         ├── ComputeMiter() for each adjacent side
         ├── If miters required: ClipBorderSidePolygon()
         └── DrawLineForBoxSide() with adjacent widths
```

## Clipping Functions

```
ClipBorderSidePolygon() [line 2006]
    │
    ├── [Hyperellipse corners] ClipBorderSidePolygonCloseToEdges()
    │    └── Complex path clipping with tangent calculations
    │
    └── [Standard corners]
         ├── Compute edge_quad (4 points defining side)
         ├── Handle inner radii (may become edge_pentagon)
         └── ClipPolygon() with anti-aliasing setting
```

## Graphics Context Operations

Key drawing operations (stopping points):

| Operation | Location | Description |
|-----------|----------|-------------|
| `context.FillRect()` | DrawSolidBoxSide | Fill solid rectangle |
| `context.FillDRRect()` | DrawBleedAdjustedDRRect | Fill between two rounded rects |
| `context.StrokeRect()` | DrawSolidBorderRect | Stroke rectangle outline |
| `context.StrokePath()` | DrawCurvedDashedDottedBoxSide | Stroke curved path |
| `context.FillPath()` | Various | Fill arbitrary path |
| `context.DrawPath()` | DrawBleedAdjustedDRRect | Draw with PaintFlags |
| `context.DrawLine()` | DrawLineWithStyle | Draw styled line |
| `context.ClipPath()` | ClipPolygon | Set clipping region |
| `context.ClipContouredRect()` | ClipContouredRect | Clip to contoured rect |
| `context.BeginLayer()` | PaintOpacityGroup | Start transparency layer |
| `context.EndLayer()` | PaintOpacityGroup | End transparency layer |
