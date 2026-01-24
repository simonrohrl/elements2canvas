# CSSMaskPainter Trace Reference

## Overview

This document provides a detailed trace of `CSSMaskPainter` and related mask painting operations in Chromium's Blink rendering engine.

## CSSMaskPainter::MaskBoundingBox() - Line-by-Line Trace

### Source: `css_mask_painter.cc`

```cpp
// Line 44-46: Entry point with parameters
std::optional<gfx::RectF> CSSMaskPainter::MaskBoundingBox(
    const LayoutObject& object,
    const PhysicalOffset& paint_offset) {
```

**Parameters:**
- `object`: The layout object that may have a CSS mask applied
- `paint_offset`: The offset from the local coordinate system to paint coordinates

```cpp
// Line 47-48: Guard - only box model objects and SVG children can have masks
  if (!object.IsBoxModelObject() && !object.IsSVGChild())
    return std::nullopt;
```

**Rationale:** Masks are only applicable to:
- Box model objects (LayoutBox, LayoutInline)
- SVG child elements (LayoutSVG*, LayoutSVGForeignObject)

```cpp
// Line 50-52: Check if the object actually has a mask
  const ComputedStyle& style = object.StyleRef();
  if (!style.HasMask())
    return std::nullopt;
```

**Check:** `HasMask()` returns true if either:
- `mask-image` property is set (via MaskLayers)
- `mask-box-image` property is set

```cpp
// Line 54-66: Special handling for SVG children
  if (object.IsSVGChild()) {
    // This is a kludge. The spec[1] says that a non-existent <mask>
    // reference should yield an image layer of transparent black.
    //
    // [1] https://drafts.fxtf.org/css-masking/#the-mask-image
    if (HasSingleInvalidSVGMaskLayer(object, style.MaskLayers())) {
      return std::nullopt;
    }
    // foreignObject handled by the regular box code.
    if (!object.IsSVGForeignObject()) {
      return SVGMaskPainter::ResourceBoundsForSVGChild(object);
    }
  }
```

**SVG Path:**
1. Check for invalid SVG mask (spec compliance)
2. Non-foreignObject SVG elements use `SVGMaskPainter` for bounds
3. foreignObject falls through to box code

```cpp
// Line 68-93: Compute maximum mask region for box elements
  PhysicalRect maximum_mask_region;
  EFillBox maximum_mask_clip = style.MaskLayers().LayersClipMax();
```

**`LayersClipMax()`:** Returns the "maximum" clip box across all mask layers:
- If any layer uses `no-clip`, returns `kNoClip`
- Otherwise returns the outermost box type used

```cpp
// Line 70-79: Handle LayoutBox elements
  if (object.IsBox()) {
    if (maximum_mask_clip == EFillBox::kNoClip) {
      maximum_mask_region =
          To<LayoutBox>(object)
              .Layer()
              ->LocalBoundingBoxIncludingSelfPaintingDescendants();
    } else {
      // We could use a tighter rect for padding-box/content-box.
      maximum_mask_region = To<LayoutBox>(object).PhysicalBorderBoxRect();
    }
```

**Box Bounds Logic:**
- `kNoClip`: Include all self-painting descendants (mask affects entire subtree)
- Other values: Use border box (conservative but correct)

```cpp
// Line 80-93: Handle LayoutInline elements
  } else {
    // For inline elements, depends on the value of box-decoration-break
    // there could be one box in multiple fragments or multiple boxes.
    if (maximum_mask_clip == EFillBox::kNoClip) {
      maximum_mask_region =
          To<LayoutInline>(object)
              .Layer()
              ->LocalBoundingBoxIncludingSelfPaintingDescendants();
    } else {
      // We could use a tighter rect for padding-box/content-box.
      maximum_mask_region = To<LayoutInline>(object).PhysicalLinesBoundingBox();
    }
  }
```

**Inline Bounds Logic:**
- `kNoClip`: Include all self-painting descendants
- Other values: Use lines bounding box (accounts for line breaks)

```cpp
// Line 94-97: Apply mask-box-image outsets and offset
  if (style.HasMaskBoxImageOutsets())
    maximum_mask_region.Expand(style.MaskBoxImageOutsets());
  maximum_mask_region.offset += paint_offset;
  return gfx::RectF(maximum_mask_region);
```

**Final Adjustments:**
1. Expand by `mask-box-image-outset` if present
2. Add paint offset to convert to paint coordinates
3. Return as floating-point rectangle

---

## Helper: HasSingleInvalidSVGMaskLayer()

```cpp
// Line 20-40: Check for invalid single SVG mask layer
bool HasSingleInvalidSVGMaskLayer(const LayoutObject& object,
                                  const FillLayer& first_layer) {
  // Multiple layers - not a single invalid layer
  if (first_layer.Next()) {
    return false;
  }

  const StyleImage* image = first_layer.GetImage();
  if (!image) {
    return false;
  }

  // Error state = invalid
  if (image->ErrorOccurred()) {
    return true;
  }

  // Check if it's an SVG mask reference
  auto* mask_source = DynamicTo<StyleMaskSourceImage>(*image);
  if (!mask_source || !mask_source->HasSVGMask()) {
    return false;
  }

  // Valid only if SVGMaskPainter can resolve it
  return !SVGMaskPainter::MaskIsValid(*mask_source, object);
}
```

**Purpose:** Per CSS Masking spec, a non-existent `<mask>` reference should yield transparent black. This helper detects that case.

---

## SVGMaskPainter::Paint() Trace

### Source: `svg_mask_painter.cc`

```cpp
// Line 236-265: Main SVG mask painting entry point
void SVGMaskPainter::Paint(GraphicsContext& context,
                           const LayoutObject& layout_object,
                           const DisplayItemClient& display_item_client) {
```

**Step 1: Get paint properties**
```cpp
  const auto* properties = layout_object.FirstFragment().PaintProperties();
  DCHECK(properties);
  DCHECK(properties->Mask());
  DCHECK(properties->MaskClip());
```

**Step 2: Setup property tree state**
```cpp
  PropertyTreeStateOrAlias property_tree_state(
      properties->Mask()->LocalTransformSpace(),
      *properties->MaskClip(),
      *properties->Mask());
  ScopedPaintChunkProperties scoped_paint_chunk_properties(
      context.GetPaintController(), property_tree_state, display_item_client,
      DisplayItem::kSVGMask);
```

**Step 3: Cache check**
```cpp
  if (DrawingRecorder::UseCachedDrawingIfPossible(context, display_item_client,
                                                  DisplayItem::kSVGMask))
    return;
```

**Step 4: Record drawing**
```cpp
  gfx::RectF visual_rect = properties->MaskClip()->PaintClipRect().Rect();
  DrawingRecorder recorder(context, display_item_client, DisplayItem::kSVGMask,
                           gfx::ToEnclosingRect(visual_rect));
```

**Step 5: Paint mask layers (bottom to top)**
```cpp
  const SVGBackgroundPaintContext bg_paint_context(layout_object);
  IterateFillLayersReversed(
      &layout_object.StyleRef().MaskLayers(),
      [&layout_object, &bg_paint_context, &context](const FillLayer& layer) {
        PaintMaskLayer(layer, layout_object, bg_paint_context, context);
      });
```

---

## SVGMaskPainter::PaintSVGMaskLayer() Trace

```cpp
// Line 267-300: Paint a single SVG mask layer
void SVGMaskPainter::PaintSVGMaskLayer(GraphicsContext& context,
                                       const StyleMaskSourceImage& mask_source,
                                       const ImageResourceObserver& observer,
                                       const gfx::RectF& reference_box,
                                       const float zoom,
                                       const SkBlendMode composite_op,
                                       const bool apply_mask_type) {
```

**Step 1: Resolve mask resource**
```cpp
  LayoutSVGResourceMasker* masker =
      ResolveElementReference(mask_source, observer);
  if (!masker) {
    return;
  }
```

**Step 2: Compute content transform**
```cpp
  const AffineTransform content_transformation =
      MaskToContentTransform(*masker, reference_box, zoom);
  SubtreeContentTransformScope content_transform_scope(content_transformation);
  PaintRecord record = masker->CreatePaintRecord();
```

**Step 3: Apply clipping**
```cpp
  context.Clip(masker->ResourceBoundingBox(reference_box, zoom));
```

**Step 4: Handle luminance and composite modes**
```cpp
  bool has_layer = false;
  if (apply_mask_type &&
      masker->StyleRef().MaskType() == EMaskType::kLuminance) {
    context.BeginLayer(cc::ColorFilter::MakeLuma(), &composite_op);
    has_layer = true;
  } else if (composite_op != SkBlendMode::kSrcOver) {
    context.BeginLayer(composite_op);
    has_layer = true;
  }
```

**Step 5: Draw mask content**
```cpp
  context.ConcatCTM(content_transformation);
  context.DrawRecord(std::move(record));
  if (has_layer) {
    context.EndLayer();
  }
```

---

## GraphicsContext Draw Operations Summary

### Operations in SVGMaskPainter::PaintSVGMaskLayer

| Line | Operation | Parameters | Purpose |
|------|-----------|------------|---------|
| 284 | `Clip()` | `ResourceBoundingBox(reference_box, zoom)` | Restrict drawing to mask bounds |
| 289 | `BeginLayer()` | `ColorFilter::MakeLuma(), &composite_op` | Start luminance conversion layer |
| 292 | `BeginLayer()` | `composite_op` | Start compositing layer |
| 295 | `ConcatCTM()` | `content_transformation` | Apply mask content transformation |
| 296 | `DrawRecord()` | `record` | Draw the mask's paint record |
| 298 | `EndLayer()` | - | Close the layer |

### Operations in PaintMaskLayer (for non-SVG masks)

| Line | Operation | Parameters | Purpose |
|------|-----------|------------|---------|
| 162 | `Scale()` | `inv_zoom, inv_zoom` | Adjust coordinate space for zoom |
| 189 | `Clip()` | `clip_rect` | Clip to specified box |
| 222-223 | `DrawImageTiled()` | `image, dest_rect, tiling_info, ...` | Draw tiled mask image |

---

## CSS Mask Types and Their Handling

### 1. CSS Gradient Mask
```css
mask-image: linear-gradient(black, transparent);
```
- Handled by: `PaintMaskLayer()` -> `DrawImageTiled()`
- Uses: `BackgroundImageGeometry` for positioning

### 2. CSS Image Mask
```css
mask-image: url(mask.png);
```
- Handled by: `PaintMaskLayer()` -> `DrawImageTiled()`
- Uses: `StyleFetchedImage` for image loading

### 3. SVG `<mask>` Reference
```css
mask-image: url(#myMask);
```
- Handled by: `PaintSVGMaskLayer()` -> `DrawRecord()`
- Uses: `LayoutSVGResourceMasker` for mask content

### 4. External SVG Mask
```css
mask-image: url(external.svg#mask);
```
- Handled by: `PaintSVGMaskLayer()` (after resource load)
- Uses: `SVGResource` + `StyleMaskSourceImage`

### 5. Mask Box Image
```css
-webkit-mask-box-image: url(border-mask.png);
```
- Handled by: `BoxPainterBase::PaintMaskImages()` -> `NinePieceImagePainter::Paint()`
- Uses: Nine-slice image algorithm

---

## Mask Modes

### Alpha Mode (Default for Images)
- Source: Alpha channel of mask image
- Implementation: Direct compositing with SrcOver or specified blend mode

### Luminance Mode (Default for SVG `<mask>`)
- Source: Luminance values converted to alpha
- Implementation: `BeginLayer(cc::ColorFilter::MakeLuma(), ...)`
- Formula: `alpha = 0.2126*R + 0.7152*G + 0.0722*B`

### Match-Source Mode
- Behavior: Use alpha for images, luminance for SVG masks
- Check: `masker->StyleRef().MaskType() == EMaskType::kLuminance`

---

## Transform Calculations

### MaskToContentTransform()

```cpp
AffineTransform MaskToContentTransform(const LayoutSVGResourceMasker& masker,
                                       const gfx::RectF& reference_box,
                                       float zoom) {
  AffineTransform content_transformation;
  if (masker.MaskContentUnits() ==
      SVGUnitTypes::kSvgUnitTypeObjectboundingbox) {
    // objectBoundingBox: scale/translate to reference box
    content_transformation.Translate(reference_box.x(), reference_box.y());
    content_transformation.ScaleNonUniform(reference_box.width(),
                                           reference_box.height());
  } else if (zoom != 1) {
    // userSpaceOnUse: apply zoom
    content_transformation.Scale(zoom);
  }
  return content_transformation;
}
```

**maskContentUnits values:**
- `objectBoundingBox`: Mask content coordinates are 0-1, mapped to reference box
- `userSpaceOnUse`: Mask content uses same coordinates as masked element

---

## Integration with Paint Property Tree

### MaskClip Node Creation

```cpp
// From paint_property_tree_builder.cc
std::optional<gfx::RectF> mask_clip = CSSMaskPainter::MaskBoundingBox(
    object_, context_.current.paint_offset);
if (mask_clip || needs_mask_based_clip_path_) {
  gfx::RectF combined_clip = mask_clip ? *mask_clip : *precise_clip_path_rect_;
  if (mask_clip && needs_mask_based_clip_path_)
    combined_clip.Intersect(*precise_clip_path_rect_);
  properties_->UpdateMaskClip(*context_.current.clip,
      ClipPaintPropertyNode::State(..., combined_clip, ...));
}
```

**Property Tree Integration:**
1. `MaskBoundingBox()` computes mask bounds
2. Bounds combined with clip-path if present
3. `MaskClip` property node created
4. Used by compositor for layer bounds

---

## Error Handling

### Invalid Mask Handling

1. **Resource Error**: `image->ErrorOccurred()` -> Invalid
2. **Missing Reference**: `!ResolveElementReference()` -> Skipped
3. **Display Lock**: `DisplayLockUtilities::LockedAncestorPreventingLayout()` -> Skipped
4. **Layout Pending**: `SECURITY_CHECK(!masker->SelfNeedsFullLayout())` -> Assert

### Spec Compliance

Per [CSS Masking Level 1](https://drafts.fxtf.org/css-masking/):
- Invalid mask-image: Treated as transparent black layer
- Non-existent SVG reference: Treated as transparent black layer
- Implementation: Return `nullopt` from `MaskBoundingBox()`
