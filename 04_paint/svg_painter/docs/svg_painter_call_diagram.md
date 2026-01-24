# SVG Painter Call Diagram

This document provides detailed call flow diagrams for SVG painting operations.

## Main Paint Flow

```
PaintLayerPainter::Paint()
    |
    +-- [For SVG root elements]
    |   BoxFragmentPainter::Paint()
    |       |
    |       +-- SVGRootPainter::PaintReplaced()
    |               |
    |               +-- [For each child]
    |                   +-- child->Paint() --> virtual dispatch
    |
    +-- [For SVG masks on foreignObject]
    |   SVGMaskPainter::Paint(context, object, object)
    |
    +-- [For clip-path masks]
        ClipPathClipper::PaintClipPathAsMaskImage()
```

## SVGRootPainter::PaintReplaced()

```
SVGRootPainter::PaintReplaced(paint_info, paint_offset)
    |
    +-- PixelSnappedSize(paint_offset) --> check for empty viewport
    |
    +-- svg->HasEmptyViewBox() --> check for empty viewBox
    |
    +-- paint_info.DescendantPaintingBlocked() --> early exit if blocked
    |
    +-- [For each child of SVG root]
        |
        +-- [If LayoutSVGForeignObject]
        |   SVGForeignObjectPainter(*foreign_object).PaintLayer(paint_info)
        |
        +-- [Else]
            child->Paint(paint_info) --> virtual dispatch to child painter
```

## SVGContainerPainter::Paint()

```
SVGContainerPainter::Paint(paint_info)
    |
    +-- Check for empty viewBox (SVGViewportContainer)
    |
    +-- ScopedSVGPaintState::ComputePaintBehavior() --> determine what to paint
    |   |
    |   +-- Returns empty if nothing to paint
    |   +-- Returns {kContent} or {kReferenceFilter} or All()
    |
    +-- CanUseCullRect() --> optimization check
    |   |
    |   +-- Returns false if:
    |       - IsSVGHiddenContainer()
    |       - SVGDescendantMayHaveTransformRelatedOperations()
    |       - HasTransform() or pixel-moving filter
    |
    +-- [If can use cull rect]
    |   paint_info.GetCullRect().IntersectsTransformed() --> skip if outside
    |   properties->Transform()->TransformCullRect() --> transform cull rect
    |
    +-- [Else]
    |   paint_info_before_filtering.ApplyInfiniteCullRect()
    |
    +-- ScopedSVGTransformState(paint_info, container)
    |   |
    |   +-- Sets up transform property scope
    |   +-- Transforms context paints
    |
    +-- [Optional overflow clip for viewport containers]
    |   ScopedPaintChunkProperties with OverflowClip
    |
    +-- ScopedSVGPaintState(container, paint_info, paint_behavior)
    |   |
    |   +-- ApplyEffects() on construction
    |   |   +-- ApplyPaintPropertyState() --> set up Effect, Clip properties
    |   +-- ~ScopedSVGPaintState() on destruction
    |       +-- SVGMaskPainter::Paint() if should_paint_mask_
    |       +-- ClipPathClipper::PaintClipPathAsMaskImage() if clip path
    |
    +-- [If content should be painted]
    |   +-- [If <use> element]
    |   |   SVGObjectPainter::ResolveContextPaint() for fill/stroke
    |   |   Create child_context_paints and set on paint_info
    |   |
    |   +-- [For each child]
    |       |
    |       +-- [If LayoutSVGForeignObject]
    |       |   SVGForeignObjectPainter(*foreign_object).PaintLayer()
    |       |
    |       +-- [Else]
    |           child->Paint(child_paint_info)
    |
    +-- [If has children]
    |   SVGModelObjectPainter(container).PaintOutline()
    |
    +-- [If should add URL metadata]
        ObjectPainter(container).AddURLRectIfNeeded()
```

## SVGShapePainter::Paint()

```
SVGShapePainter::Paint(paint_info)
    |
    +-- Check paint_info.phase == PaintPhase::kForeground
    +-- Check !layout_svg_shape_.IsShapeEmpty()
    |
    +-- ScopedSVGPaintState::ComputePaintBehavior()
    |
    +-- [If can use cull rect]
    |   paint_info.GetCullRect().IntersectsTransformed()
    |
    +-- ScopedSVGTransformState(paint_info, shape)
    |
    +-- ScopedSVGPaintState(shape, content_paint_info, paint_behavior)
    |
    +-- [If content should be painted]
    |   |
    |   +-- SVGModelObjectPainter::RecordHitTestData()
    |   +-- SVGModelObjectPainter::RecordRegionCaptureData()
    |   |
    |   +-- [If not cached]
    |       SVGDrawingRecorder(context, shape, phase)
    |       PaintShape(content_paint_info)
    |
    +-- SVGModelObjectPainter(shape).PaintOutline()
```

## SVGShapePainter::PaintShape()

```
PaintShape(paint_info)
    |
    +-- Determine should_anti_alias from shape-rendering
    |
    +-- [If rendering clip path as mask]
    |   FillShape() with black color and clip rule
    |   RETURN
    |
    +-- [For each paint order item (fill/stroke/markers)]
        |
        +-- [Case PT_FILL]
        |   |
        |   +-- SVGObjectPainter::HasFill() --> check if has fill
        |   +-- SVGObjectPainter::PreparePaint() --> prepare fill flags
        |   +-- FillShape(context, fill_flags, fill_rule)
        |       |
        |       +-- [Switch on geometry type]
        |           - kRectangle: context.DrawRect()
        |           - kCircle/kEllipse: context.DrawOval()
        |           - kPath: context.DrawPath()
        |       +-- PaintTiming::MarkFirstContentfulPaint()
        |
        +-- [Case PT_STROKE]
        |   |
        |   +-- SVGObjectPainter::HasVisibleStroke()
        |   +-- [If non-scaling stroke]
        |   |   SetupNonScalingStrokeContext() --> reset transform
        |   +-- SVGObjectPainter::PreparePaint() --> prepare stroke flags
        |   +-- SVGLayoutSupport::ApplyStrokeStyleToStrokeData()
        |   +-- StrokeShape(context, stroke_flags)
        |       |
        |       +-- [Switch on geometry type, force path for non-scaling]
        |           - context.DrawRect/DrawOval/DrawPath()
        |       +-- PaintTiming::MarkFirstContentfulPaint()
        |
        +-- [Case PT_MARKERS]
            PaintMarkers(paint_info)
                |
                +-- [For each marker position]
                    PaintMarker(paint_info, marker, position, stroke_width)
                        |
                        +-- marker.MarkerTransformation()
                        +-- canvas->save/concat/clipRect
                        +-- PaintRecordBuilder for isolated drawing
                        +-- SVGContainerPainter(marker).Paint()
                        +-- canvas->restore
```

## SVGImagePainter::Paint()

```
SVGImagePainter::Paint(paint_info)
    |
    +-- Check phase == PaintPhase::kForeground
    +-- Check ImageResource()->HasImage()
    |
    +-- ScopedSVGPaintState::ComputePaintBehavior()
    |
    +-- [If privacy preserving and not allowed]
    |   RETURN
    |
    +-- [If can use cull rect]
    |   IntersectsTransformed() check
    |
    +-- ScopedSVGTransformState(paint_info, image)
    |
    +-- ScopedSVGPaintState(image, paint_info, paint_behavior)
    |
    +-- [If content should be painted]
    |   +-- SVGModelObjectPainter::RecordHitTestData()
    |   +-- SVGModelObjectPainter::RecordRegionCaptureData()
    |   +-- [If not cached]
    |       SVGDrawingRecorder(context, image, phase)
    |       PaintForeground(paint_info)
    |           |
    |           +-- ComputeImageViewportSize()
    |           +-- image_resource.GetImage()
    |           +-- preserveAspectRatio->TransformRect()
    |           +-- ImageElementTiming::NotifyImagePaint()
    |           +-- PaintTiming::MarkFirstContentfulPaint()
    |           +-- context.DrawImage()
    |
    +-- SVGModelObjectPainter(image).PaintOutline()
```

## SVGForeignObjectPainter::PaintLayer()

```
SVGForeignObjectPainter::PaintLayer(paint_info)
    |
    +-- Check phase is Foreground or SelectionDragImage
    |
    +-- Check FirstFragment().HasLocalBorderBoxProperties()
    |
    +-- PaintLayerPainter(*foreign_object.Layer()).Paint(context, flags)
        |
        +-- [Normal PaintLayerPainter flow]
        +-- Paints HTML content inside SVG
```

## SVGMaskPainter::Paint()

```
SVGMaskPainter::Paint(context, layout_object, display_item_client)
    |
    +-- Get Mask and MaskClip from PaintProperties
    |
    +-- ScopedPaintChunkProperties with mask property state
    |
    +-- [If cached]
    |   RETURN
    |
    +-- DrawingRecorder(context, client, kSVGMask, visual_rect)
    |
    +-- SVGBackgroundPaintContext(layout_object)
    |
    +-- [Iterate mask layers in reverse]
        PaintMaskLayer(layer, object, bg_paint_context, context)
            |
            +-- [If SVG mask reference]
            |   PaintSVGMaskLayer()
            |       |
            |       +-- ResolveElementReference() --> get LayoutSVGResourceMasker
            |       +-- MaskToContentTransform() --> compute transform
            |       +-- masker->CreatePaintRecord()
            |       +-- context.Clip(ResourceBoundingBox)
            |       +-- [If luminance mask]
            |       |   context.BeginLayer(MakeLuma filter)
            |       +-- context.ConcatCTM(content_transformation)
            |       +-- context.DrawRecord()
            |       +-- context.EndLayer() if needed
            |
            +-- [If image mask]
                BackgroundImageGeometry::Calculate()
                image->GetImage()
                context.DrawImageTiled()
```

## SVGObjectPainter::PreparePaint()

```
PreparePaint(paint_flags, style, resource_mode, cc_flags, additional_transform)
    |
    +-- Determine alpha (FillOpacity or StrokeOpacity)
    +-- Get initial_paint (FillPaint or StrokePaint)
    +-- ResolveContextPaint() --> handle contextFill/contextStroke
    |
    +-- [If paint has URL (gradient/pattern)]
    |   |
    |   +-- ResolveContextTransform() --> additional transform handling
    |   +-- ApplyPaintResource()
    |       |
    |       +-- SVGResources::GetClient()
    |       +-- GetSVGResourceAsType<LayoutSVGResourcePaintServer>()
    |       +-- uri_resource->ApplyShader()
    |   |
    |   +-- flags.setColor(Black)
    |   +-- flags.setAlphaf(alpha)
    |   +-- ApplyColorInterpolation()
    |   +-- RETURN true
    |
    +-- [If paint has color]
        |
        +-- Resolve color (contextFill, contextStroke, or direct)
        +-- flag_color.SetAlpha(alpha)
        +-- flags.setColor(flag_color)
        +-- flags.setShader(nullptr)
        +-- ApplyColorInterpolation()
        +-- RETURN true
```

## Paint Property Application (ScopedSVGPaintState)

```
ScopedSVGPaintState::ApplyEffects()
    |
    +-- Get PaintProperties from FirstFragment()
    |
    +-- ApplyPaintPropertyState(properties)
    |   |
    |   +-- Get current chunk properties
    |   +-- [If Filter exists]
    |   |   state.SetEffect(*filter)
    |   +-- [Else if Effect exists]
    |   |   state.SetEffect(*effect)
    |   +-- [If PixelMovingFilterClipExpander exists]
    |   |   state.SetClip(*filter_clip)
    |   +-- [Else if MaskClip exists]
    |   |   state.SetClip(*mask_clip)
    |   +-- [Else if ClipPathClip exists]
    |       state.SetClip(*clip_path_clip)
    |   |
    |   +-- ScopedPaintChunkProperties with new state
    |
    +-- [If rendering clip path as mask]
    |   should_paint_clip_path_as_mask_image_ = true
    |   RETURN
    |
    +-- [If foreignObject]
    |   RETURN (PaintLayerPainter handles effects)
    |
    +-- [If ClipPathMask]
    |   should_paint_clip_path_as_mask_image_ = true
    |
    +-- [If Mask]
        should_paint_mask_ = true

~ScopedSVGPaintState()
    |
    +-- [If should_paint_mask_]
    |   SVGMaskPainter::Paint()
    |
    +-- [If should_paint_clip_path_as_mask_image_]
        ClipPathClipper::PaintClipPathAsMaskImage()
```
