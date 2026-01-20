// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is adapted from Chromium's text_decoration_painter.h
// Original: third_party/blink/renderer/core/paint/text_decoration_painter.h
//
// Changes from Chromium:
// - Simplified to not require Begin/PaintExcept/PaintOnly phases
// - Removed selection handling
// - Removed clip rect handling (no selection)
// - Removed InlinePaintContext dependency
// - Uses PaintOpList instead of GraphicsContext
// - Added shadow support via PaintWithTextShadow pattern

#ifndef TEXT_PAINTER_TEXT_DECORATION_PAINTER_H_
#define TEXT_PAINTER_TEXT_DECORATION_PAINTER_H_

#include "types.h"
#include "draw_commands.h"
#include "text_decoration_info.h"
#include "decoration_line_painter.h"
#include "text_shadow_painter.h"
#include <optional>

// TextDecorationPainter - paints text decorations (underline, overline, line-through)
//
// This version supports:
// - Shadow painting for decorations (like Chromium)
// - Spelling/grammar error decorations
// - All decoration styles (solid, double, dotted, dashed, wavy)
class TextDecorationPainter {
 public:
  TextDecorationPainter(PaintOpList& ops,
                        const GraphicsStateIds& state_ids,
                        float local_origin_x,
                        float local_origin_y,
                        float width,
                        float font_size,
                        float ascent,
                        float descent,
                        const std::vector<TextDecoration>& decorations,
                        const std::optional<std::vector<ShadowData>>& shadows = std::nullopt,
                        float scaling_factor = 1.0f);

  // Paint all decorations except line-through
  // (underlines/overlines should be painted before text)
  void PaintExceptLineThrough();

  // Paint line-through decorations
  // (line-through should be painted after text)
  void PaintOnlyLineThrough();

  // Convenience method to paint all decorations at once
  // (calls PaintExceptLineThrough then PaintOnlyLineThrough)
  void PaintAll();

  // Check if there are any decorations to paint
  bool HasDecorations() const { return !decorations_.empty(); }

  // Get the decoration info for external use
  TextDecorationInfo& GetDecorationInfo() { return decoration_info_; }

 private:
  void PaintUnderOrOverLineDecorations(TextDecorationLine lines_to_paint);
  void PaintLineThroughDecorations();

  // Get line color based on shadow phase
  Color LineColorForPhase(TextShadowPaintPhase phase) const;

  PaintOpList& ops_;
  const GraphicsStateIds& state_ids_;
  std::vector<TextDecoration> decorations_;
  std::optional<std::vector<ShadowData>> shadows_;
  TextDecorationInfo decoration_info_;
};

#endif  // TEXT_PAINTER_TEXT_DECORATION_PAINTER_H_
