// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is adapted from Chromium's text_decoration_info.cc
// Original: third_party/blink/renderer/core/paint/text_decoration_info.cc
//
// Changes from Chromium:
// - Simplified to work with our TextDecoration struct
// - Removed platform-specific spelling/grammar wave definitions
// - Removed decorating box offset calculations
// - Removed font override handling
// - Uses our types instead of Chromium's

#include "text_decoration_info.h"
#include <cmath>
#include <algorithm>

namespace {

// Compute decoration thickness from CSS text-decoration-thickness
// or fall back to auto (font_size / 10)
float ComputeDecorationThickness(float thickness, float font_size) {
  if (thickness > 0) {
    return std::round(thickness);
  }
  // Auto thickness: 1/10 of font size
  return font_size / 10.f;
}

}  // namespace

TextDecorationInfo::TextDecorationInfo(
    float local_origin_x,
    float local_origin_y,
    float width,
    float font_size,
    float ascent,
    float descent,
    const std::vector<TextDecoration>& decorations,
    float scaling_factor,
    std::optional<float> font_underline_position,
    std::optional<float> font_underline_thickness)
    : local_origin_x_(local_origin_x),
      local_origin_y_(local_origin_y),
      width_(width),
      font_size_(font_size),
      ascent_(ascent),
      descent_(descent),
      decorations_(decorations),
      scaling_factor_(scaling_factor),
      font_underline_position_(font_underline_position),
      font_underline_thickness_(font_underline_thickness) {
  // Compute union of all lines across all decorations
  for (const auto& decoration : decorations_) {
    union_all_lines_ = union_all_lines_ | decoration.line;
  }

  // Check if any decoration needs antialiasing (dotted/dashed)
  for (const auto& decoration : decorations_) {
    if (decoration.style == TextDecorationStyle::kDotted ||
        decoration.style == TextDecorationStyle::kDashed) {
      antialias_ = true;
      break;
    }
  }

  UpdateForDecorationIndex();
}

void TextDecorationInfo::SetDecorationIndex(size_t index) {
  if (decoration_index_ == index) {
    return;
  }
  decoration_index_ = index;
  UpdateForDecorationIndex();
}

void TextDecorationInfo::UpdateForDecorationIndex() {
  if (decoration_index_ >= decorations_.size()) {
    return;
  }

  const TextDecoration& decoration = decorations_[decoration_index_];
  lines_ = decoration.line;
  has_underline_ = HasFlag(lines_, TextDecorationLine::kUnderline);
  has_overline_ = HasFlag(lines_, TextDecorationLine::kOverline);
  resolved_thickness_ = ComputeThickness();
}

float TextDecorationInfo::ComputeThickness() const {
  if (decoration_index_ >= decorations_.size()) {
    // Fall back to font-supplied thickness or auto
    if (font_underline_thickness_) {
      return *font_underline_thickness_;
    }
    return font_size_ / 10.f;
  }
  const TextDecoration& decoration = decorations_[decoration_index_];

  // If decoration has explicit thickness, use it
  if (decoration.thickness > 0) {
    return ComputeDecorationThickness(decoration.thickness, font_size_);
  }

  // Otherwise, try font-supplied thickness
  if (font_underline_thickness_) {
    return *font_underline_thickness_;
  }

  // Fall back to auto thickness
  return ComputeDecorationThickness(decoration.thickness, font_size_);
}

void TextDecorationInfo::SetLineData(TextDecorationLine line, float line_offset) {
  const float double_offset_from_thickness = ResolvedThickness() + 1.0f;
  float double_offset;
  float wavy_offset;

  switch (line) {
    case TextDecorationLine::kUnderline:
      double_offset = double_offset_from_thickness;
      wavy_offset = double_offset_from_thickness;
      break;
    case TextDecorationLine::kOverline:
      double_offset = -double_offset_from_thickness;
      wavy_offset = -double_offset_from_thickness;
      break;
    case TextDecorationLine::kLineThrough:
      // Floor double_offset to avoid varying gap sizes
      double_offset = floorf(double_offset_from_thickness);
      wavy_offset = 0;
      break;
    default:
      double_offset = 0;
      wavy_offset = 0;
      break;
  }

  StrokeStyle style = StrokeStyle::kSolidStroke;
  if (decoration_index_ < decorations_.size()) {
    style = decorations_[decoration_index_].GetStrokeStyle();
  }

  PointF start_point{local_origin_x_, local_origin_y_ + line_offset};
  RectF line_rect{start_point.x, start_point.y, width_, ResolvedThickness()};

  line_geometry_ = DecorationGeometry::Make(style, line_rect, double_offset,
                                            wavy_offset, nullptr);
  line_geometry_.antialias = antialias_;
}

void TextDecorationInfo::SetUnderlineLineData() {
  if (!HasUnderline()) {
    return;
  }

  // Compute underline offset
  // Priority:
  // 1. Font-supplied underline position (if available)
  // 2. CSS underline-offset adjustment
  // 3. Default: descent (below baseline)
  float underline_offset;

  if (font_underline_position_) {
    // Font provides underline position (distance from baseline, positive = below)
    underline_offset = *font_underline_position_;
  } else {
    // Default: position at descent
    underline_offset = descent_;
  }

  // Add CSS text-underline-offset if specified
  if (decoration_index_ < decorations_.size()) {
    underline_offset += decorations_[decoration_index_].underline_offset;
  }

  SetLineData(TextDecorationLine::kUnderline, underline_offset);
}

void TextDecorationInfo::SetOverlineLineData() {
  if (!HasOverline()) {
    return;
  }

  // Overline is at the top of the text (above the ascent)
  float overline_offset = -ascent_;

  SetLineData(TextDecorationLine::kOverline, overline_offset);
}

void TextDecorationInfo::SetLineThroughLineData() {
  if (!HasLineThrough()) {
    return;
  }

  // Line-through is at 2/3 of ascent from baseline, minus half thickness
  // to center it
  const float line_through_offset = 2 * ascent_ / 3 - ResolvedThickness() / 2;

  SetLineData(TextDecorationLine::kLineThrough, -line_through_offset);
}

void TextDecorationInfo::SetSpellingOrGrammarErrorLineData() {
  if (!HasSpellingOrGrammarError()) {
    return;
  }

  // Spelling/grammar errors use a special wavy underline pattern
  // Position is similar to regular underline but with specific wave parameters
  float underline_offset = descent_;
  if (decoration_index_ < decorations_.size()) {
    underline_offset += decorations_[decoration_index_].underline_offset;
  }

  // Spelling/grammar errors always use wavy style
  const float double_offset_from_thickness = ResolvedThickness() + 1.0f;

  PointF start_point{local_origin_x_, local_origin_y_ + underline_offset};
  RectF line_rect{start_point.x, start_point.y, width_, ResolvedThickness()};

  // Use platform-specific wave definition for spelling/grammar
  // These values match Chromium's spelling error wave pattern
  // Wavelength is 4, control point distance is 2.5 (tighter than regular wavy)
  WaveDefinition spelling_wave{4.0f, 2.5f, 0.0f};

  line_geometry_ = DecorationGeometry::Make(StrokeStyle::kWavyStroke, line_rect,
                                            double_offset_from_thickness,
                                            double_offset_from_thickness,
                                            &spelling_wave);
  line_geometry_.antialias = true;
}

Color TextDecorationInfo::LineColor() const {
  if (decoration_index_ < decorations_.size()) {
    return decorations_[decoration_index_].color;
  }
  return Color::Black();
}

RectF TextDecorationInfo::Bounds() const {
  return DecorationLinePainter::Bounds(GetGeometry());
}
