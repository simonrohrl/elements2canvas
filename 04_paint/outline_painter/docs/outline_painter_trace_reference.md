# OutlinePainter Trace Reference

## Paint Phase Trigger

### How Outline Painting is Triggered

From `paint_layer_painter.cc`, outline painting is triggered via the `PaintPhase::kSelfOutlineOnly` phase:

```cpp
// Lines 1097-1103 in paint_layer_painter.cc
// Outline always needs to be painted even if we have no visible content.
bool should_paint_self_outline =
    is_self_painting_layer && object.StyleRef().HasOutline();

bool is_video = IsA<LayoutVideo>(object);
if (!is_video && should_paint_self_outline) {
  PaintWithPhase(PaintPhase::kSelfOutlineOnly, context, paint_flags);
}
```

For video elements, outlines are painted later (lines 1123-1127) to prevent being obscured by video controls.

Additionally, descendant outlines are painted during `PaintPhase::kDescendantOutlinesOnly` (lines 1378-1381):
```cpp
if (RuntimeEnabledFeatures::PaintUnderInvalidationCheckingEnabled() ||
    paint_layer_.NeedsPaintPhaseDescendantOutlines()) {
  PaintWithPhase(PaintPhase::kDescendantOutlinesOnly, context, paint_flags);
}
```

## Main Entry Point: PaintOutlineRects()

### Location
`outline_painter.cc` lines 895-949

### Function Signature
```cpp
void OutlinePainter::PaintOutlineRects(
    const PaintInfo& paint_info,
    const DisplayItemClient& client,
    const Vector<PhysicalRect>& outline_rects,
    const LayoutObject::OutlineInfo& info,
    const ComputedStyle& style)
```

### Parameters
| Parameter | Type | Description |
|-----------|------|-------------|
| `paint_info` | `const PaintInfo&` | Current paint context and phase |
| `client` | `const DisplayItemClient&` | Display item identifier |
| `outline_rects` | `const Vector<PhysicalRect>&` | Rects to outline |
| `info` | `const LayoutObject::OutlineInfo&` | Contains offset and width |
| `style` | `const ComputedStyle&` | CSS computed style |

### Line-by-Line Trace

```cpp
// Line 901: Early assertion
DCHECK(style.HasOutline());
DCHECK(!outline_rects.empty());

// Lines 904-906: Check for cached drawing
if (DrawingRecorder::UseCachedDrawingIfPossible(paint_info.context, client,
                                                paint_info.phase))
  return;

// Lines 908-920: Pixel snap rects and compute union
Vector<gfx::Rect> pixel_snapped_outline_rects;
std::optional<gfx::Rect> united_outline_rect;
for (auto& r : outline_rects) {
  gfx::Rect pixel_snapped_rect = ToPixelSnappedRect(r);
  // Keep empty rect for normal outline, but not for focus rings.
  if (!pixel_snapped_rect.IsEmpty() || !style.OutlineStyleIsAuto()) {
    pixel_snapped_outline_rects.push_back(pixel_snapped_rect);
    if (!united_outline_rect)
      united_outline_rect = pixel_snapped_rect;
    else
      united_outline_rect->UnionEvenIfEmpty(pixel_snapped_rect);
  }
}
if (pixel_snapped_outline_rects.empty())
  return;

// Lines 924-927: Calculate visual rect with outset
gfx::Rect visual_rect = *united_outline_rect;
visual_rect.Outset(OutlineOutsetExtent(style, info));
DrawingRecorder recorder(paint_info.context, client, paint_info.phase,
                         visual_rect);

// Lines 929-934: Focus ring path (outline-style: auto)
if (style.OutlineStyleIsAuto()) {
  auto corner_radii = GetFocusRingCornerRadii(style, outline_rects[0], info);
  PaintFocusRing(paint_info.context, pixel_snapped_outline_rects, style,
                 corner_radii, info);
  return;
}

// Lines 936-943: Single rect optimization
if (*united_outline_rect == pixel_snapped_outline_rects[0]) {
  gfx::Outsets offset =
      AdjustedOutlineOffset(*united_outline_rect, info.offset);
  BoxBorderPainter::PaintSingleRectOutline(
      paint_info.context, style, outline_rects[0], info.width,
      PhysicalBoxStrut::FromInts(offset.top(), offset.right(),
                                 offset.bottom(), offset.left()));
  return;
}

// Lines 946-948: Complex multi-rect outline
ComplexOutlinePainter(paint_info.context, pixel_snapped_outline_rects,
                      outline_rects[0], style, info)
    .Paint();
```

## Outline Offset Handling

### AdjustedOutlineOffset()
`outline_painter.cc` lines 77-80

Negative outline-offset must not make the outline smaller than twice the outline-width:

```cpp
gfx::Outsets AdjustedOutlineOffset(const gfx::Rect& rect, int offset) {
  return gfx::Outsets::VH(std::max(offset, -rect.height() / 2),
                          std::max(offset, -rect.width() / 2));
}
```

This implements the CSS spec requirement: https://drafts.csswg.org/css-ui/#outline-offset

### Focus Ring Offset
`outline_painter.cc` lines 57-72

Focus rings have special offset calculation that considers border width:

```cpp
int FocusRingOffset(const ComputedStyle& style,
                    const LayoutObject::OutlineInfo& info) {
  DCHECK(style.OutlineStyleIsAuto());
  // How much space the focus ring would like to take from the actual border.
  const float max_inside_border_width =
      ui::NativeTheme::AdjustBorderWidthByZoom(1.0f, style.EffectiveZoom());
  int offset = info.offset;
  // Focus ring is dependent on whether the border is large enough
  float min_border_width =
      std::min({style.BorderTopWidth(), style.BorderBottomWidth(),
                style.BorderLeftWidth(), style.BorderRightWidth()});
  if (min_border_width >= max_inside_border_width)
    offset -= max_inside_border_width;
  return offset;
}
```

## Outline Style Variations

### Style Enum Values
| Value | Rendering |
|-------|-----------|
| `kSolid` | Single solid fill |
| `kDouble` | Two parallel lines with gap |
| `kDotted` | Dotted line stroke |
| `kDashed` | Dashed line stroke |
| `kGroove` | 3D groove (light top-left, dark bottom-right) |
| `kRidge` | 3D ridge (dark top-left, light bottom-right) |
| `kInset` | 3D inset effect |
| `kOutset` | 3D outset effect |
| `kAuto` | Focus ring (platform-specific) |

### Style Fallbacks
`outline_painter.cc` lines 448-455 (ComplexOutlinePainter constructor):

```cpp
if (width_ <= 2 && outline_style_ == EBorderStyle::kDouble) {
  // Double becomes solid when too thin
  outline_style_ = EBorderStyle::kSolid;
} else if (width_ == 1 && (outline_style_ == EBorderStyle::kRidge ||
                           outline_style_ == EBorderStyle::kGroove)) {
  // Ridge/Groove become solid blend when 1px
  outline_style_ = EBorderStyle::kSolid;
  color_ = Color::FromColorMix(Color::ColorSpace::kSRGB, std::nullopt,
                               color_, color_.Dark(), 0.5f, 1.0f);
}
```

## Key Differences: Outline vs Border

### Layout Impact
- **Border**: Affects element size and layout
- **Outline**: Does not affect layout, drawn outside border-box

### Code Evidence
From `box_border_painter.h` lines 28-36 vs `outline_painter.cc` lines 938-942:

Border uses element's border_rect directly:
```cpp
BoxBorderPainter(context, border_rect, style, bleed_avoidance, sides_to_include)
```

Outline expands with offset:
```cpp
gfx::Outsets offset = AdjustedOutlineOffset(*united_outline_rect, info.offset);
BoxBorderPainter::PaintSingleRectOutline(
    paint_info.context, style, outline_rects[0], info.width,
    PhysicalBoxStrut::FromInts(offset.top(), offset.right(),
                               offset.bottom(), offset.left()));
```

### Uniformity
- **Border**: Can have different width/color/style per side
- **Outline**: Same on all sides (single width, color, style)

### Corner Radius Handling
Both follow the element's border-radius, but outline adjusts for offset:

```cpp
// outline_painter.cc lines 202-208
FloatRoundedRect::Radii ComputeCornerRadii(
    const ComputedStyle& style,
    const PhysicalRect& reference_border_rect,
    float offset) {
  return ContouredBorderGeometry::PixelSnappedContouredBorderWithOutsets(
             style, reference_border_rect, PhysicalBoxStrut(LayoutUnit(offset)))
      .GetRadii();
}
```

## Graphics Context Draw Operations

### Solid Outline
```cpp
// Lines 487-491
case EBorderStyle::kSolid:
  context_.FillRect(
      gfx::SkRectToRectF(outer_path.getBounds()),
      PaintAutoDarkMode(style_, DarkModeFilter::ElementRole::kBackground));
  break;
```

### Double Outline
```cpp
// Lines 519-537 (PaintDoubleOutline)
context_.FillPath(inner_third_path, auto_dark_mode);
context_.ClipPath(MakeClipOutPath(outer_third_path), kAntiAliased);
context_.FillRect(gfx::SkRectToRectF(right_angle_outer_path_.getBounds()),
                  auto_dark_mode);
```

### Dotted/Dashed Outline
```cpp
// Lines 540-573 (PaintDottedOrDashedOutline)
context_.SetStrokeColor(color_);
// For rounded:
context_.SetStroke(stroke_data);
context_.StrokePath(path, auto_dark_mode);
// For right-angle:
PaintStraightEdge(line, styled_stroke, auto_dark_mode);
```

### Groove/Ridge/Inset/Outset Outline
```cpp
// Lines 597-601 (PaintInsetOrOutsetOutline)
context_.SetStrokeColor(color_);
PaintTopLeftOrBottomRight(center_path, !is_inset);  // Light edges
context_.SetStrokeColor(color_.Dark());
PaintTopLeftOrBottomRight(center_path, is_inset);   // Dark edges
```

### Focus Ring
```cpp
// Lines 820-822 (PaintSingleFocusRing)
context.DrawFocusRingRect(
    SkRRect(FloatRoundedRect(gfx::SkRectToRectF(rect), corner_radii)),
    color, width, auto_dark_mode);

// Line 840-841
context.DrawFocusRingPath(path, color, width, *corner_radius, auto_dark_mode);
```

## Path Construction

### Right-Angle Path from Rects
`outline_painter.cc` lines 85-99:

```cpp
bool ComputeRightAnglePath(SkPath& path,
                           const Vector<gfx::Rect>& rects,
                           int outline_offset,
                           int additional_outset) {
  DCHECK_GE(additional_outset, 0);
  SkRegion region;
  for (auto& r : rects) {
    gfx::Rect rect = r;
    rect.Outset(AdjustedOutlineOffset(rect, outline_offset));
    rect.Outset(additional_outset);
    region.op(gfx::RectToSkIRect(rect), SkRegion::kUnion_Op);
  }
  path = region.getBoundaryPath();
  return !path.isEmpty();
}
```

### Adding Corner Radii
`outline_painter.cc` lines 293-320 (AddCornerRadiiToPath):

Uses conic arcs (weight = 1/sqrt(2)) to create 90-degree rounded corners:
```cpp
constexpr float kCornerConicWeight = 0.707106781187;
path.conicTo(lines[i].start, new_lines[i].start, kCornerConicWeight);
```

## OutlineInfo Structure

From `layout_object.h`:
```cpp
struct OutlineInfo {
  int offset;  // outline-offset CSS property
  int width;   // outline-width CSS property
};
```

Used throughout outline painting to determine positioning and thickness.
