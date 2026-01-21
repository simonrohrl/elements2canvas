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

  // Try fast path first
  if (PaintFastPath(input, props, ops)) {
    return ops;
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
  // For non-uniform borders, draw each side as a line
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

// Paint a single border side
void BorderPainter::PaintSide(const BorderPaintInput& input,
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

}  // namespace border_painter
