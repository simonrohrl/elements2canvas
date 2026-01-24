# Border Painter Trace Reference

## Entry Point: BoxBorderPainter::PaintBorder()

**File:** `box_border_painter.h:28-36`

```cpp
static void PaintBorder(GraphicsContext& context,
                        const PhysicalRect& border_rect,
                        const ComputedStyle& style,
                        BackgroundBleedAvoidance bleed_avoidance,
                        PhysicalBoxSides sides_to_include) {
    BoxBorderPainter(context, border_rect, style, bleed_avoidance,
                     sides_to_include)
        .Paint();
}
```

**Parameters:**
- `context`: GraphicsContext for drawing
- `border_rect`: The border-box rectangle
- `style`: ComputedStyle with all border properties
- `bleed_avoidance`: Background bleed strategy (kBackgroundBleedNone, kBackgroundBleedClipOnly, kBackgroundBleedClipLayer)
- `sides_to_include`: Which sides to paint (for table cells, etc.)

---

## Constructor: BoxBorderPainter()

**File:** `box_border_painter.cc:1315-1357`

### Steps:

1. **Extract border edges** (line 1333)
   ```cpp
   style.GetBorderEdgeInfo(edges_, sides_to_include);
   ```
   - Populates `BorderEdgeArray edges_` with 4 edges (top, right, bottom, left)

2. **Compute border properties** (line 1334)
   ```cpp
   ComputeBorderProperties();
   ```
   - Sets: `visible_edge_count_`, `first_visible_edge_`, `visible_edge_set_`
   - Determines: `is_uniform_style_`, `is_uniform_width_`, `is_uniform_color_`
   - Sets `has_transparency_` if any edge color is not fully opaque

3. **Compute outer contoured rect** (line 1340-1341)
   ```cpp
   outer_ = ContouredBorderGeometry::PixelSnappedContouredBorder(
       style_, border_rect, sides_to_include);
   ```

4. **Compute inner contoured rect** (line 1342-1343)
   ```cpp
   inner_ = ContouredBorderGeometry::PixelSnappedContouredInnerBorder(
       style_, border_rect, sides_to_include);
   ```

5. **Clamp widths** (lines 1349-1352)
   ```cpp
   Edge(BoxSide::kTop).ClampWidth(max_height);
   Edge(BoxSide::kRight).ClampWidth(max_width);
   // etc.
   ```

6. **Set rounded flag** (line 1354)
   ```cpp
   is_rounded_ = outer_.IsRounded();
   ```

---

## Paint(): Main Painting Logic

**File:** `box_border_painter.cc:1439-1463`

### Flow:

```cpp
void BoxBorderPainter::Paint() const {
    // 1. Early out if nothing to paint
    if (!visible_edge_count_ || outer_.Rect().IsEmpty())
        return;

    // 2. Try fast path
    if (PaintBorderFastPath())
        return;

    // 3. Slow path: complex border
    bool clip_to_outer_border = outer_.IsRounded();
    GraphicsContextStateSaver state_saver(context_, clip_to_outer_border);

    if (clip_to_outer_border) {
        // Clip to outer rounded rect
        if (!BleedAvoidanceIsClipping(bleed_avoidance_)) {
            ClipContouredRect(outer_);
        }
        // Exclude inner rounded rect
        if (inner_.IsRenderable() && !inner_.IsEmpty()) {
            ClipOutContouredRect(inner_);
        }
    }

    // 4. Create opacity groups and paint
    const ComplexBorderInfo border_info(*this);
    PaintOpacityGroup(border_info, 0, 1);
}
```

---

## PaintBorderFastPath()

**File:** `box_border_painter.cc:1255-1313`

### Decision Points:

| Condition | Line | Result |
|-----------|------|--------|
| `!is_uniform_color_` | 1256 | Slow path |
| `!is_uniform_style_` | 1256 | Slow path |
| `!inner_.IsRenderable()` | 1256 | Slow path |
| `!inner_.HasRoundCurvature()` | 1257 | Slow path |
| Style not SOLID/DOUBLE | 1261-1263 | Slow path |

### Fast Path Cases:

1. **Uniform solid rectangular** (lines 1267-1271)
   ```cpp
   if (is_uniform_width_ && !outer_.IsRounded()) {
       DrawSolidBorderRect(context_, gfx::ToRoundedRect(outer_.Rect()),
                           FirstEdge().Width(), FirstEdge().GetColor(),
                           PaintAutoDarkMode(style_, element_role_));
   }
   ```

2. **Uniform solid rounded** (lines 1273-1278)
   ```cpp
   DrawBleedAdjustedDRRect(context_, bleed_avoidance_,
                           outer_.AsRoundedRect(), inner_.AsRoundedRect(),
                           FirstEdge().GetColor(),
                           PaintAutoDarkMode(style_, element_role_));
   ```

3. **Uniform double** (lines 1280-1283)
   ```cpp
   DCHECK(FirstEdge().BorderStyle() == EBorderStyle::kDouble);
   DrawDoubleBorder();
   ```

4. **Translucent solid rectangular partial** (lines 1290-1310)
   - Merges visible side rectangles into a single path
   - Uses `PathBuilder` to create compound path
   - Fills with single color

---

## ComplexBorderInfo Construction

**File:** `box_border_painter.cc:1143-1215`

Groups edges by opacity for correct transparency handling:

1. **Collect visible sides** (lines 1150-1156)
   ```cpp
   for (unsigned i = border_painter.first_visible_edge_; i < 4; ++i) {
       BoxSide side = static_cast<BoxSide>(i);
       if (IncludesEdge(border_painter.visible_edge_set_, side))
           sorted_sides.push_back(side);
   }
   ```

2. **Sort by paint order** (lines 1161-1180)
   Priority: alpha < style_priority < side_priority

3. **Build opacity groups** (lines 1189-1214)
   Groups sides with same alpha together

---

## PaintOpacityGroup()

**File:** `box_border_painter.cc:1509-1567`

### Algorithm:

1. Reverse iterate over groups (decreasing opacity)
2. Create transparency layer if needed:
   ```cpp
   if (needs_layer) {
       context_.BeginLayer(group.alpha / effective_opacity);
       effective_opacity = group.alpha;
       paint_alpha = 1.0f;  // Draw opaque inside layer
   }
   ```
3. Recursively process nested groups
4. Paint sides in current group:
   ```cpp
   for (BoxSide side : group.sides) {
       PaintSide(border_info, side, paint_alpha, completed_edges);
       completed_edges |= EdgeFlagForSide(side);
   }
   ```
5. End layer if created

---

## PaintSide()

**File:** `box_border_painter.cc:1569-1649`

### Per-Side Logic:

For each side (Top, Right, Bottom, Left):

1. **Determine if curved** (e.g., for Top, lines 1585-1589):
   ```cpp
   const bool is_curved =
       is_rounded_ && (BorderStyleHasInnerDetail(edge.BorderStyle()) ||
                       !inner_.HasRoundCurvature() ||
                       BorderWillArcInnerEdge(inner_.GetRadii().TopLeft(),
                                              inner_.GetRadii().TopRight()));
   ```

2. **Set side_rect** (if not curved):
   ```cpp
   if (!is_curved) {
       side_rect.set_height(edge.Width());  // For top edge
   }
   ```

3. **Call PaintOneBorderSide**:
   ```cpp
   PaintOneBorderSide(side_rect, BoxSide::kTop, BoxSide::kLeft,
                      BoxSide::kRight, side_type, color, completed_edges);
   ```

---

## PaintOneBorderSide()

**File:** `box_border_painter.cc:1696-1751`

### Curved Side:

```cpp
if (side_type == kCurved) {
    MiterType miter1 = ColorsMatchAtCorner(side, adjacent_side1) ? kHardMiter : kSoftMiter;
    MiterType miter2 = ColorsMatchAtCorner(side, adjacent_side2) ? kHardMiter : kSoftMiter;

    GraphicsContextStateSaver state_saver(context_);
    ClipBorderSidePolygon(side, miter1, miter2);

    if (!inner_.IsRenderable()) {
        ContouredRect adjusted_inner_rect = CalculateAdjustedInnerBorder(inner_, side);
        if (!adjusted_inner_rect.IsEmpty()) {
            context_.ClipOutContouredRect(adjusted_inner_rect);
        }
    }

    DrawCurvedBoxSide(edge_to_render.Width(), stroke_thickness, side, color,
                      edge_to_render.BorderStyle());
}
```

### Straight Side:

```cpp
else {
    MiterType miter1 = ComputeMiter(side, adjacent_side1, completed_edges);
    MiterType miter2 = ComputeMiter(side, adjacent_side2, completed_edges);
    bool should_clip = MitersRequireClipping(miter1, miter2, edge_to_render.BorderStyle());

    GraphicsContextStateSaver clip_state_saver(context_, should_clip);
    if (should_clip) {
        ClipBorderSidePolygon(side, miter1, miter2);
        miter1 = miter2 = kNoMiter;  // Miters applied via clip
    }

    DrawLineForBoxSide(context_, side_rect.x(), side_rect.y(),
                       side_rect.right(), side_rect.bottom(), side, color,
                       edge_to_render.BorderStyle(),
                       miter1 != kNoMiter ? adjacent_edge1.Width() : 0,
                       miter2 != kNoMiter ? adjacent_edge2.Width() : 0,
                       PaintAutoDarkMode(style_, element_role_));
}
```

---

## DrawLineForBoxSide()

**File:** `box_border_painter.cc:900-958`

### Style Switch:

```cpp
style = BorderEdge::EffectiveStyle(style, thickness);

switch (style) {
    case EBorderStyle::kDotted:
    case EBorderStyle::kDashed:
        DrawDashedOrDottedBoxSide(...);
        break;
    case EBorderStyle::kDouble:
        DrawDoubleBoxSide(...);
        break;
    case EBorderStyle::kRidge:
    case EBorderStyle::kGroove:
        DrawRidgeOrGrooveBoxSide(...);
        break;
    case EBorderStyle::kInset:
    case EBorderStyle::kOutset:
        color = CalculateInsetOutsetColor(DarkenBoxSide(side, style), color);
        [[fallthrough]];
    case EBorderStyle::kSolid:
        DrawSolidBoxSide(...);
        break;
}
```

---

## DrawCurvedBoxSide()

**File:** `box_border_painter.cc:1753-1793`

### Style Switch:

```cpp
switch (border_style) {
    case EBorderStyle::kDotted:
    case EBorderStyle::kDashed:
        DrawCurvedDashedDottedBoxSide(border_thickness, stroke_thickness, color, border_style);
        return;
    case EBorderStyle::kDouble:
        DrawCurvedDoubleBoxSide(color);
        return;
    case EBorderStyle::kRidge:
    case EBorderStyle::kGroove:
        DrawCurvedRidgeGrooveBoxSide(side, color, border_style);
        return;
    case EBorderStyle::kInset:
    case EBorderStyle::kOutset:
        color = CalculateInsetOutsetColor(DarkenBoxSide(side, border_style), color);
        [[fallthrough]];
    case EBorderStyle::kSolid:
        break;
}

// Solid/Inset/Outset: simple fill
context_.SetFillColor(color);
context_.FillRect(gfx::ToRoundedRect(outer_.Rect()), PaintAutoDarkMode(style_, element_role_));
```

---

## Key Helper Functions

### ComputeMiter()

**File:** `box_border_painter.cc:1651-1680`

Returns `MiterType`:
- `kNoMiter`: Adjacent edge will overdraw, no miter needed
- `kSoftMiter`: Color transition requires anti-aliased miter
- `kHardMiter`: Same color, style requires non-AA miter

### ColorsMatchAtCorner()

**File:** `box_border_painter.cc:2453-2463`

```cpp
bool BoxBorderPainter::ColorsMatchAtCorner(BoxSide side, BoxSide adjacent_side) const {
    if (!Edge(adjacent_side).ShouldRender())
        return false;
    if (!Edge(side).SharesColorWith(Edge(adjacent_side)))
        return false;
    return !BorderStyleHasUnmatchedColorsAtCorner(Edge(side).BorderStyle(), side, adjacent_side);
}
```

### CalculateInsetOutsetColor()

**File:** `box_border_painter.cc:616-643`

Computes darkened/lightened color for 3D border effects:
- Inset: Top/Left darkened, Bottom/Right original or lightened
- Outset: Top/Left original or lightened, Bottom/Right darkened

---

## Graphics Context Draw Operations (Terminal Nodes)

| Function | File:Line | GraphicsContext Method |
|----------|-----------|----------------------|
| DrawSolidBorderRect | 261-279 | `context.StrokeRect()` |
| DrawBleedAdjustedDRRect | 281-318 | `context.FillDRRect()` or `context.DrawPath()` |
| DrawDashedOrDottedBoxSide | 561-597 | `DrawLineWithStyle()` -> `context.DrawLine()` |
| DrawDoubleBoxSide | 645-750 | `context.FillRect()` x2 |
| DrawRidgeOrGrooveBoxSide | 752-822 | `DrawLineForBoxSide()` x2 |
| DrawSolidBoxSide | 841-898 | `context.FillRect()` or `FillQuad()` |
| DrawCurvedDashedDottedBoxSide | 1795-1829 | `context.StrokePath()` |
| DrawCurvedDoubleBoxSide | 1831-1867 | `context.FillRect()` x2 with clipping |
| DrawCurvedRidgeGrooveBoxSide | 1869-1897 | `context.FillRect()` x2 |
| ClipPolygon | 389-398 | `context.ClipPath()` |
| ClipContouredRect | 1431-1433 | `context.ClipContouredRect()` |
| ClipOutContouredRect | 1435-1437 | `context.ClipOutContouredRect()` |

---

## ContouredBorderGeometry Functions

**File:** `contoured_border_geometry.cc`

| Function | Line | Purpose |
|----------|------|---------|
| PixelSnappedContouredBorder | 178-185 | Outer border rect with radii |
| PixelSnappedContouredInnerBorder | 207-217 | Inner border rect (padding edge) |
| PixelSnappedContouredBorderWithOutsets | 219-255 | Border rect with custom outsets |
| ComputeContouredBorderFromStyle | 131-174 | Core computation with curvature |
| CalcRadiiFor | 27-47 | Compute radii from style |
| RadiiConstraintFactorForOppositeCorners | 113-129 | Handle opposite corner overlap |
