// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is adapted from Chromium's text_shadow_painter.h
// Original: third_party/blink/renderer/core/paint/text_shadow_painter.h
//
// Changes from Chromium:
// - Uses PaintOpList instead of GraphicsContext layers
// - Simplified shadow application

#ifndef TEXT_PAINTER_TEXT_SHADOW_PAINTER_H_
#define TEXT_PAINTER_TEXT_SHADOW_PAINTER_H_

#include "types.h"
#include "draw_commands.h"

// Phase of shadow painting - mirrors Chromium's TextShadowPaintPhase
enum class TextShadowPaintPhase {
  kShadow,      // Painting the shadow layer
  kForeground,  // Painting the actual content
};

// Helper to paint content with text shadows.
// In Chromium, this uses GraphicsContext layers with paint filters.
// For our pure-functional approach, we emit separate shadow ops.
//
// Usage:
//   PaintWithTextShadow(
//       [&](TextShadowPaintPhase phase) {
//         Color color = (phase == TextShadowPaintPhase::kShadow)
//                       ? Color::Black() : decoration_color;
//         painter.Paint(geometry, color);
//       },
//       ops, shadows);
template <typename PaintProc>
void PaintWithTextShadow(PaintProc paint_proc,
                         PaintOpList& ops,
                         const std::optional<std::vector<ShadowData>>& shadows) {
  if (shadows && !shadows->empty()) {
    // Paint shadows first (in reverse order - bottom-most first)
    for (auto it = shadows->rbegin(); it != shadows->rend(); ++it) {
      ops.AddShadow(it->offset_x, it->offset_y, it->BlurAsSigma(), it->color);
    }
    // Paint the shadow phase (with black color for proper shadow masking)
    paint_proc(TextShadowPaintPhase::kShadow);

    // Clear shadows for foreground
    ops.ClearShadow();
  }

  // Paint the foreground
  paint_proc(TextShadowPaintPhase::kForeground);
}

// Simpler version for when you just want to check if shadows should be painted
inline bool HasTextShadow(const std::optional<std::vector<ShadowData>>& shadows) {
  return shadows && !shadows->empty();
}

#endif  // TEXT_PAINTER_TEXT_SHADOW_PAINTER_H_
