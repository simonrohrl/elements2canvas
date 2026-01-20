// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is adapted from Chromium's text_decoration_painter.cc
// Original: third_party/blink/renderer/core/paint/text_decoration_painter.cc
//
// Changes from Chromium:
// - Simplified to not require Begin/PaintExcept/PaintOnly phases
// - Removed selection handling and clipping
// - Added shadow support via PaintWithTextShadow pattern
// - Uses PaintOpList instead of GraphicsContext
// - Uses DecorationLinePainter directly

#include "text_decoration_painter.h"

TextDecorationPainter::TextDecorationPainter(
    PaintOpList& ops,
    const GraphicsStateIds& state_ids,
    float local_origin_x,
    float local_origin_y,
    float width,
    float font_size,
    float ascent,
    float descent,
    const std::vector<TextDecoration>& decorations,
    const std::optional<std::vector<ShadowData>>& shadows,
    float scaling_factor,
    std::optional<float> font_underline_position,
    std::optional<float> font_underline_thickness)
    : ops_(ops),
      state_ids_(state_ids),
      decorations_(decorations),
      shadows_(shadows),
      decoration_info_(local_origin_x, local_origin_y, width, font_size,
                       ascent, descent, decorations, scaling_factor,
                       font_underline_position, font_underline_thickness) {}

Color TextDecorationPainter::LineColorForPhase(TextShadowPaintPhase phase) const {
  // In shadow phase, use black for proper shadow masking
  // In foreground phase, use the actual decoration color
  if (phase == TextShadowPaintPhase::kShadow) {
    return Color::Black();
  }
  return decoration_info_.LineColor();
}

void TextDecorationPainter::PaintUnderOrOverLineDecorations(
    TextDecorationLine lines_to_paint) {
  DecorationLinePainter line_painter(ops_, state_ids_);

  auto paint_decorations = [&](TextShadowPaintPhase phase) {
    for (size_t i = 0; i < decoration_info_.DecorationCount(); i++) {
      decoration_info_.SetDecorationIndex(i);

      // Handle spelling/grammar errors (special wavy underlines)
      if (decoration_info_.HasSpellingOrGrammarError() &&
          HasFlag(lines_to_paint, TextDecorationLine::kSpellingError |
                                  TextDecorationLine::kGrammarError)) {
        decoration_info_.SetSpellingOrGrammarErrorLineData();
        line_painter.Paint(decoration_info_.GetGeometry(),
                           LineColorForPhase(phase));
        continue;
      }

      if (decoration_info_.HasUnderline() &&
          HasFlag(lines_to_paint, TextDecorationLine::kUnderline)) {
        decoration_info_.SetUnderlineLineData();
        line_painter.Paint(decoration_info_.GetGeometry(),
                           LineColorForPhase(phase));
      }

      if (decoration_info_.HasOverline() &&
          HasFlag(lines_to_paint, TextDecorationLine::kOverline)) {
        decoration_info_.SetOverlineLineData();
        line_painter.Paint(decoration_info_.GetGeometry(),
                           LineColorForPhase(phase));
      }
    }
  };

  // Paint with shadows if present
  PaintWithTextShadow(paint_decorations, ops_, shadows_);
}

void TextDecorationPainter::PaintLineThroughDecorations() {
  DecorationLinePainter line_painter(ops_, state_ids_);

  auto paint_decorations = [&](TextShadowPaintPhase phase) {
    for (size_t i = 0; i < decoration_info_.DecorationCount(); i++) {
      decoration_info_.SetDecorationIndex(i);

      if (decoration_info_.HasLineThrough()) {
        decoration_info_.SetLineThroughLineData();
        line_painter.Paint(decoration_info_.GetGeometry(),
                           LineColorForPhase(phase));
      }
    }
  };

  // Paint with shadows if present
  PaintWithTextShadow(paint_decorations, ops_, shadows_);
}

void TextDecorationPainter::PaintExceptLineThrough() {
  if (!decoration_info_.HasAnyLine(
          TextDecorationLine::kUnderline | TextDecorationLine::kOverline |
          TextDecorationLine::kSpellingError | TextDecorationLine::kGrammarError)) {
    return;
  }

  PaintUnderOrOverLineDecorations(
      TextDecorationLine::kUnderline | TextDecorationLine::kOverline |
      TextDecorationLine::kSpellingError | TextDecorationLine::kGrammarError);
}

void TextDecorationPainter::PaintOnlyLineThrough() {
  if (!decoration_info_.HasAnyLine(TextDecorationLine::kLineThrough)) {
    return;
  }

  PaintLineThroughDecorations();
}

void TextDecorationPainter::PaintAll() {
  PaintExceptLineThrough();
  PaintOnlyLineThrough();
}
