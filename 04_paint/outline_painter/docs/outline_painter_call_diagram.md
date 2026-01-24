# OutlinePainter Call Flow Diagram

## Paint Layer to Outline Painter Flow

```
PaintLayerPainter::Paint()
    |
    | [Check: is_self_painting_layer && style.HasOutline()]
    |
    +---> PaintWithPhase(PaintPhase::kSelfOutlineOnly, ...)
              |
              +---> PaintFragmentWithPhase()
                        |
                        +---> BoxFragmentPainter::Paint()
                                  |
                                  +---> PaintOutline()
                                            |
                                            +---> OutlinePainter::PaintOutlineRects()
```

## PaintOutlineRects() Flow

```
OutlinePainter::PaintOutlineRects(paint_info, client, outline_rects, info, style)
    |
    +---> [1] DrawingRecorder::UseCachedDrawingIfPossible()
    |         |
    |         +---> [Early return if cached]
    |
    +---> [2] Pixel-snap outline rects
    |         |
    |         +---> ToPixelSnappedRect() for each rect
    |         +---> Union rects into visual_rect
    |
    +---> [3] Create DrawingRecorder
    |         |
    |         +---> DrawingRecorder(context, client, phase, visual_rect)
    |
    +---> [4] Branch: style.OutlineStyleIsAuto()?
              |
              +---> YES: PaintFocusRing()
              |           |
              |           +---> GetFocusRingCornerRadii()
              |           +---> PaintSingleFocusRing() [outer ring]
              |           +---> PaintSingleFocusRing() [inner ring]
              |
              +---> NO: Is single rect?
                        |
                        +---> YES: BoxBorderPainter::PaintSingleRectOutline()
                        |
                        +---> NO: ComplexOutlinePainter::Paint()
```

## ComplexOutlinePainter::Paint() Flow

```
ComplexOutlinePainter::Paint()
    |
    +---> [1] ComputeRightAnglePath(right_angle_outer_path_)
    |         |
    |         +---> Creates SkRegion from rects with offset
    |         +---> Returns boundary path
    |
    +---> [2] Alpha layer check (non-opaque complex styles)
    |         |
    |         +---> context.BeginLayer(color.Alpha())
    |
    +---> [3] Compute outer_path and inner_path
    |         |
    |         +---> ShrinkRightAnglePath(outer, width)
    |         +---> If rounded: AddCornerRadiiToPath()
    |
    +---> [4] Setup clipping
    |         |
    |         +---> context.ClipPath(outer_path)
    |         +---> context.ClipPath(MakeClipOutPath(inner_path))
    |         +---> context.SetFillColor(color)
    |
    +---> [5] Branch by outline_style:
              |
              +---> kSolid:
              |       +---> context.FillRect(outer bounds)
              |
              +---> kDouble:
              |       +---> PaintDoubleOutline()
              |             |
              |             +---> Compute inner_third_path, outer_third_path
              |             +---> context.FillPath(inner_third)
              |             +---> context.ClipPath(clip out outer_third)
              |             +---> context.FillRect()
              |
              +---> kDotted/kDashed:
              |       +---> PaintDottedOrDashedOutline()
              |             |
              |             +---> StyledStrokeData setup
              |             +---> If rounded: StrokePath(center_path)
              |             +---> Else: IterateRightAnglePath -> PaintStraightEdge()
              |
              +---> kGroove/kRidge:
              |       +---> PaintGrooveOrRidgeOutline()
              |             |
              |             +---> PaintInsetOrOutsetOutline(center_path, is_groove)
              |             +---> context.ClipPath(center_path)
              |             +---> context.SetStrokeColor(color.Dark())
              |             +---> PaintTopLeftOrBottomRight(ridge_side)
              |             +---> context.SetStrokeColor(color)
              |             +---> PaintTopLeftOrBottomRight(groove_side)
              |
              +---> kInset/kOutset:
                      +---> PaintInsetOrOutsetOutline(center_path, is_inset)
                            |
                            +---> context.SetStrokeColor(color)
                            +---> PaintTopLeftOrBottomRight(!is_inset)
                            +---> context.SetStrokeColor(color.Dark())
                            +---> PaintTopLeftOrBottomRight(is_inset)
```

## Focus Ring Painting Flow

```
PaintFocusRing(context, rects, style, corner_radii, info)
    |
    +---> [1] Determine colors
    |         |
    |         +---> inner_color = VisitedDependentColor(OutlineColor)
    |         +---> outer_color = White (or dark for DarkColorScheme)
    |
    +---> [2] Calculate widths
    |         |
    |         +---> outer_ring_width = FocusRingOuterStrokeWidth() [2/3 of total]
    |         +---> inner_ring_width = FocusRingInnerStrokeWidth() [1/3 of total]
    |
    +---> [3] Calculate offset
    |         |
    |         +---> FocusRingOffset() considers border inset
    |
    +---> [4] Paint outer ring
    |         |
    |         +---> PaintSingleFocusRing(outer_ring_width, offset + inner_width, outer_color)
    |
    +---> [5] Paint inner ring (overlapping)
              |
              +---> PaintSingleFocusRing(outer_ring_width, offset, inner_color)

PaintSingleFocusRing(context, rects, width, offset, radii, curvature, color)
    |
    +---> ComputeRightAnglePath()
    |
    +---> Branch: path.isRect()?
              |
              +---> YES: Is round curvature?
              |           +---> YES: context.DrawFocusRingRect(SkRRect)
              |           +---> NO: context.DrawFocusRingPath(ContouredRect path)
              |
              +---> NO: Has uniform radius?
                        +---> YES: context.DrawFocusRingPath(path, radius)
                        +---> NO: AddCornerRadiiToPath() then DrawFocusRingPath()
```

## Graphics Context Operations

```
GraphicsContext operations used by OutlinePainter:

Drawing:
  +---> FillRect()           [Solid outline fill]
  +---> FillPath()           [Complex path fill]
  +---> StrokePath()         [Dotted/dashed strokes]
  +---> DrawFocusRingRect()  [Rectangular focus rings]
  +---> DrawFocusRingPath()  [Path-based focus rings]

Clipping:
  +---> ClipPath()           [Constrain drawing area]

State:
  +---> SetFillColor()       [Fill color]
  +---> SetStrokeColor()     [Stroke color]
  +---> SetStroke()          [Stroke style]
  +---> SetStrokeThickness() [Line width]
  +---> BeginLayer()/EndLayer() [Alpha compositing]
```
