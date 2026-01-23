// Copyright 2025 The Chromium Authors (adapted)
// Border Painter Implementation - Following Chromium's BoxBorderPainter logic

#include "border_painter.h"

#include <algorithm>
#include <cmath>

namespace border_painter {

namespace {

// Determine if border side should darken for inset/outset effects
// BorderStyleOutset darkens the bottom and right
// BorderStyleInset darkens the top and left
bool DarkenBoxSide(BoxSide side, EBorderStyle style) {
  return (side == BoxSide::kTop || side == BoxSide::kLeft) ==
         (style == EBorderStyle::kInset);
}

}  // namespace

// Analyze the border properties to determine rendering strategy
BorderPainter::BorderProperties BorderPainter::AnalyzeBorder(
    const BorderPaintInput& input) {
  BorderProperties props;

  // Get edge info for each side
  BorderEdge edges[4] = {
      GetEdge(input, BoxSide::kTop),
      GetEdge(input, BoxSide::kRight),
      GetEdge(input, BoxSide::kBottom),
      GetEdge(input, BoxSide::kLeft)
  };

  for (unsigned i = 0; i < 4; ++i) {
    const BorderEdge& edge = edges[i];

    if (!edge.ShouldRender()) {
      if (edge.PresentButInvisible()) {
        props.is_uniform_width = false;
        props.is_uniform_color = false;
      }
      continue;
    }

    if (!edge.color.IsOpaque()) {
      props.has_transparency = true;
    }

    props.visible_edge_count++;

    if (props.visible_edge_count == 1) {
      props.first_visible_edge = i;
      continue;
    }

    const BorderEdge& first = edges[props.first_visible_edge];
    props.is_uniform_style &= edge.style == first.style;
    props.is_uniform_width &= edge.width == first.width;
    props.is_uniform_color &= edge.SharesColorWith(first);
  }

  props.is_rounded = HasBorderRadius(input);
  return props;
}

// Get edge information for a specific side
BorderEdge BorderPainter::GetEdge(const BorderPaintInput& input, BoxSide side) {
  BorderEdge edge;
  switch (side) {
    case BoxSide::kTop:
      edge.width = input.border_widths.top;
      edge.color = input.border_colors.top;
      edge.style = input.border_styles.has_value()
                       ? input.border_styles->top
                       : EBorderStyle::kSolid;
      break;
    case BoxSide::kRight:
      edge.width = input.border_widths.right;
      edge.color = input.border_colors.right;
      edge.style = input.border_styles.has_value()
                       ? input.border_styles->right
                       : EBorderStyle::kSolid;
      break;
    case BoxSide::kBottom:
      edge.width = input.border_widths.bottom;
      edge.color = input.border_colors.bottom;
      edge.style = input.border_styles.has_value()
                       ? input.border_styles->bottom
                       : EBorderStyle::kSolid;
      break;
    case BoxSide::kLeft:
      edge.width = input.border_widths.left;
      edge.color = input.border_colors.left;
      edge.style = input.border_styles.has_value()
                       ? input.border_styles->left
                       : EBorderStyle::kSolid;
      break;
  }
  return edge;
}

PaintOpList BorderPainter::Paint(const BorderPaintInput& input) {
  PaintOpList ops;

  // Skip if not visible
  if (input.visibility != Visibility::kVisible) {
    return ops;
  }

  // Skip if no border
  if (input.border_widths.IsZero()) {
    return ops;
  }

  // Analyze border properties
  BorderProperties props = AnalyzeBorder(input);

  // No visible edges
  if (props.visible_edge_count == 0) {
    return ops;
  }

  // If render_hint is specified, follow it for verification
  if (input.render_hint == BorderRenderHint::kDrawLine) {
    // Force per-side stroked lines
    PaintSidesAsLines(input, props, ops);
    return ops;
  }

  if (input.render_hint == BorderRenderHint::kFilledThinRect) {
    // Force per-side filled rects
    PaintSidesAsFilledRects(input, props, ops);
    return ops;
  }

  if (input.render_hint == BorderRenderHint::kDoubleStroked) {
    // Force double border as 2 stroked rects
    PaintDoubleBorder(input, props, ops);
    return ops;
  }

  if (input.render_hint == BorderRenderHint::kDottedLines) {
    // Force dotted border as 4 stroked lines
    PaintDottedBorder(input, props, ops);
    return ops;
  }

  if (input.render_hint == BorderRenderHint::kGrooveRidge) {
    // Force groove/ridge border as paired thin rects
    PaintGrooveRidgeBorder(input, props, ops);
    return ops;
  }

  // Default behavior: try fast path first
  if (input.render_hint == BorderRenderHint::kAuto ||
      input.render_hint == BorderRenderHint::kStrokedRect) {
    if (PaintFastPath(input, props, ops)) {
      return ops;
    }
  }

  // Fall back to per-side painting
  PaintSides(input, props, ops);
  return ops;
}

// Fast path for uniform borders
// Based on BoxBorderPainter::PaintBorderFastPath()
bool BorderPainter::PaintFastPath(const BorderPaintInput& input,
                                  const BorderProperties& props,
                                  PaintOpList& ops) {
  // Must be uniform to use fast path
  if (!props.is_uniform_color || !props.is_uniform_style) {
    return false;
  }

  BorderEdge first_edge = GetEdge(input, static_cast<BoxSide>(props.first_visible_edge));

  // Only handle solid borders in fast path for now
  // (Chromium also handles double borders here)
  if (first_edge.style != EBorderStyle::kSolid) {
    return false;
  }

  // All 4 sides must be visible for the simplest fast path
  if (props.visible_edge_count != 4) {
    return false;
  }

  float stroke_width = first_edge.width;
  Color color = first_edge.color;

  // Uniform width + rectangular (no radius) => single stroked rect
  if (props.is_uniform_width && !props.is_rounded) {
    DrawRectOp op;
    op.rect = CalculateStrokeRect(input.geometry, stroke_width);
    op.flags = BuildStrokeFlags(color, stroke_width, first_edge.style);
    op.transform_id = input.state_ids.transform_id;
    op.clip_id = input.state_ids.clip_id;
    op.effect_id = input.state_ids.effect_id;
    ops.AddDrawRect(std::move(op));
    return true;
  }

  // Uniform width + rounded => single stroked rrect
  if (props.is_uniform_width && props.is_rounded) {
    DrawRRectOp op;
    op.rect = CalculateStrokeRect(input.geometry, stroke_width);
    op.radii = AdjustRadiiForStroke(*input.border_radii, stroke_width);
    op.flags = BuildStrokeFlags(color, stroke_width, first_edge.style);
    op.transform_id = input.state_ids.transform_id;
    op.clip_id = input.state_ids.clip_id;
    op.effect_id = input.state_ids.effect_id;
    ops.AddDrawRRect(std::move(op));
    return true;
  }

  // Non-uniform width => fill DRRect (outer - inner)
  if (props.is_rounded) {
    DrawDRRectOp op;
    op.outer_rect = {
        input.geometry.x,
        input.geometry.y,
        input.geometry.x + input.geometry.width,
        input.geometry.y + input.geometry.height
    };
    op.outer_radii = *input.border_radii;
    op.inner_rect = CalculateInnerRect(input.geometry, input.border_widths);
    op.inner_radii = AdjustRadiiForInner(*input.border_radii, input.border_widths);
    op.flags = BuildFillFlags(color);
    op.transform_id = input.state_ids.transform_id;
    op.clip_id = input.state_ids.clip_id;
    op.effect_id = input.state_ids.effect_id;
    ops.AddDrawDRRect(std::move(op));
    return true;
  }

  return false;
}

// Paint individual border sides (slow path)
void BorderPainter::PaintSides(const BorderPaintInput& input,
                               const BorderProperties& props,
                               PaintOpList& ops) {
  // For non-uniform borders, draw each side
  // This is a simplified version - Chromium has much more complex logic
  // for handling corners and miters

  for (auto side : {BoxSide::kTop, BoxSide::kRight, BoxSide::kBottom, BoxSide::kLeft}) {
    BorderEdge edge = GetEdge(input, side);
    if (!edge.ShouldRender()) {
      continue;
    }

    PaintSide(input, side, ops);
  }
}

// Paint all sides as stroked lines (for verification)
void BorderPainter::PaintSidesAsLines(const BorderPaintInput& input,
                                      const BorderProperties& props,
                                      PaintOpList& ops) {
  for (auto side : {BoxSide::kTop, BoxSide::kRight, BoxSide::kBottom, BoxSide::kLeft}) {
    BorderEdge edge = GetEdge(input, side);
    if (!edge.ShouldRender()) {
      continue;
    }
    PaintSideAsLine(input, side, ops);
  }
}

// Paint all sides as filled thin rects (for verification)
void BorderPainter::PaintSidesAsFilledRects(const BorderPaintInput& input,
                                            const BorderProperties& props,
                                            PaintOpList& ops) {
  for (auto side : {BoxSide::kTop, BoxSide::kRight, BoxSide::kBottom, BoxSide::kLeft}) {
    BorderEdge edge = GetEdge(input, side);
    if (!edge.ShouldRender()) {
      continue;
    }
    PaintSideAsFilledRect(input, side, ops);
  }
}

// Paint a single border side as filled thin rect
// Chromium uses this for thin borders (typically < 10px)
void BorderPainter::PaintSideAsFilledRect(const BorderPaintInput& input,
                                          BoxSide side,
                                          PaintOpList& ops) {
  BorderEdge edge = GetEdge(input, side);
  Color color = CalculateBorderColor(edge.color, side, edge.style);
  const RectF& g = input.geometry;

  DrawRectOp op;

  switch (side) {
    case BoxSide::kTop:
      op.rect = {g.x, g.y, g.x + g.width, g.y + edge.width};
      break;
    case BoxSide::kRight:
      op.rect = {g.x + g.width - edge.width, g.y, g.x + g.width, g.y + g.height};
      break;
    case BoxSide::kBottom:
      op.rect = {g.x, g.y + g.height - edge.width, g.x + g.width, g.y + g.height};
      break;
    case BoxSide::kLeft:
      op.rect = {g.x, g.y, g.x + edge.width, g.y + g.height};
      break;
  }

  op.flags = BuildFillFlags(color);
  op.transform_id = input.state_ids.transform_id;
  op.clip_id = input.state_ids.clip_id;
  op.effect_id = input.state_ids.effect_id;

  ops.AddDrawRect(std::move(op));
}

// Paint a single border side as stroked line
// Chromium uses this for thicker borders (typically >= 10px)
void BorderPainter::PaintSideAsLine(const BorderPaintInput& input,
                                    BoxSide side,
                                    PaintOpList& ops) {
  BorderEdge edge = GetEdge(input, side);
  Color color = CalculateBorderColor(edge.color, side, edge.style);

  DrawLineOp op;
  float x1, y1, x2, y2;

  // Calculate line endpoints (center of the border edge)
  float half_width = edge.width / 2.0f;
  const RectF& g = input.geometry;

  switch (side) {
    case BoxSide::kTop:
      x1 = g.x;
      y1 = g.y + half_width;
      x2 = g.x + g.width;
      y2 = g.y + half_width;
      break;
    case BoxSide::kRight:
      x1 = g.x + g.width - half_width;
      y1 = g.y;
      x2 = g.x + g.width - half_width;
      y2 = g.y + g.height;
      break;
    case BoxSide::kBottom:
      x1 = g.x;
      y1 = g.y + g.height - half_width;
      x2 = g.x + g.width;
      y2 = g.y + g.height - half_width;
      break;
    case BoxSide::kLeft:
      x1 = g.x + half_width;
      y1 = g.y;
      x2 = g.x + half_width;
      y2 = g.y + g.height;
      break;
  }

  op.x0 = x1;
  op.y0 = y1;
  op.x1 = x2;
  op.y1 = y2;
  op.flags = BuildStrokeFlags(color, edge.width, edge.style);
  op.transform_id = input.state_ids.transform_id;
  op.clip_id = input.state_ids.clip_id;
  op.effect_id = input.state_ids.effect_id;

  ops.AddDrawLine(std::move(op));
}

// Paint a single border side - chooses between filled rect and stroked line
void BorderPainter::PaintSide(const BorderPaintInput& input,
                              BoxSide side,
                              PaintOpList& ops) {
  BorderEdge edge = GetEdge(input, side);

  // Chromium uses filled thin rects for thin borders (< 10px)
  // and stroked lines for thicker borders
  // The threshold appears to be around 10px based on observed output
  if (edge.width < 10.0f) {
    PaintSideAsFilledRect(input, side, ops);
  } else {
    PaintSideAsLine(input, side, ops);
  }
}

// Calculate color for inset/outset/ridge/groove borders
Color BorderPainter::CalculateBorderColor(const Color& color, BoxSide side,
                                          EBorderStyle style) {
  if (style == EBorderStyle::kInset || style == EBorderStyle::kOutset) {
    bool darken = DarkenBoxSide(side, style);
    return darken ? color.Dark() : color;
  }

  if (style == EBorderStyle::kRidge || style == EBorderStyle::kGroove) {
    // Ridge/groove use similar logic but split the border
    bool darken = DarkenBoxSide(side,
        style == EBorderStyle::kGroove ? EBorderStyle::kInset : EBorderStyle::kOutset);
    return darken ? color.Dark() : color;
  }

  return color;
}

bool BorderPainter::HasBorderRadius(const BorderPaintInput& input) {
  if (!input.border_radii.has_value()) {
    return false;
  }
  return !IsZeroRadii(*input.border_radii);
}

BorderRadii BorderPainter::AdjustRadiiForStroke(const BorderRadii& radii,
                                                float stroke_width) {
  // The stroke is centered on the rect edge.
  // Inner radius = outer radius - strokeWidth/2
  // Clamp to 0 (can't have negative radius)
  BorderRadii adjusted;
  float adjustment = stroke_width / 2.0f;
  for (size_t i = 0; i < radii.size(); ++i) {
    adjusted[i] = std::max(0.0f, radii[i] - adjustment);
  }
  return adjusted;
}

std::array<float, 4> BorderPainter::CalculateStrokeRect(const RectF& geometry,
                                                        float stroke_width) {
  // The stroke is centered on the rect edge.
  // Inset by strokeWidth/2 from the outer edge to get the stroke center.
  float inset = stroke_width / 2.0f;
  return {
      geometry.x + inset,                    // left
      geometry.y + inset,                    // top
      geometry.x + geometry.width - inset,   // right
      geometry.y + geometry.height - inset   // bottom
  };
}

std::array<float, 4> BorderPainter::CalculateInnerRect(const RectF& geometry,
                                                       const BorderWidths& widths) {
  return {
      geometry.x + widths.left,                          // left
      geometry.y + widths.top,                           // top
      geometry.x + geometry.width - widths.right,        // right
      geometry.y + geometry.height - widths.bottom       // bottom
  };
}

BorderRadii BorderPainter::AdjustRadiiForInner(const BorderRadii& radii,
                                               const BorderWidths& widths) {
  // Inner radii = outer radii - border width (clamped to 0)
  // radii format: [tl_x, tl_y, tr_x, tr_y, br_x, br_y, bl_x, bl_y]
  BorderRadii adjusted;
  adjusted[0] = std::max(0.0f, radii[0] - widths.left);   // tl_x
  adjusted[1] = std::max(0.0f, radii[1] - widths.top);    // tl_y
  adjusted[2] = std::max(0.0f, radii[2] - widths.right);  // tr_x
  adjusted[3] = std::max(0.0f, radii[3] - widths.top);    // tr_y
  adjusted[4] = std::max(0.0f, radii[4] - widths.right);  // br_x
  adjusted[5] = std::max(0.0f, radii[5] - widths.bottom); // br_y
  adjusted[6] = std::max(0.0f, radii[6] - widths.left);   // bl_x
  adjusted[7] = std::max(0.0f, radii[7] - widths.bottom); // bl_y
  return adjusted;
}

DrawFlags BorderPainter::BuildStrokeFlags(const Color& color,
                                          float stroke_width,
                                          EBorderStyle style) {
  DrawFlags flags;
  flags.color = color;
  flags.style = PaintStyle::kStroke;
  flags.stroke_width = stroke_width;
  flags.stroke_cap = StrokeCap::kButt;
  flags.stroke_join = StrokeJoin::kMiter;

  // Set up dash pattern for dotted/dashed
  if (style == EBorderStyle::kDotted) {
    flags.dash_pattern.has_pattern = true;
    // Dots: square dots with gaps equal to stroke width
    flags.dash_pattern.intervals[0] = stroke_width;
    flags.dash_pattern.intervals[1] = stroke_width;
    flags.stroke_cap = StrokeCap::kRound;
  } else if (style == EBorderStyle::kDashed) {
    flags.dash_pattern.has_pattern = true;
    // Dashes: 3x width dash, 1x width gap
    flags.dash_pattern.intervals[0] = stroke_width * 3;
    flags.dash_pattern.intervals[1] = stroke_width;
  }

  return flags;
}

DrawFlags BorderPainter::BuildFillFlags(const Color& color) {
  DrawFlags flags;
  flags.color = color;
  flags.style = PaintStyle::kFill;
  return flags;
}

// Paint double border as 2 stroked rects
// For a double border of width W:
// - Each stroke line is ceil(W/3) thick
// - Outer rect: inset by sw/2
// - Inner rect: inset by W - sw/2
void BorderPainter::PaintDoubleBorder(const BorderPaintInput& input,
                                      const BorderProperties& props,
                                      PaintOpList& ops) {
  BorderEdge first_edge = GetEdge(input, static_cast<BoxSide>(props.first_visible_edge));
  float border_width = first_edge.width;
  Color color = first_edge.color;

  // Stroke width for double border: ceil(border_width / 3)
  float sw = std::ceil(border_width / 3.0f);
  float outer_inset = sw / 2.0f;
  float inner_inset = border_width - sw / 2.0f;

  // Outer stroked rect/rrect
  if (props.is_rounded && input.border_radii.has_value()) {
    DrawRRectOp outer_op;
    outer_op.rect = {
        input.geometry.x + outer_inset,
        input.geometry.y + outer_inset,
        input.geometry.x + input.geometry.width - outer_inset,
        input.geometry.y + input.geometry.height - outer_inset
    };
    outer_op.radii = AdjustRadiiForStroke(*input.border_radii, sw);
    outer_op.flags = BuildStrokeFlags(color, sw, EBorderStyle::kSolid);
    outer_op.transform_id = input.state_ids.transform_id;
    outer_op.clip_id = input.state_ids.clip_id;
    outer_op.effect_id = input.state_ids.effect_id;
    ops.AddDrawRRect(std::move(outer_op));

    // Inner stroked rrect
    DrawRRectOp inner_op;
    inner_op.rect = {
        input.geometry.x + inner_inset,
        input.geometry.y + inner_inset,
        input.geometry.x + input.geometry.width - inner_inset,
        input.geometry.y + input.geometry.height - inner_inset
    };
    inner_op.radii = AdjustRadiiForStroke(*input.border_radii, border_width + sw);
    inner_op.flags = BuildStrokeFlags(color, sw, EBorderStyle::kSolid);
    inner_op.transform_id = input.state_ids.transform_id;
    inner_op.clip_id = input.state_ids.clip_id;
    inner_op.effect_id = input.state_ids.effect_id;
    ops.AddDrawRRect(std::move(inner_op));
  } else {
    DrawRectOp outer_op;
    outer_op.rect = {
        input.geometry.x + outer_inset,
        input.geometry.y + outer_inset,
        input.geometry.x + input.geometry.width - outer_inset,
        input.geometry.y + input.geometry.height - outer_inset
    };
    outer_op.flags = BuildStrokeFlags(color, sw, EBorderStyle::kSolid);
    outer_op.transform_id = input.state_ids.transform_id;
    outer_op.clip_id = input.state_ids.clip_id;
    outer_op.effect_id = input.state_ids.effect_id;
    ops.AddDrawRect(std::move(outer_op));

    DrawRectOp inner_op;
    inner_op.rect = {
        input.geometry.x + inner_inset,
        input.geometry.y + inner_inset,
        input.geometry.x + input.geometry.width - inner_inset,
        input.geometry.y + input.geometry.height - inner_inset
    };
    inner_op.flags = BuildStrokeFlags(color, sw, EBorderStyle::kSolid);
    inner_op.transform_id = input.state_ids.transform_id;
    inner_op.clip_id = input.state_ids.clip_id;
    inner_op.effect_id = input.state_ids.effect_id;
    ops.AddDrawRect(std::move(inner_op));
  }
}

// Paint dotted border as 4 stroked lines with dash pattern
void BorderPainter::PaintDottedBorder(const BorderPaintInput& input,
                                      const BorderProperties& props,
                                      PaintOpList& ops) {
  // Dotted borders use stroked lines with dash pattern
  // Similar to regular lines but with dotted style
  for (auto side : {BoxSide::kTop, BoxSide::kRight, BoxSide::kBottom, BoxSide::kLeft}) {
    BorderEdge edge = GetEdge(input, side);
    if (!edge.ShouldRender()) {
      continue;
    }

    DrawLineOp op;
    float half_width = edge.width / 2.0f;
    const RectF& g = input.geometry;

    switch (side) {
      case BoxSide::kTop:
        op.x0 = g.x;
        op.y0 = g.y + half_width;
        op.x1 = g.x + g.width;
        op.y1 = g.y + half_width;
        break;
      case BoxSide::kRight:
        op.x0 = g.x + g.width - half_width;
        op.y0 = g.y;
        op.x1 = g.x + g.width - half_width;
        op.y1 = g.y + g.height;
        break;
      case BoxSide::kBottom:
        op.x0 = g.x;
        op.y0 = g.y + g.height - half_width;
        op.x1 = g.x + g.width;
        op.y1 = g.y + g.height - half_width;
        break;
      case BoxSide::kLeft:
        op.x0 = g.x + half_width;
        op.y0 = g.y;
        op.x1 = g.x + half_width;
        op.y1 = g.y + g.height;
        break;
    }

    op.flags = BuildStrokeFlags(edge.color, edge.width, EBorderStyle::kDotted);
    op.transform_id = input.state_ids.transform_id;
    op.clip_id = input.state_ids.clip_id;
    op.effect_id = input.state_ids.effect_id;

    ops.AddDrawLine(std::move(op));
  }
}

// Paint groove/ridge border as paired thin rects per side
// Groove: outer half is dark, inner half is light
// Ridge: outer half is light, inner half is dark
void BorderPainter::PaintGrooveRidgeBorder(const BorderPaintInput& input,
                                           const BorderProperties& props,
                                           PaintOpList& ops) {
  BorderEdge first_edge = GetEdge(input, static_cast<BoxSide>(props.first_visible_edge));
  float border_width = first_edge.width;
  Color color = first_edge.color;
  float half_width = border_width / 2.0f;

  // Determine if groove or ridge (affects which half is dark)
  bool is_groove = first_edge.style == EBorderStyle::kGroove;
  Color dark_color = color.Dark();
  Color light_color = color;

  const RectF& g = input.geometry;

  // For each side, draw 2 thin rects (outer half, inner half)
  for (auto side : {BoxSide::kTop, BoxSide::kRight, BoxSide::kBottom, BoxSide::kLeft}) {
    BorderEdge edge = GetEdge(input, side);
    if (!edge.ShouldRender()) {
      continue;
    }

    // Determine colors for outer/inner based on side and groove/ridge
    // Groove: T/L outer=dark, inner=light; B/R outer=light, inner=dark
    // Ridge: opposite
    bool is_top_or_left = (side == BoxSide::kTop || side == BoxSide::kLeft);
    Color outer_color = is_groove == is_top_or_left ? dark_color : light_color;
    Color inner_color = is_groove == is_top_or_left ? light_color : dark_color;

    DrawRectOp outer_op, inner_op;
    outer_op.flags = BuildFillFlags(outer_color);
    inner_op.flags = BuildFillFlags(inner_color);

    switch (side) {
      case BoxSide::kTop:
        // Outer: top strip
        outer_op.rect = {g.x, g.y, g.x + g.width, g.y + half_width};
        // Inner: second strip
        inner_op.rect = {g.x, g.y + half_width, g.x + g.width, g.y + border_width};
        break;
      case BoxSide::kBottom:
        // Outer: bottom outer strip (closer to edge)
        outer_op.rect = {g.x, g.y + g.height - half_width, g.x + g.width, g.y + g.height};
        // Inner: inner strip
        inner_op.rect = {g.x, g.y + g.height - border_width, g.x + g.width, g.y + g.height - half_width};
        break;
      case BoxSide::kRight:
        // Outer: right outer strip
        outer_op.rect = {g.x + g.width - half_width, g.y, g.x + g.width, g.y + g.height};
        // Inner: inner strip
        inner_op.rect = {g.x + g.width - border_width, g.y, g.x + g.width - half_width, g.y + g.height};
        break;
      case BoxSide::kLeft:
        // Outer: left outer strip
        outer_op.rect = {g.x, g.y, g.x + half_width, g.y + g.height};
        // Inner: inner strip
        inner_op.rect = {g.x + half_width, g.y, g.x + border_width, g.y + g.height};
        break;
    }

    outer_op.transform_id = input.state_ids.transform_id;
    outer_op.clip_id = input.state_ids.clip_id;
    outer_op.effect_id = input.state_ids.effect_id;
    inner_op.transform_id = input.state_ids.transform_id;
    inner_op.clip_id = input.state_ids.clip_id;
    inner_op.effect_id = input.state_ids.effect_id;

    ops.AddDrawRect(std::move(outer_op));
    ops.AddDrawRect(std::move(inner_op));
  }
}

}  // namespace border_painter
