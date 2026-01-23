// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is adapted from Chromium's decoration_line_painter.h
// Original: third_party/blink/renderer/core/paint/decoration_line_painter.h

#ifndef TEXT_PAINTER_DECORATION_LINE_PAINTER_H_
#define TEXT_PAINTER_DECORATION_LINE_PAINTER_H_

#include "types.h"
#include "draw_commands.h"

// Helper class for painting text decorations. Each instance paints a single
// decoration.
//
// Differences from Chromium:
// - Uses PaintOpList instead of GraphicsContext
// - Removed AutoDarkMode parameter (dark mode not implemented)
// - Removed cc::PaintFlags parameter (SVG-specific, not needed for our use case)
class DecorationLinePainter {
 public:
  explicit DecorationLinePainter(PaintOpList& ops,
                                 const GraphicsStateIds& state_ids)
      : ops_(ops), state_ids_(state_ids) {}

  static RectF Bounds(const DecorationGeometry& geometry);

  void Paint(const DecorationGeometry& geometry, const Color& color);

 private:
  void PaintWavyTextDecoration(const DecorationGeometry& geometry,
                               const Color& color);

  PaintOpList& ops_;
  const GraphicsStateIds& state_ids_;
};

#endif  // TEXT_PAINTER_DECORATION_LINE_PAINTER_H_
