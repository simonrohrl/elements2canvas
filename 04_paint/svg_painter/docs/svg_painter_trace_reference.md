# SVG Painter Trace Reference

This document provides a detailed line-by-line trace of the key SVG painter methods, documenting decision points, graphics context operations, and key differences from regular HTML painting.

## Table of Contents

1. [Paint Phase Triggering](#paint-phase-triggering)
2. [SVGContainerPainter::Paint() Trace](#svgcontainerpainterpaint-trace)
3. [SVGShapePainter::Paint() Trace](#svgshapepainterpaint-trace)
4. [SVGRootPainter::PaintReplaced() Trace](#svgrootpainterpaintreplaced-trace)
5. [Context and Transform Handling](#context-and-transform-handling)
6. [Mask and Clip Path Handling](#mask-and-clip-path-handling)
7. [Key Differences from HTML Painting](#key-differences-from-html-painting)

---

## Paint Phase Triggering

### How SVG Painters are Called

SVG painters are triggered from `PaintLayerPainter::Paint()` which operates in several paint phases:

```cpp
// paint_layer_painter.cc:1075
if (should_paint_background) {
  PaintWithPhase(PaintPhase::kSelfBlockBackgroundOnly, context, paint_flags);
}
// ... negative z-order children ...

// Foreground painting (where SVG content is painted)
if (should_paint_content) {
  // paint_layer_painter.cc:1093
  PaintForegroundPhases(context, paint_flags);
}
```

For SVG masks specifically, `PaintLayerPainter::Paint()` checks for mask properties:

```cpp
// paint_layer_painter.cc:1131-1137
if (properties->Mask()) {
  if (object.IsSVGForeignObject()) {
    SVGMaskPainter::Paint(context, object, object);
  } else {
    PaintWithPhase(PaintPhase::kMask, context, paint_flags);
  }
}
```

### Context Passed to SVG Painters

The context passed to SVG painters includes:
- `GraphicsContext&` from `PaintInfo` - the drawing context
- `PaintInfo` - contains:
  - `CullRect` - clipping optimization
  - `PaintPhase` - current painting phase
  - `PaintFlags` - painting behavior flags
  - `SvgContextPaints*` - inherited fill/stroke for context paints

---

## SVGContainerPainter::Paint() Trace

**File:** `svg_container_painter.cc`

### Line-by-Line Analysis

```cpp
void SVGContainerPainter::Paint(const PaintInfo& paint_info) {
  // Line 49-57: DECISION POINT - Empty viewBox check
  // Spec: An empty viewBox on the <svg> element disables rendering.
  DCHECK(layout_svg_container_.GetElement());
  auto* viewport_container_element = DynamicTo<SVGViewportContainerElement>(
      *layout_svg_container_.GetElement());
  if (viewport_container_element &&
      viewport_container_element->HasEmptyViewBox()) {
    return;  // EXIT: Empty viewBox disables rendering
  }
```

**Decision Point 1:** Empty viewBox disables all rendering per SVG spec.

```cpp
  // Line 59-66: DECISION POINT - Compute paint behavior
  auto paint_behavior = ScopedSVGPaintState::ComputePaintBehavior(
      layout_svg_container_, paint_info,
      !RuntimeEnabledFeatures::SvgFilterPaintsForHiddenContentEnabled() ||
          layout_svg_container_.FirstChild());

  if (paint_behavior.empty()) {
    return;  // EXIT: Nothing to paint
  }
```

**Decision Point 2:** `ComputePaintBehavior()` returns:
- `{}` (empty) - no content and no reference filter to paint
- `{kContent}` - has content to paint
- `{kReferenceFilter}` - no content but has reference filter
- `All()` - both content and potential reference filter

```cpp
  // Line 68-89: DECISION POINT - Cull rect optimization
  const auto* properties =
      layout_svg_container_.FirstFragment().PaintProperties();
  PaintInfo paint_info_before_filtering(paint_info);

  if (CanUseCullRect()) {
    // Optimization: can skip if outside cull rect
    CHECK(paint_behavior.Has(ScopedSVGPaintState::PaintComponent::kContent));
    if (!paint_info.GetCullRect().IntersectsTransformed(
            layout_svg_container_.LocalToSVGParentTransform(),
            layout_svg_container_.VisualRectInLocalSVGCoordinates()))
      return;  // EXIT: Outside cull rect

    // Transform cull rect for children
    if (properties) {
      if (const auto* transform = properties->Transform())
        paint_info_before_filtering.TransformCullRect(*transform);
    }
  } else {
    // Can't optimize - use infinite cull rect
    paint_info_before_filtering.ApplyInfiniteCullRect();
  }
```

**Decision Point 3:** `CanUseCullRect()` returns false if:
- Container is `LayoutSVGHiddenContainer` (needs to paint for resources)
- Has descendants with transform-related operations
- Has transform or pixel-moving filter (per `SVGModelObjectPainter::CanUseCullRect()`)

```cpp
  // Line 91-106: TRANSFORM STATE SETUP
  ScopedSVGTransformState transform_state(paint_info_before_filtering,
                                          layout_svg_container_);
  {
    // Optional overflow clip for viewport containers
    std::optional<ScopedPaintChunkProperties> scoped_paint_chunk_properties;
    if (layout_svg_container_.IsSVGViewportContainer() &&
        SVGLayoutSupport::IsOverflowHidden(layout_svg_container_)) {
      if (properties && properties->OverflowClip()) {
        scoped_paint_chunk_properties.emplace(
            paint_info_before_filtering.context.GetPaintController(),
            *properties->OverflowClip(), layout_svg_container_,
            paint_info_before_filtering.DisplayItemTypeForClipping());
      }
    }
```

**Key Operation:** `ScopedSVGTransformState` sets up:
1. Transform paint property scope (if transform exists)
2. Updates context paints with accumulated transform

```cpp
    // Line 108-143: PAINT STATE AND CHILD PAINTING
    ScopedSVGPaintState paint_state(
        layout_svg_container_, paint_info_before_filtering, paint_behavior);

    // Ensure chunk for reference filters
    if (paint_info_before_filtering.phase == PaintPhase::kForeground &&
        properties && HasReferenceFilterEffect(*properties) &&
        !RuntimeEnabledFeatures::SvgFilterPaintsForHiddenContentEnabled()) {
      paint_info_before_filtering.context.GetPaintController().EnsureChunk();
    }

    if (paint_behavior.Has(ScopedSVGPaintState::PaintComponent::kContent)) {
      PaintInfo& child_paint_info = transform_state.ContentPaintInfo();

      // SPECIAL CASE: <use> element context paints
      std::optional<SvgContextPaints> child_context_paints;
      if (IsA<SVGUseElement>(layout_svg_container_.GetElement())) {
        SVGObjectPainter object_painter(layout_svg_container_,
                                        child_paint_info.GetSvgContextPaints());
        child_context_paints.emplace(
            object_painter.ResolveContextPaint(
                layout_svg_container_.StyleRef().FillPaint()),
            object_painter.ResolveContextPaint(
                layout_svg_container_.StyleRef().StrokePaint()));
        child_paint_info.SetSvgContextPaints(&(*child_context_paints));
      }

      // CHILD ITERATION
      for (LayoutObject* child = layout_svg_container_.FirstChild(); child;
           child = child->NextSibling()) {
        if (auto* foreign_object = DynamicTo<LayoutSVGForeignObject>(child)) {
          SVGForeignObjectPainter(*foreign_object)
              .PaintLayer(paint_info_before_filtering);
        } else {
          child->Paint(child_paint_info);
        }
      }
    }
  }
```

**Decision Point 4:** `<use>` elements establish new context paint scope by resolving their own fill/stroke as context paints for cloned content.

**Decision Point 5:** `<foreignObject>` children are painted via `SVGForeignObjectPainter::PaintLayer()` which bridges to `PaintLayerPainter`.

```cpp
  // Line 146-156: OUTLINE AND URL METADATA
  if (layout_svg_container_.FirstChild()) {
    SVGModelObjectPainter(layout_svg_container_)
        .PaintOutline(paint_info_before_filtering);
  }

  if (paint_info_before_filtering.ShouldAddUrlMetadata() &&
      paint_info_before_filtering.phase == PaintPhase::kForeground) {
    ObjectPainter(layout_svg_container_)
        .AddURLRectIfNeeded(paint_info_before_filtering, PhysicalOffset());
  }
}
```

---

## SVGShapePainter::Paint() Trace

**File:** `svg_shape_painter.cc`

### Line-by-Line Analysis

```cpp
void SVGShapePainter::Paint(const PaintInfo& paint_info) {
  // Line 43-49: EARLY EXIT CHECKS
  // SVG shapes ONLY paint in foreground phase
  if (paint_info.phase != PaintPhase::kForeground ||
      layout_svg_shape_.IsShapeEmpty()) {
    return;  // EXIT: Wrong phase or empty shape
  }
```

**Key Difference from HTML:** SVG shapes only paint in `kForeground` phase, not background or other phases.

```cpp
  // Line 51-57: PAINT BEHAVIOR DETERMINATION
  auto paint_behavior = ScopedSVGPaintState::ComputePaintBehavior(
      layout_svg_shape_, paint_info,
      layout_svg_shape_.StyleRef().Visibility() == EVisibility::kVisible);

  if (paint_behavior.empty()) {
    return;  // EXIT: Hidden or no content
  }
```

**Decision Point:** Visibility check - `visibility: hidden` prevents content painting.

```cpp
  // Line 59-68: CULL RECT OPTIMIZATION
  if (SVGModelObjectPainter::CanUseCullRect(layout_svg_shape_.StyleRef())) {
    CHECK(paint_behavior.Has(ScopedSVGPaintState::PaintComponent::kContent));
    if (!paint_info.GetCullRect().IntersectsTransformed(
            layout_svg_shape_.LocalSVGTransform(),
            layout_svg_shape_.VisualRectInLocalSVGCoordinates()))
      return;  // EXIT: Outside cull rect
  }
  // Shapes cannot have children so do not call TransformCullRect.
```

**Note:** Unlike containers, shapes don't transform cull rect (no children to propagate to).

```cpp
  // Line 71-91: TRANSFORM AND PAINT STATE
  ScopedSVGTransformState transform_state(paint_info, layout_svg_shape_);
  const PaintInfo& content_paint_info = transform_state.ContentPaintInfo();

  {
    ScopedSVGPaintState paint_state(layout_svg_shape_, content_paint_info,
                                    paint_behavior);
    if (paint_behavior.Has(ScopedSVGPaintState::PaintComponent::kContent)) {
      // Record hit test and region capture data
      SVGModelObjectPainter::RecordHitTestData(layout_svg_shape_,
                                               content_paint_info);
      SVGModelObjectPainter::RecordRegionCaptureData(layout_svg_shape_,
                                                     content_paint_info);
      // Check drawing cache
      if (!DrawingRecorder::UseCachedDrawingIfPossible(
              content_paint_info.context, layout_svg_shape_,
              content_paint_info.phase)) {
        SVGDrawingRecorder recorder(content_paint_info.context,
                                    layout_svg_shape_,
                                    content_paint_info.phase);
        PaintShape(content_paint_info);  // ACTUAL DRAWING
      }
    }
  }

  // Outline painting
  if (paint_behavior.Has(ScopedSVGPaintState::PaintComponent::kContent)) {
    SVGModelObjectPainter(layout_svg_shape_).PaintOutline(content_paint_info);
  }
}
```

### PaintShape() - Actual Drawing Operations

```cpp
void SVGShapePainter::PaintShape(const PaintInfo& paint_info) {
  const ComputedStyle& style = layout_svg_shape_.StyleRef();

  // Line 100-103: Anti-aliasing decision
  const bool should_anti_alias =
      style.ShapeRendering() != EShapeRendering::kCrispedges &&
      style.ShapeRendering() != EShapeRendering::kOptimizespeed;
```

**Decision Point:** `shape-rendering` property controls anti-aliasing.

```cpp
  // Line 104-110: SPECIAL CASE - Clip path rendering
  if (paint_info.IsRenderingClipPathAsMaskImage()) {
    cc::PaintFlags clip_flags;
    clip_flags.setColor(SK_ColorBLACK);
    clip_flags.setAntiAlias(should_anti_alias);
    FillShape(paint_info.context, clip_flags, style.ClipRule());
    return;  // EXIT: Only fill for clip paths
  }
```

**Key Difference:** When rendering as clip path mask, only fill with black using clip-rule.

```cpp
  // Line 112-171: PAINT ORDER ITERATION
  const PaintOrderArray paint_order(style.PaintOrder());
  for (unsigned i = 0; i < 3; i++) {
    switch (paint_order[i]) {
      case PT_FILL: {
        // Line 116-127: FILL OPERATION
        if (SVGObjectPainter::HasFill(style,
                                      paint_info.GetSvgContextPaints())) {
          cc::PaintFlags fill_flags;
          if (!SVGObjectPainter(layout_svg_shape_,
                                paint_info.GetSvgContextPaints())
                   .PreparePaint(paint_info.GetPaintFlags(), style,
                                 kApplyToFillMode, fill_flags)) {
            break;  // Paint preparation failed
          }
          fill_flags.setAntiAlias(should_anti_alias);
          FillShape(paint_info.context, fill_flags, style.FillRule());
        }
        break;
      }
```

**Graphics Context Call:** `FillShape()` calls:
- `context.DrawRect()` for rectangles
- `context.DrawOval()` for circles/ellipses
- `context.DrawPath()` for general paths

```cpp
      case PT_STROKE:
        // Line 130-163: STROKE OPERATION
        if (SVGObjectPainter::HasVisibleStroke(
                style, paint_info.GetSvgContextPaints())) {
          GraphicsContextStateSaver state_saver(paint_info.context, false);
          std::optional<AffineTransform> non_scaling_transform;

          // SPECIAL CASE: Non-scaling stroke
          if (layout_svg_shape_.HasNonScalingStroke()) {
            non_scaling_transform =
                SetupNonScalingStrokeContext(layout_svg_shape_, state_saver);
            if (!non_scaling_transform) {
              return;  // Non-invertible transform
            }
          }

          cc::PaintFlags stroke_flags;
          if (!SVGObjectPainter(layout_svg_shape_,
                                paint_info.GetSvgContextPaints())
                   .PreparePaint(paint_info.GetPaintFlags(), style,
                                 kApplyToStrokeMode, stroke_flags,
                                 base::OptionalToPtr(non_scaling_transform))) {
            break;
          }
          stroke_flags.setAntiAlias(should_anti_alias);

          // Apply stroke style
          StrokeData stroke_data;
          SVGLayoutSupport::ApplyStrokeStyleToStrokeData(
              stroke_data, style, layout_svg_shape_,
              layout_svg_shape_.DashScaleFactor());
          stroke_data.SetupPaint(&stroke_flags);

          StrokeShape(paint_info.context, stroke_flags);
        }
        break;
```

**Key Operation - Non-scaling stroke:** Resets transform to host coordinate system so stroke width remains constant regardless of element transforms.

```cpp
      case PT_MARKERS:
        // Line 165-167: MARKERS
        PaintMarkers(paint_info);
        break;
    }
  }
}
```

**Marker Painting:** For each marker position, creates isolated paint context with marker transform and paints marker via `SVGContainerPainter`.

---

## SVGRootPainter::PaintReplaced() Trace

**File:** `svg_root_painter.cc`

```cpp
void SVGRootPainter::PaintReplaced(const PaintInfo& paint_info,
                                   const PhysicalOffset& paint_offset) {
  // Line 66-67: DECISION POINT - Empty viewport
  if (PixelSnappedSize(paint_offset).IsEmpty())
    return;  // EXIT: Empty viewport

  // Line 69-74: DECISION POINT - Empty viewBox
  auto* svg = To<SVGSVGElement>(layout_svg_root_.GetNode());
  DCHECK(svg);
  if (svg->HasEmptyViewBox())
    return;  // EXIT: Empty viewBox

  // Line 76-78: Descendant painting blocked check
  if (paint_info.DescendantPaintingBlocked()) {
    return;
  }

  // Line 80-87: CHILD ITERATION
  for (LayoutObject* child = layout_svg_root_.FirstChild(); child;
       child = child->NextSibling()) {
    if (auto* foreign_object = DynamicTo<LayoutSVGForeignObject>(child)) {
      SVGForeignObjectPainter(*foreign_object).PaintLayer(paint_info);
    } else {
      child->Paint(paint_info);  // Virtual dispatch to child painter
    }
  }
}
```

### TransformToPixelSnappedBorderBox()

```cpp
AffineTransform SVGRootPainter::TransformToPixelSnappedBorderBox(
    const PhysicalOffset& paint_offset) const {
  const gfx::Rect snapped_size = PixelSnappedSize(paint_offset);
  AffineTransform paint_offset_to_border_box =
      AffineTransform::Translation(snapped_size.x(), snapped_size.y());

  const PhysicalSize size = layout_svg_root_.StitchedSize();
  if (!size.IsEmpty()) {
    // Scale adjustment for document root SVG
    if (layout_svg_root_.IsDocumentElement()) {
      paint_offset_to_border_box.Scale(
          snapped_size.width() / size.width.ToFloat(),
          snapped_size.height() / size.height.ToFloat());
    } else if (RuntimeEnabledFeatures::
                   SvgInlineRootPixelSnappingScaleAdjustmentEnabled()) {
      // Handle snapping shrinkage for inline SVG
      // ...scale adjustments...
    }
  }
  paint_offset_to_border_box.PreConcat(
      layout_svg_root_.LocalToBorderBoxTransform());
  return paint_offset_to_border_box;
}
```

**Key Operation:** Handles the coordinate transform from SVG user space to pixel-snapped border box, including zoom adjustments.

---

## Context and Transform Handling

### ScopedSVGTransformState

```cpp
ScopedSVGTransformState::ScopedSVGTransformState(const PaintInfo& paint_info,
                                                 const LayoutObject& object)
    : content_paint_info_(paint_info) {
  DCHECK(object.IsSVGChild());

  const auto* fragment = &object.FirstFragment();
  const auto* properties = fragment->PaintProperties();
  if (!properties) {
    return;
  }

  if (const auto* transform_node = properties->Transform()) {
    // Set up transform property scope
    transform_property_scope_.emplace(
        paint_info.context.GetPaintController(), *transform_node, object,
        DisplayItem::PaintPhaseToSVGTransformType(paint_info.phase));

    // Update context paints with accumulated transform
    if (auto* context_paints = paint_info.GetSvgContextPaints()) {
      transformed_context_paints_.emplace(
          context_paints->fill, context_paints->stroke,
          context_paints->transform *
              AffineTransform::FromTransform(transform_node->Matrix()));
      content_paint_info_.SetSvgContextPaints(
          base::OptionalToPtr(transformed_context_paints_));
    }
  }
}
```

**Key Operation:** Accumulates transforms in context paints for proper gradient/pattern rendering in transformed coordinate spaces.

---

## Mask and Clip Path Handling

### ScopedSVGPaintState Effects

```cpp
void ScopedSVGPaintState::ApplyEffects() {
  // Applied on construction for foreground phase
  const auto* properties = object_.FirstFragment().PaintProperties();
  if (!properties) return;

  ApplyPaintPropertyState(*properties);

  // Clip path rendering mode - skip non-geometric effects
  if (paint_info_.IsRenderingClipPathAsMaskImage()) {
    if (properties->ClipPathMask())
      should_paint_clip_path_as_mask_image_ = true;
    return;
  }

  // ForeignObject handles effects via PaintLayerPainter
  if (object_.IsSVGForeignObject()) {
    return;
  }

  // Schedule mask and clip-path painting for destructor
  if (properties->ClipPathMask()) {
    should_paint_clip_path_as_mask_image_ = true;
  }
  if (properties->Mask()) {
    should_paint_mask_ = true;
  }
}

ScopedSVGPaintState::~ScopedSVGPaintState() {
  // Paint mask first (mask contains clip-path mask as child effect)
  if (should_paint_mask_)
    SVGMaskPainter::Paint(paint_info_.context, object_, display_item_client_);

  if (should_paint_clip_path_as_mask_image_) {
    ClipPathClipper::PaintClipPathAsMaskImage(paint_info_.context, object_,
                                              display_item_client_);
  }
}
```

**Key Design:** Mask and clip-path are painted on scope destruction, after content, ensuring proper composition order.

---

## Key Differences from HTML Painting

### 1. Coordinate Systems

| Aspect | HTML Painting | SVG Painting |
|--------|---------------|--------------|
| Units | CSS pixels | User units (viewBox) |
| Origin | Top-left of border box | Depends on viewBox |
| Transform accumulation | CSS transforms | SVG transforms + viewBox |

### 2. Paint Phases

| Phase | HTML | SVG |
|-------|------|-----|
| Background | Yes | No (except SVG root) |
| Foreground | Yes | Primary phase |
| Outline | Yes | Yes |
| Mask | Separate phase | Integrated in foreground |

### 3. Paint Order

HTML paints in fixed order (background, float, foreground, outline).

SVG respects `paint-order` property:
- Default: fill, stroke, markers
- Can be reordered: stroke, fill, markers

### 4. Effects Application

| Effect | HTML | SVG |
|--------|------|-----|
| Filters | PaintLayerPainter | ScopedSVGPaintState |
| Masks | PaintLayerPainter | SVGMaskPainter (inline) |
| Clip paths | PaintLayerPainter | ClipPathClipper (inline) |
| Opacity | Effect property | Effect property |

### 5. Context Paints (SVG-specific)

SVG supports `context-fill` and `context-stroke` which inherit paint from ancestor `<use>` elements:
- Not available in HTML
- Managed via `SvgContextPaints` struct
- Transform-aware for gradients/patterns

### 6. Cull Rect Handling

HTML: Generally uses cull rect optimization.

SVG: More conservative:
- Disabled for hidden containers (resources)
- Disabled if transform-related operations
- Disabled for pixel-moving filters
- Uses infinite cull rect when optimization not possible
