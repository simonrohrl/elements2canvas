#include "text_painter.h"
#include "text_decoration_painter.h"
#include <cmath>

// Helper to check if horizontal writing mode
static bool IsHorizontalWritingMode(WritingMode mode) {
  return mode == WritingMode::kHorizontalTb;
}

TextBlobRun TextPainter::ConvertRun(const GlyphRun& run) {
  TextBlobRun blob_run;
  blob_run.glyph_count = run.GlyphCount();
  blob_run.glyphs = run.glyphs;
  blob_run.positioning = run.positioning;
  blob_run.offset_x = run.offset_x;
  blob_run.offset_y = run.offset_y;
  blob_run.positions = run.positions;

  // Convert FontInfo to RunFont
  blob_run.font.size = run.font.size;
  blob_run.font.scale_x = run.font.scale_x;
  blob_run.font.skew_x = run.font.skew_x;
  blob_run.font.embolden = run.font.embolden;
  blob_run.font.linear_metrics = run.font.linear_metrics;
  blob_run.font.subpixel = run.font.subpixel;
  blob_run.font.force_auto_hinting = run.font.force_auto_hinting;
  blob_run.font.family = run.font.family;
  blob_run.font.typeface_id = run.font.typeface_id;
  blob_run.font.weight = run.font.weight;
  blob_run.font.width = run.font.width;
  blob_run.font.slant = run.font.slant;

  return blob_run;
}

PointF TextPainter::ComputeTextOrigin(const RectF& box, const ShapeResult& shape,
                                       float scaling_factor,
                                       const TextCombineInfo* text_combine) {
  // Text origin is at baseline
  // Use first run's font metrics if available
  float ascent = 0.0f;
  if (!shape.runs.empty()) {
    ascent = shape.runs[0].font.ascent;
  }

  float top = box.y + ascent * scaling_factor;
  float left = box.x;

  if (text_combine) {
    left += text_combine->text_left_adjustment;
    top = text_combine->text_top_adjustment;
  }

  return PointF{left, top};
}

PaintFlags TextPainter::BuildPaintFlags(const TextPaintStyle& style) {
  PaintFlags flags;
  flags.color = style.fill_color;
  flags.stroke_width = style.stroke_width;

  if (style.stroke_width > 0) {
    flags.style = PaintStyle::kStrokeAndFill;
  } else {
    flags.style = PaintStyle::kFill;
  }

  return flags;
}

AffineTransform TextPainter::ComputeWritingModeRotation(const RectF& box,
                                                         WritingMode writing_mode) {
  // For vertical writing modes, we need to rotate the text
  // The rotation is around the center of the box
  if (writing_mode == WritingMode::kVerticalRl) {
    // Rotate 90 degrees clockwise
    float cx = box.x + box.width / 2.0f;
    float cy = box.y + box.height / 2.0f;
    // Translate to origin, rotate, translate back
    AffineTransform translate1 = AffineTransform::MakeTranslation(-cx, -cy);
    AffineTransform rotate = AffineTransform::MakeRotation(90.0f);
    AffineTransform translate2 = AffineTransform::MakeTranslation(cx, cy);
    return translate2.Concat(rotate.Concat(translate1));
  } else if (writing_mode == WritingMode::kVerticalLr) {
    // Rotate 90 degrees counter-clockwise
    float cx = box.x + box.width / 2.0f;
    float cy = box.y + box.height / 2.0f;
    AffineTransform translate1 = AffineTransform::MakeTranslation(-cx, -cy);
    AffineTransform rotate = AffineTransform::MakeRotation(-90.0f);
    AffineTransform translate2 = AffineTransform::MakeTranslation(cx, cy);
    return translate2.Concat(rotate.Concat(translate1));
  }
  return AffineTransform::Identity();
}

std::vector<PointF> TextPainter::GetDisclosurePathPoints(PhysicalDirection direction,
                                                          const RectF& rect) {
  // Map unit points to the actual rect
  auto map_point = [&rect](float x, float y) -> PointF {
    return {rect.x + x * rect.width, rect.y + y * rect.height};
  };

  switch (direction) {
    case PhysicalDirection::kLeft:
      return {map_point(1.0f, 0.0f), map_point(0.14f, 0.5f), map_point(1.0f, 1.0f)};
    case PhysicalDirection::kRight:
      return {map_point(0.0f, 0.0f), map_point(0.86f, 0.5f), map_point(0.0f, 1.0f)};
    case PhysicalDirection::kUp:
      return {map_point(0.0f, 0.93f), map_point(0.5f, 0.07f), map_point(1.0f, 0.93f)};
    case PhysicalDirection::kDown:
      return {map_point(0.0f, 0.07f), map_point(0.5f, 0.93f), map_point(1.0f, 0.07f)};
  }
  return {};
}

void TextPainter::PaintSymbolMarker(PaintOpList& ops, const SymbolMarkerInfo& marker,
                                     const GraphicsStateIds& state_ids) {
  switch (marker.type) {
    case SymbolMarkerType::kDisc:
      ops.FillEllipse(marker.marker_rect, marker.color,
                      state_ids.transform_id, state_ids.clip_id, state_ids.effect_id);
      break;

    case SymbolMarkerType::kCircle:
      ops.StrokeEllipse(marker.marker_rect, marker.color, 1.0f,
                        state_ids.transform_id, state_ids.clip_id, state_ids.effect_id);
      break;

    case SymbolMarkerType::kSquare:
      ops.FillRect(marker.marker_rect, marker.color,
                   state_ids.transform_id, state_ids.clip_id, state_ids.effect_id);
      break;

    case SymbolMarkerType::kDisclosureOpen:
    case SymbolMarkerType::kDisclosureClosed: {
      // Determine direction based on writing mode (simplified - assuming horizontal LTR)
      PhysicalDirection direction = marker.is_open ? PhysicalDirection::kDown
                                                   : PhysicalDirection::kRight;
      std::vector<PointF> points = GetDisclosurePathPoints(direction, marker.marker_rect);
      ops.FillPath(points, marker.color,
                   state_ids.transform_id, state_ids.clip_id, state_ids.effect_id);
      break;
    }

    case SymbolMarkerType::kNone:
      break;
  }
}

// PaintDecorations is now handled by TextDecorationPainter
// This method is kept for backward compatibility but delegates to the new painter
void TextPainter::PaintDecorationsExceptLineThrough(
    PaintOpList& ops,
    const std::vector<TextDecoration>& decorations,
    const RectF& box,
    float font_size,
    float ascent,
    float descent,
    const GraphicsStateIds& state_ids,
    const std::optional<std::vector<ShadowData>>& shadows,
    float scaling_factor) {
  if (decorations.empty()) return;

  TextDecorationPainter painter(ops, state_ids, box.x, box.y, box.width,
                                font_size, ascent, descent, decorations,
                                shadows, scaling_factor);
  painter.PaintExceptLineThrough();
}

void TextPainter::PaintDecorationsLineThrough(
    PaintOpList& ops,
    const std::vector<TextDecoration>& decorations,
    const RectF& box,
    float font_size,
    float ascent,
    float descent,
    const GraphicsStateIds& state_ids,
    const std::optional<std::vector<ShadowData>>& shadows,
    float scaling_factor) {
  if (decorations.empty()) return;

  TextDecorationPainter painter(ops, state_ids, box.x, box.y, box.width,
                                font_size, ascent, descent, decorations,
                                shadows, scaling_factor);
  painter.PaintOnlyLineThrough();
}

void TextPainter::PaintShadows(PaintOpList& ops,
                                const std::vector<ShadowData>& shadows) {
  // Paint shadows in reverse order (bottom-most first)
  for (auto it = shadows.rbegin(); it != shadows.rend(); ++it) {
    ops.AddShadow(it->offset_x, it->offset_y, it->BlurAsSigma(), it->color);
  }
}

void TextPainter::PaintEmphasisMarks(PaintOpList& ops,
                                      const EmphasisMarkInfo& emphasis,
                                      const ShapeResult& shape,
                                      const PointF& origin,
                                      const Color& color,
                                      const GraphicsStateIds& state_ids) {
  if (emphasis.mark.empty()) return;

  // Collect positions for emphasis marks (one per character/cluster)
  std::vector<float> positions;
  for (const auto& run : shape.runs) {
    for (size_t i = 0; i < run.positions.size(); ++i) {
      positions.push_back(run.positions[i] + run.offset_x);
    }
  }

  float font_size = shape.runs.empty() ? 16.0f : shape.runs[0].font.size;
  ops.DrawEmphasisMarks(origin.x, origin.y + emphasis.offset, emphasis.mark,
                        positions, color, font_size,
                        state_ids.transform_id, state_ids.clip_id, state_ids.effect_id);
}

PaintOpList TextPainter::Paint(const TextPaintInput& input) {
  PaintOpList ops;

  // === Early exit checks (following Chromium's TextFragmentPainter::Paint) ===

  // 1. Check visibility
  if (input.visibility != Visibility::kVisible) {
    return ops;
  }

  // 2. Paint symbol markers first (if present) - check before text checks
  // because symbol markers are special items that don't have text
  if (input.symbol_marker && input.symbol_marker->type != SymbolMarkerType::kNone) {
    PaintSymbolMarker(ops, *input.symbol_marker, input.state_ids);
    return ops;  // Symbol markers don't have text
  }

  // 3. Check if we have text to paint
  if (input.fragment.from >= input.fragment.to) {
    return ops;
  }

  // 4. Check if we have shape result (and it's not just a line break)
  if (!input.fragment.HasShapeResult() && !input.is_line_break) {
    return ops;
  }

  // 5. Flow controls (line break, tab, <wbr>) need only selection painting
  // which we're excluding, so skip them
  if (input.is_flow_control) {
    return ops;
  }

  // === Build effective style (handle text clip phase) ===
  TextPaintStyle effective_style = input.style;
  if (input.paint_phase == PaintPhase::kTextClip) {
    // When we use the text as a clip, we only care about the alpha
    effective_style.current_color = Color::Black();
    effective_style.fill_color = Color::Black();
    effective_style.stroke_color = Color::Black();
    effective_style.emphasis_mark_color = Color::Black();
    effective_style.shadow = std::nullopt;
    effective_style.paint_order = EPaintOrder::kPaintOrderNormal;
  }

  // === Handle SVG text scaling ===
  float scaling_factor = 1.0f;
  bool has_svg_transform = false;
  AffineTransform svg_transform;

  if (input.svg_info) {
    scaling_factor = input.svg_info->scaling_factor;
    if (input.svg_info->has_transform) {
      has_svg_transform = true;
      svg_transform = input.svg_info->transform;
    }
  }

  // === Handle writing mode rotation ===
  bool needs_rotation = !input.is_horizontal;
  AffineTransform rotation;
  if (needs_rotation) {
    rotation = ComputeWritingModeRotation(input.box, input.writing_mode);
  }

  // === Compute text origin ===
  const ShapeResult& shape = input.fragment.shape_result;
  const TextCombineInfo* text_combine = input.text_combine
                                        ? &(*input.text_combine)
                                        : nullptr;
  PointF origin = ComputeTextOrigin(input.box, shape, scaling_factor, text_combine);

  // === Apply transforms if needed ===
  bool state_saved = false;
  if (scaling_factor != 1.0f || has_svg_transform || needs_rotation) {
    ops.Save();
    state_saved = true;

    if (scaling_factor != 1.0f) {
      ops.Scale(1.0f / scaling_factor, 1.0f / scaling_factor);
    }
    if (has_svg_transform) {
      ops.Concat(svg_transform);
    }
    if (needs_rotation) {
      ops.Concat(rotation);
    }
  }

  // === Check for shadows and decorations ===
  bool has_shadows = effective_style.shadow && !effective_style.shadow->empty();
  bool has_decorations = !input.decorations.empty();
  // Note: Shadows for decorations are now handled by TextDecorationPainter internally
  // via PaintWithTextShadow pattern, so we don't need to paint shadows separately here

  // === Get font metrics for decorations ===
  float font_size = 16.0f;
  float ascent = 14.0f;
  float descent = 4.0f;
  if (!shape.runs.empty()) {
    font_size = shape.runs[0].font.size;
    ascent = shape.runs[0].font.ascent;
    descent = shape.runs[0].font.descent;
  }

  // === Paint decorations (except line-through) ===
  if (has_decorations) {
    PaintDecorationsExceptLineThrough(ops, input.decorations, input.box,
                                      font_size, ascent, descent,
                                      input.state_ids, effective_style.shadow,
                                      scaling_factor);
  }

  // === Paint text ===
  PaintFlags flags = BuildPaintFlags(effective_style);

  // Convert glyph runs
  std::vector<TextBlobRun> blob_runs;
  for (const auto& run : shape.runs) {
    blob_runs.push_back(ConvertRun(run));
  }

  // Bounds are relative to the text blob origin
  std::array<float, 4> bounds = {
      shape.bounds.x,
      shape.bounds.y,
      shape.bounds.x + shape.bounds.width,
      shape.bounds.y + shape.bounds.height
  };

  // Emit DrawTextBlobOp (with or without shadows depending on whether we painted shadows earlier)
  if (has_shadows && !has_decorations) {
    // Paint shadows as part of the text
    PaintShadows(ops, *effective_style.shadow);
  }

  ops.DrawTextBlob(origin.x, origin.y, input.node_id, flags, bounds, blob_runs,
                   input.state_ids.transform_id, input.state_ids.clip_id,
                   input.state_ids.effect_id);

  // === Paint line-through decoration ===
  if (has_decorations) {
    PaintDecorationsLineThrough(ops, input.decorations, input.box,
                                font_size, ascent, descent,
                                input.state_ids, effective_style.shadow,
                                scaling_factor);
  }

  // === Paint emphasis marks ===
  if (input.emphasis_mark && !input.emphasis_mark->mark.empty() && !input.is_ellipsis) {
    PaintEmphasisMarks(ops, *input.emphasis_mark, shape, origin,
                       effective_style.emphasis_mark_color, input.state_ids);
  }

  // === Restore state if we saved it ===
  if (state_saved) {
    ops.Restore();
  }

  return ops;
}
