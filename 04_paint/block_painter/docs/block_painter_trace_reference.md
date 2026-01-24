# Block Painter Trace Reference

This document provides detailed line-by-line traces of key methods in the block painter group, with special focus on visibility checks, border radius handling, shadow rendering, and background painting phases.

## Entry Point: PaintLayerPainter::PaintFragmentWithPhase()

**File**: `paint_layer_painter.cc`
**Lines**: 1275-1320

```cpp
void PaintLayerPainter::PaintFragmentWithPhase(
    PaintPhase phase,
    const FragmentData& fragment_data,
    wtf_size_t fragment_data_idx,
    const PhysicalBoxFragment* physical_fragment,
    GraphicsContext& context,
    PaintFlags paint_flags) {
```

### Key Decision Points:

| Line | Condition | Action |
|------|-----------|--------|
| 1285-1288 | `cull_rect.Rect().IsEmpty()` | Return early if cull rect empty |
| 1291-1298 | `phase == PaintPhase::kMask` | Set up mask-specific properties |
| 1308-1309 | `physical_fragment != nullptr` | Use `BoxFragmentPainter` for NG fragments |
| 1310-1313 | `DynamicTo<LayoutInline>` | Use `InlineBoxFragmentPainter::PaintAllFragments` |
| 1314-1319 | else (legacy) | Use legacy `LayoutObject::Paint()` |

### Context Passed:

```cpp
PaintInfo paint_info(
    context, cull_rect, phase,
    paint_layer_.GetLayoutObject().ChildPaintBlockedByDisplayLock(),
    paint_flags);
```

---

## BoxFragmentPainter::Paint()

**File**: `box_fragment_painter.cc`
**Lines**: 568-587

### Visibility Checks:

| Line | Check | Purpose |
|------|-------|---------|
| 569-571 | `IsHiddenForPaint()` | Skip fragments marked hidden |
| 573-576 | `IsPaintedAtomically() && !HasSelfPaintingLayer()` | Route to atomic painting |

### Decision Flow:

```cpp
void BoxFragmentPainter::Paint(const PaintInfo& paint_info) {
  // Line 569: Hidden check
  if (GetPhysicalFragment().IsHiddenForPaint()) {
    return;
  }

  // Line 573-576: Atomic inline check
  if (GetPhysicalFragment().IsPaintedAtomically() &&
      !box_fragment_.HasSelfPaintingLayer() &&
      paint_info.phase != PaintPhase::kOverlayOverflowControls) {
    PaintAllPhasesAtomically(paint_info);  // Line 576
  }
  // Line 577-583: SVG foreign object
  else if (layout_object && layout_object->IsSVGForeignObject()) {
    ScopedSVGPaintState paint_state(...);
    PaintInternal(paint_info);
  }
  // Line 584-585: Normal path
  else {
    PaintInternal(paint_info);
  }
}
```

---

## BoxFragmentPainter::PaintInternal()

**File**: `box_fragment_painter.cc`
**Lines**: 589-752

### Scoped Paint State Setup:

```cpp
// Line 592-593: Create scoped paint state
STACK_UNINITIALIZED ScopedPaintState paint_state(box_fragment_, paint_info);
if (!ShouldPaint(paint_state))
  return;
```

### Key Decision Points:

| Line | Condition | Action |
|------|-----------|--------|
| 593-594 | `!ShouldPaint(paint_state)` | Return if outside cull rect or filters |
| 596-598 | `!IsFirstForNode() && !CanPaintMultipleFragments()` | Skip non-first fragments |
| 638-641 | `original_phase == PaintPhase::kOutline` | Switch to `kDescendantOutlinesOnly` |
| 640-641 | `ShouldPaintSelfBlockBackground()` | Switch to `kSelfBlockBackgroundOnly` |
| 648-649 | `GetBackgroundPaintLocation()` | Determine border-box vs contents-space |
| 652-654 | `box.ScrollsOverflow()` | Handle scrolling backgrounds |

### Background Painting Phases:

```cpp
// Lines 648-679: Two-pass background painting for scroll containers
auto paint_location = box.GetBackgroundPaintLocation();

// Pass 1: Border box space (lines 650-656)
if (!(paint_location & kBackgroundPaintInBorderBoxSpace))
  info.SetSkipsBackground(true);
bool has_overflow = box.ScrollsOverflow();
info.SetSkipsGapDecorations(has_overflow);
PaintObject(info, paint_offset);

// Pass 2: Contents space (lines 659-675)
if (box.ScrollsOverflow() ||
    (paint_location & kBackgroundPaintInContentsSpace)) {
  painted_overflow_controls = PaintOverflowControls(info, paint_offset);
  info.SetIsPaintingBackgroundInContentsSpace(true);
  PaintObject(info, paint_offset);
}
```

---

## BoxFragmentPainter::PaintObject()

**File**: `box_fragment_painter.cc`
**Lines**: 783-890

### Visibility Check:

```cpp
// Line 798-799
const ComputedStyle& style = fragment.Style();
const bool is_visible = IsVisibleToPaint(fragment, style);
```

### IsVisibleToPaint() Implementation:

**Lines 83-113** (anonymous namespace):
```cpp
inline bool IsVisibleToPaint(const PhysicalFragment& fragment,
                             const ComputedStyle& style) {
  // Line 85-86: Fragment-level hidden check
  if (fragment.IsHiddenForPaint())
    return false;

  // Lines 87-95: Visibility property check with table exception
  if (style.Visibility() != EVisibility::kVisible) {
    auto display = style.Display();
    // Hidden table rows/sections still paint into cells
    if (display != EDisplay::kTableRowGroup &&
        display != EDisplay::kTableRow && ...)
      return false;
  }

  // Lines 101-110: Check for hidden atomic inline in IFC
  if (fragment.IsAtomicInline() && fragment.HasSelfPaintingLayer()) {
    // Check fragment in inline formatting context
    ...
  }

  return true;
}
```

### Painting Dispatch:

```cpp
// Lines 800-808: Self block background
if (ShouldPaintSelfBlockBackground(paint_phase)) {
  if (is_visible) {
    PaintBoxDecorationBackground(paint_info, paint_offset,
                                 suppress_box_decoration_background);
  }
  if (paint_phase == PaintPhase::kSelfBlockBackgroundOnly) {
    return;
  }
}

// Lines 811-814: Mask phase
if (paint_phase == PaintPhase::kMask && is_visible) {
  PaintMask(paint_info, paint_offset);
  return;
}
```

---

## BoxFragmentPainter::PaintBoxDecorationBackgroundWithRectImpl()

**File**: `box_fragment_painter.cc`
**Lines**: 1554-1649

This is the core method that orchestrates background, shadow, and border painting.

### Shadow Rendering:

```cpp
// Lines 1565-1571: Normal (outer) box shadow
if (box_decoration_data.ShouldPaintShadow()) {
  std::optional<BorderShapeReferenceRects> border_shape_rects =
      ComputeBorderShapeReferenceRects(paint_rect, style, layout_object);
  PaintNormalBoxShadow(paint_info, paint_rect, style, border_shape_rects,
                       box_fragment_.SidesToInclude(),
                       !box_decoration_data.ShouldPaintBackground());
}
```

### Border Radius Handling for Clipping:

```cpp
// Lines 1573-1588: Background bleed avoidance with border radius
if (!box_decoration_data.IsPaintingBackgroundInContentsSpace() &&
    BleedAvoidanceIsClipping(box_decoration_data.GetBackgroundBleedAvoidance())) {
  state_saver.Save();

  // Line 1579-1580: Create rounded border for clipping
  ContouredRect border = ContouredBorderGeometry::PixelSnappedContouredBorder(
      style, paint_rect, box_fragment_.SidesToInclude());
  paint_info.context.ClipContouredRect(border);

  // Line 1583-1586: Begin layer for clip-layer bleed avoidance
  if (box_decoration_data.GetBackgroundBleedAvoidance() ==
      kBackgroundBleedClipLayer) {
    paint_info.context.BeginLayer();
    needs_end_layer = true;
  }
}
```

### Background Painting:

```cpp
// Lines 1595-1606: Theme and background painting
bool theme_painted = box_decoration_data.HasAppearance() &&
    !theme_painter.Paint(layout_box, paint_info, snapped_paint_rect);

if (!theme_painted) {
  if (box_decoration_data.ShouldPaintBackground()) {
    // Line 1597-1599: Main background painting
    PaintBackground(paint_info, paint_rect,
                    box_decoration_data.BackgroundColor(),
                    box_decoration_data.GetBackgroundBleedAvoidance());
  }
}
```

### Inset Shadow Rendering:

```cpp
// Lines 1608-1623: Inset (inner) box shadow
if (box_decoration_data.ShouldPaintShadow()) {
  if (layout_box.IsTableCell()) {
    // Table cells need special handling for collapsed borders
    PhysicalRect inner_rect = paint_rect;
    inner_rect.Contract(layout_box.BorderOutsets());
    BoxPainterBase::PaintInsetBoxShadowWithInnerRect(paint_info, inner_rect,
                                                     style);
  } else {
    PaintInsetBoxShadowWithBorderRect(paint_info, paint_rect, style,
                                      box_fragment_.SidesToInclude());
  }
}
```

### Border Painting:

```cpp
// Lines 1627-1644: Border painting
if (box_decoration_data.ShouldPaintBorder()) {
  if (!theme_painted) {
    theme_painted = box_decoration_data.HasAppearance() &&
        !LayoutTheme::GetTheme().Painter().PaintBorderOnly(...);
  }
  if (!theme_painted) {
    std::optional<BorderShapeReferenceRects> border_shape_rects =
        ComputeBorderShapeReferenceRects(paint_rect, style, layout_object);
    PaintBorder(*box_fragment_.GetLayoutObject(), document, generating_node,
                paint_info, paint_rect, style,
                box_decoration_data.GetBackgroundBleedAvoidance(),
                box_fragment_.SidesToInclude(),
                border_shape_rects ? &*border_shape_rects : nullptr);
  }
}
```

---

## BoxPainterBase::PaintNormalBoxShadow()

**File**: `box_painter_base.cc`
**Lines**: 253-363

### Border Radius Computation:

```cpp
// Lines 264-266: Compute contoured border for shadow clipping
ContouredRect border = ContouredBorderGeometry::PixelSnappedContouredBorder(
    style, paint_rect, sides_to_include);

bool has_border_radius = style.HasBorderRadius() && !style.HasBorderShape();
```

### Shadow Iteration:

```cpp
// Lines 275-363: Process each shadow (in reverse order for stacking)
const ShadowList* shadow_list = style.BoxShadow();
for (wtf_size_t i = shadow_list->Shadows().size(); i--;) {
  const ShadowData& shadow = shadow_list->Shadows()[i];

  // Line 277-278: Skip inset shadows
  if (shadow.Style() != ShadowStyle::kNormal)
    continue;

  // Lines 279-281: Skip fully obscured shadows
  if (ShadowIsFullyObscured(shadow))
    continue;
```

### Shadow Rendering with Border Radius:

```cpp
// Lines 349-362: Draw shadow with appropriate shape
if (has_border_radius) {
  ContouredRect rounded_fill_rect(
      FloatRoundedRect(fill_rect, border.GetRadii()),
      border.GetCornerCurvature());
  ApplySpreadToShadowShape(rounded_fill_rect, shadow.Spread());
  context.FillContouredRect(rounded_fill_rect, Color::kBlack, auto_dark_mode);
} else {
  fill_rect.Outset(shadow.Spread());
  context.FillRect(fill_rect, Color::kBlack, auto_dark_mode);
}
```

---

## InlineBoxFragmentPainterBase::PaintBoxDecorationBackground()

**File**: `inline_box_fragment_painter.cc`
**Lines**: 422-482

### Decoration Order:

```cpp
void InlineBoxFragmentPainterBase::PaintBoxDecorationBackground(
    BoxPainterBase& box_painter,
    const PaintInfo& paint_info,
    const PhysicalOffset& paint_offset,
    const PhysicalRect& adjusted_frame_rect,
    const BoxBackgroundPaintContext& bg_paint_context,
    bool object_has_multiple_boxes,
    PhysicalBoxSides sides_to_include) {

  // Line 431: 1. Normal (outer) box shadow
  PaintNormalBoxShadow(paint_info, line_style_, adjusted_frame_rect);

  // Lines 433-437: 2. Background fill layers
  Color background_color =
      line_style_.VisitedDependentColor(GetCSSPropertyBackgroundColor());
  PaintFillLayers(box_painter, paint_info, background_color,
                  line_style_.BackgroundLayers(), adjusted_frame_rect,
                  bg_paint_context, object_has_multiple_boxes);

  // Line 439: 3. Inset (inner) box shadow
  PaintInsetBoxShadow(paint_info, line_style_, adjusted_frame_rect);

  // Lines 441-481: 4. Border (with special handling for multi-box inlines)
  gfx::Rect adjusted_clip_rect;
  SlicePaintingType border_painting_type = GetBorderPaintType(
      adjusted_frame_rect, adjusted_clip_rect, object_has_multiple_boxes);

  switch (border_painting_type) {
    case kDontPaint:
      break;
    case kPaintWithoutClip:
      BoxPainterBase::PaintBorder(...);
      break;
    case kPaintWithClip:
      // For spanning inlines, clip and use continuous image strip
      paint_info.context.Clip(adjusted_clip_rect);
      BoxPainterBase::PaintBorder(..., image_strip_paint_rect, ...);
      break;
  }
}
```

---

## Hit Test with Border Radius

**File**: `box_fragment_painter.cc`
**Lines**: 2319-2325

```cpp
// Lines 2319-2325: Skip children if outside rounded border
if (!skip_children && style.HasBorderRadius()) {
  PhysicalRect bounds_rect(physical_offset, size);
  skip_children = !hit_test.location.Intersects(
      ContouredBorderGeometry::PixelSnappedContouredInnerBorder(
          style, bounds_rect));
}
```

---

## Key Helper Functions

### IsVisibleToPaint (Fragment)
**File**: `box_fragment_painter.cc`, Lines 83-113

### IsVisibleToHitTest
**File**: `box_fragment_painter.cc`, Lines 121-155

### ShouldPaintSelfBlockBackground()
**File**: (inline in paint_phase.h)
Returns true for: `kBlockBackground`, `kSelfBlockBackgroundOnly`

### ShouldPaintDescendantBlockBackgrounds()
**File**: (inline in paint_phase.h)
Returns true for: `kBlockBackground`, `kDescendantBlockBackgroundsOnly`

---

## Summary: Paint Order within a Box

1. **Normal (outer) box shadow** - Behind everything
2. **Background clip setup** - If border-radius with bleed avoidance
3. **Theme background** - Native form controls
4. **Background layers** - CSS backgrounds (in reverse layer order)
5. **Theme decorations** - Additional native styling
6. **Inset (inner) box shadow** - Inside border
7. **Border** - On top of background
8. **Layer cleanup** - End isolation layer if needed

## Summary: Visibility Checks

| Check | Location | Purpose |
|-------|----------|---------|
| `IsHiddenForPaint()` | Fragment | LineTruncator, display:none |
| `style.Visibility()` | ComputedStyle | visibility: hidden/collapse |
| `IsAtomicInline() + SelfPaintingLayer` | Special case | Hidden in IFC but self-painting |
| `ShouldPaint()` | ScopedPaintState | Cull rect intersection |
| `IsFirstForNode()` | Fragment | Avoid duplicate painting |
