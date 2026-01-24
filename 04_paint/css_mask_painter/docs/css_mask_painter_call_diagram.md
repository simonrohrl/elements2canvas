# CSSMaskPainter Call Diagram

## Entry Point: MaskBoundingBox

```
CSSMaskPainter::MaskBoundingBox(object, paint_offset)
|
+-- [Guard] Check: object.IsBoxModelObject() || object.IsSVGChild()
|       |
|       +-- Return nullopt if neither
|
+-- [Guard] style.HasMask()
|       |
|       +-- Return nullopt if no mask
|
+-- [Branch] object.IsSVGChild()
|       |
|       +-- [Guard] HasSingleInvalidSVGMaskLayer(object, style.MaskLayers())
|       |       |
|       |       +-- Return nullopt if invalid SVG mask
|       |
|       +-- [Branch] !object.IsSVGForeignObject()
|               |
|               +-- Return SVGMaskPainter::ResourceBoundsForSVGChild(object)
|
+-- [Compute] maximum_mask_region based on element type
|       |
|       +-- [Branch] object.IsBox()
|       |       |
|       |       +-- [Branch] maximum_mask_clip == EFillBox::kNoClip
|       |       |       |
|       |       |       +-- Layer()->LocalBoundingBoxIncludingSelfPaintingDescendants()
|       |       |
|       |       +-- [Else]
|       |               |
|       |               +-- PhysicalBorderBoxRect()
|       |
|       +-- [Else] (LayoutInline)
|               |
|               +-- [Branch] maximum_mask_clip == EFillBox::kNoClip
|               |       |
|               |       +-- Layer()->LocalBoundingBoxIncludingSelfPaintingDescendants()
|               |
|               +-- [Else]
|                       |
|                       +-- PhysicalLinesBoundingBox()
|
+-- [Optional] style.HasMaskBoxImageOutsets()
|       |
|       +-- maximum_mask_region.Expand(style.MaskBoxImageOutsets())
|
+-- maximum_mask_region.offset += paint_offset
|
+-- Return gfx::RectF(maximum_mask_region)
```

## Helper Function: HasSingleInvalidSVGMaskLayer

```
HasSingleInvalidSVGMaskLayer(object, first_layer)
|
+-- [Guard] first_layer.Next()
|       |
|       +-- Return false (multiple layers)
|
+-- [Get] image = first_layer.GetImage()
|       |
|       +-- Return false if !image
|
+-- [Guard] image->ErrorOccurred()
|       |
|       +-- Return true (invalid - error state)
|
+-- [Cast] mask_source = DynamicTo<StyleMaskSourceImage>(*image)
|       |
|       +-- Return false if !mask_source || !mask_source->HasSVGMask()
|
+-- Return !SVGMaskPainter::MaskIsValid(*mask_source, object)
```

## Consumer: PaintPropertyTreeBuilder

```
PaintPropertyTreeBuilder::UpdateEffect()
|
+-- [Check] NeedsPaintPropertyUpdate() && NeedsEffect()
|
+-- CSSMaskPainter::MaskBoundingBox(object_, context_.current.paint_offset)
|       |
|       +-- [Use] mask_clip for MaskClip property
|
+-- [Optional] Combine with clip-path rect
|
+-- properties_->UpdateMaskClip(...)
|       |
|       +-- Create ClipPaintPropertyNode with combined_clip
|
+-- [Create] EffectPaintPropertyNode::State
        |
        +-- Set opacity, backdrop-filter effects
        +-- Reference mask compositor element ID
```

## SVG Mask Painting Flow

```
SVGMaskPainter::Paint(context, layout_object, display_item_client)
|
+-- [Get] properties from layout_object.FirstFragment().PaintProperties()
|
+-- [Create] PropertyTreeStateOrAlias with Mask properties
|
+-- [Create] ScopedPaintChunkProperties
|
+-- [Cache Check] DrawingRecorder::UseCachedDrawingIfPossible()
|
+-- [Get] visual_rect from MaskClip
|
+-- [Create] DrawingRecorder
|
+-- [Create] SVGBackgroundPaintContext
|
+-- IterateFillLayersReversed(style.MaskLayers(), PaintMaskLayer)
        |
        +-- PaintMaskLayer() for each layer (bottom to top)
```

## PaintMaskLayer Flow

```
PaintMaskLayer(layer, object, bg_paint_context, context)
|
+-- [Guard] style_image = layer.GetImage()
|
+-- [Setup] composite_op = SkBlendMode::kSrcOver (or from layer if not bottom)
|
+-- [Branch] layer.MaskMode() == EFillMaskMode::kLuminance
|       |
|       +-- [Create] ScopedMaskLuminanceLayer(context, composite_op)
|       +-- Reset composite_op to SrcOver
|
+-- [Create] GraphicsContextStateSaver (lazy)
|
+-- [Branch] SVG <mask> reference
|       |
|       +-- [Compute] reference_box with zoom
|       +-- saver.Save()
|       +-- SVGMaskPainter::PaintSVGMaskLayer(...)
|       +-- Return
|
+-- [Compute] BackgroundImageGeometry::Calculate()
|
+-- [Guard] geometry.TileSize().IsEmpty()
|
+-- [Get] image from style_image->GetImage()
|
+-- [Create] ScopedImageRenderingSettings
|
+-- [Adjust] Coordinate space for zoom if needed
|
+-- [Setup] Clip box based on layer.Clip()
|
+-- [Compute] ImageTilingInfo
|
+-- context.DrawImageTiled(...)
```

## SVGMaskPainter::PaintSVGMaskLayer Flow

```
PaintSVGMaskLayer(context, mask_source, observer, reference_box, zoom, composite_op, apply_mask_type)
|
+-- [Resolve] masker = ResolveElementReference(mask_source, observer)
|       |
|       +-- Return if !masker
|
+-- [Compute] content_transformation = MaskToContentTransform()
|
+-- [Create] SubtreeContentTransformScope
|
+-- [Create] record = masker->CreatePaintRecord()
|
+-- context.Clip(masker->ResourceBoundingBox())
|
+-- [Branch] apply_mask_type && luminance mask
|       |
|       +-- context.BeginLayer(cc::ColorFilter::MakeLuma(), &composite_op)
|       +-- has_layer = true
|
+-- [Branch] composite_op != SrcOver
|       |
|       +-- context.BeginLayer(composite_op)
|       +-- has_layer = true
|
+-- context.ConcatCTM(content_transformation)
|
+-- context.DrawRecord(record)
|
+-- [Optional] context.EndLayer() if has_layer
```

## Data Flow Diagram

```
+------------------+
| ComputedStyle    |
+------------------+
        |
        | HasMask(), MaskLayers(), MaskBoxImageOutsets()
        v
+------------------+     +------------------+
| CSSMaskPainter   |---->| gfx::RectF       |
| ::MaskBoundingBox|     | (mask bounds)    |
+------------------+     +------------------+
        |                         |
        | (for SVG)               | (used by)
        v                         v
+------------------+     +---------------------------+
| SVGMaskPainter   |     | PaintPropertyTreeBuilder  |
| ::ResourceBounds |     | (creates MaskClip node)   |
+------------------+     +---------------------------+

+------------------+
| FillLayer        |
| (mask-image)     |
+------------------+
        |
        | GetImage()
        v
+---------------------+
| StyleMaskSourceImage|
+---------------------+
        |
        | HasSVGMask(), GetSVGResource()
        v
+------------------+     +------------------+
| SVGResource      |---->| LayoutSVG        |
|                  |     | ResourceMasker   |
+------------------+     +------------------+
                                |
                                | CreatePaintRecord()
                                v
                         +------------------+
                         | PaintRecord      |
                         | (mask content)   |
                         +------------------+
```

## GraphicsContext Operations Summary

| Operation | Location | Purpose |
|-----------|----------|---------|
| `BeginLayer(ColorFilter::MakeLuma())` | SVGMaskPainter::PaintSVGMaskLayer | Luminance mask conversion |
| `BeginLayer(composite_op)` | SVGMaskPainter::PaintSVGMaskLayer | Composite operation layer |
| `EndLayer()` | SVGMaskPainter::PaintSVGMaskLayer | Close layer |
| `Clip(rect)` | SVGMaskPainter::PaintSVGMaskLayer, PaintMaskLayer | Restrict drawing to mask bounds |
| `ConcatCTM(transform)` | SVGMaskPainter::PaintSVGMaskLayer | Apply mask content transform |
| `DrawRecord(record)` | SVGMaskPainter::PaintSVGMaskLayer | Draw SVG mask content |
| `DrawImageTiled(...)` | PaintMaskLayer | Draw raster/CSS mask image |
| `Scale(inv_zoom, inv_zoom)` | PaintMaskLayer | Adjust for zoom |
