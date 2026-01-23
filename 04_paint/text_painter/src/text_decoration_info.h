// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is adapted from Chromium's text_decoration_info.h
// Original: third_party/blink/renderer/core/paint/text_decoration_info.h
//
// Changes from Chromium:
// - Simplified constructor that takes our TextDecoration struct directly
// - Removed InlinePaintContext, DecoratingBox, ComputedStyle dependencies
// - Removed selection decoration handling
// - Removed highlight override
// - Uses our types instead of Chromium's

#ifndef TEXT_PAINTER_TEXT_DECORATION_INFO_H_
#define TEXT_PAINTER_TEXT_DECORATION_INFO_H_

#include "types.h"
#include "decoration_line_painter.h"
#include <optional>

// Position of underline relative to text
enum class ResolvedUnderlinePosition {
  kNearAlphabeticBaselineAuto,
  kNearAlphabeticBaselineFromFont,
  kUnder,
  kOver
};

// Container for computing and storing information for text decoration
// invalidation and painting.
class TextDecorationInfo {
 public:
  // Simplified constructor for our use case
  TextDecorationInfo(float local_origin_x,
                     float local_origin_y,
                     float width,
                     float font_size,
                     float ascent,
                     float descent,
                     const std::vector<TextDecoration>& decorations,
                     float scaling_factor = 1.0f,
                     std::optional<float> font_underline_position = std::nullopt,
                     std::optional<float> font_underline_thickness = std::nullopt);

  size_t DecorationCount() const { return decorations_.size(); }
  const TextDecoration& GetDecoration(size_t index) const {
    return decorations_[index];
  }

  // Returns whether any of the decorations have any of the given lines
  bool HasAnyLine(TextDecorationLine lines) const {
    return HasFlag(union_all_lines_, lines);
  }

  // Check for specific line types in current decoration
  bool HasUnderline() const { return has_underline_; }
  bool HasOverline() const { return has_overline_; }
  bool HasLineThrough() const { return HasFlag(lines_, TextDecorationLine::kLineThrough); }
  bool HasSpellingOrGrammarError() const {
    return HasFlag(lines_, TextDecorationLine::kSpellingError | TextDecorationLine::kGrammarError);
  }

  // Set the decoration index to use for subsequent operations
  void SetDecorationIndex(size_t index);

  // Set line data for specific decoration types
  void SetUnderlineLineData();
  void SetOverlineLineData();
  void SetLineThroughLineData();
  void SetSpellingOrGrammarErrorLineData();

  // Get the geometry for the current line (after SetXxxLineData was called)
  const DecorationGeometry& GetGeometry() const { return line_geometry_; }

  // Get the line color for the current decoration
  Color LineColor() const;

  // Compute bounds for the current line geometry
  RectF Bounds() const;

  // Accessors
  float ScalingFactor() const { return scaling_factor_; }
  float Ascent() const { return ascent_; }
  float FontSize() const { return font_size_; }
  float ResolvedThickness() const { return resolved_thickness_; }

 private:
  void UpdateForDecorationIndex();
  float ComputeThickness() const;
  void SetLineData(TextDecorationLine line, float line_offset);

  // Input parameters
  float local_origin_x_;
  float local_origin_y_;
  float width_;
  float font_size_;
  float ascent_;
  float descent_;
  std::vector<TextDecoration> decorations_;
  float scaling_factor_;
  std::optional<float> font_underline_position_;
  std::optional<float> font_underline_thickness_;

  // Cached state for current decoration
  size_t decoration_index_ = 0;
  TextDecorationLine lines_ = TextDecorationLine::kNone;
  TextDecorationLine union_all_lines_ = TextDecorationLine::kNone;
  bool has_underline_ = false;
  bool has_overline_ = false;
  float resolved_thickness_ = 1.0f;
  bool antialias_ = false;

  DecorationGeometry line_geometry_;
};

#endif  // TEXT_PAINTER_TEXT_DECORATION_INFO_H_
