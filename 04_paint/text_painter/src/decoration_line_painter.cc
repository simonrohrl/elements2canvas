// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file is adapted from Chromium's decoration_line_painter.cc
// Original: third_party/blink/renderer/core/paint/decoration_line_painter.cc
//
// Changes from Chromium:
// - Uses PaintOpList instead of GraphicsContext
// - Emits paint ops instead of calling GraphicsContext methods
// - Removed caching (WavyCache) - we generate fresh paths each time
// - Removed AutoDarkMode handling
// - Removed cc::PaintFlags for SVG

#include "decoration_line_painter.h"
#include <cmath>
#include <algorithm>

namespace {

float RoundDownThickness(float stroke_thickness) {
  return std::max(floorf(stroke_thickness), 1.0f);
}

RectF SnapYAxis(const RectF& decoration_rect) {
  RectF snapped = decoration_rect;
  snapped.y = floorf(decoration_rect.y + 0.5f);
  snapped.height = RoundDownThickness(decoration_rect.height);
  return snapped;
}

std::pair<PointF, PointF> GetSnappedPointsForTextLine(
    const RectF& decoration_rect) {
  float mid_y = floorf(decoration_rect.y +
                       std::max(decoration_rect.height / 2.0f, 0.5f));
  return {{decoration_rect.x, mid_y},
          {decoration_rect.x + decoration_rect.width, mid_y}};
}

// Check if stroke style is dashed (vs dotted which uses round caps)
bool StrokeIsDashed(float thickness, StrokeStyle style) {
  // Thick lines with dotted style use round caps (circles)
  // Thin lines or dashed style use actual dashes
  return style == StrokeStyle::kDashedStroke ||
         (style == StrokeStyle::kDottedStroke && thickness < 2);
}

// Prepares a path for a cubic Bezier curve repeated three times, yielding a
// wavy pattern that we can cut into a tiling shader.
//
// The result ignores the local origin, line offset, and (wavy) double offset,
// so the midpoints are always at y=0.5, while the phase is shifted for either
// wavy or spelling/grammar decorations so the desired pattern starts at x=0.
//
// The start point, control points (cp1 and cp2), and end point of each curve
// form a diamond shape:
//
//            cp2                      cp2                      cp2
// ---         +                        +                        +
// |               x=0
// | control         |--- spelling/grammar ---|
// | point          . .                      . .                      . .
// | distance     .     .                  .     .                  .     .
// |            .         .              .         .              .         .
// +-- y=0.5   .            +           .            +           .            +
//  .         .              .         .              .         .
//    .     .                  .     .                  .     .
//      . .                      . .                      . .
//                          |-------- other ---------|
//                        x=0
//             +                        +                        +
//            cp1                      cp1                      cp1
// |----- wavelength -------|
Path WavyPath(const WaveDefinition& wave) {
  // Midpoints at y=0.5, to reduce vertical antialiasing.
  PointF start{wave.phase, 0.5f};
  PointF end{start.x + wave.wavelength, 0.5f};
  PointF cp1{start.x + wave.wavelength * 0.5f,
             0.5f + wave.control_point_distance};
  PointF cp2{start.x + wave.wavelength * 0.5f,
             0.5f - wave.control_point_distance};

  Path result;
  result.MoveTo(start.x, start.y);

  // First curve
  result.CubicTo(cp1.x, cp1.y, cp2.x, cp2.y, end.x, end.y);

  // Second curve
  cp1.x += wave.wavelength;
  cp2.x += wave.wavelength;
  end.x += wave.wavelength;
  result.CubicTo(cp1.x, cp1.y, cp2.x, cp2.y, end.x, end.y);

  // Third curve
  cp1.x += wave.wavelength;
  cp2.x += wave.wavelength;
  end.x += wave.wavelength;
  result.CubicTo(cp1.x, cp1.y, cp2.x, cp2.y, end.x, end.y);

  return result;
}

// Computes the wavy pattern rect, which is where the desired wavy pattern
// would be found when painting the wavy stroke path at the origin.
RectF ComputeWavyPatternRect(float thickness, const WaveDefinition& wave,
                              const Path& stroke_path) {
  // Approximate the stroke bounds
  // The path oscillates around y=0.5 with amplitude ~= control_point_distance
  // With stroke, it extends by thickness/2 on each side
  float amplitude = wave.control_point_distance;
  float half_thickness = thickness / 2.0f;

  float top = floorf(0.5f - amplitude - half_thickness);
  float bottom = ceilf(0.5f + amplitude + half_thickness);

  return {0.f, top, wave.wavelength, bottom - top};
}

// Compute the paint rect for a wavy decoration
RectF ComputeWavyPaintRect(const DecorationGeometry& geometry,
                           const RectF& pattern_bounds) {
  // The offset from the local origin is the wavy offset and the origin of the
  // wavy pattern rect (around minus half the amplitude).
  PointF origin{geometry.line.x + pattern_bounds.x,
                geometry.line.y + pattern_bounds.y + geometry.wavy_offset};
  // Get the height of the wavy tile, and the width of the decoration.
  return {origin.x, origin.y, geometry.line.width, pattern_bounds.height};
}

}  // namespace

RectF DecorationLinePainter::Bounds(const DecorationGeometry& geometry) {
  switch (geometry.style) {
    case StrokeStyle::kDottedStroke:
    case StrokeStyle::kDashedStroke: {
      const float thickness = roundf(geometry.Thickness());
      auto [start, end] = GetSnappedPointsForTextLine(geometry.line);
      return {start.x, start.y - thickness / 2,
              end.x - start.x, thickness};
    }
    case StrokeStyle::kWavyStroke: {
      Path stroke_path = WavyPath(geometry.wavy_wave);
      RectF pattern_bounds = ComputeWavyPatternRect(geometry.Thickness(),
                                                     geometry.wavy_wave,
                                                     stroke_path);
      return ComputeWavyPaintRect(geometry, pattern_bounds);
    }
    case StrokeStyle::kDoubleStroke: {
      RectF double_line_rect = geometry.line;
      if (geometry.double_offset < 0) {
        double_line_rect.y += geometry.double_offset;
      }
      double_line_rect.height += std::abs(geometry.double_offset);
      return double_line_rect;
    }
    case StrokeStyle::kSolidStroke:
    case StrokeStyle::kNoStroke:
      return geometry.line;
  }
  return geometry.line;
}

void DecorationLinePainter::Paint(const DecorationGeometry& geometry,
                                   const Color& color) {
  if (geometry.line.width <= 0) {
    return;
  }

  switch (geometry.style) {
    case StrokeStyle::kWavyStroke:
      PaintWavyTextDecoration(geometry, color);
      break;

    case StrokeStyle::kDottedStroke:
    case StrokeStyle::kDashedStroke: {
      auto [start, end] = GetSnappedPointsForTextLine(geometry.line);
      const float thickness = roundf(geometry.Thickness());

      PointF p1{static_cast<float>(start.x), static_cast<float>(start.y)};
      PointF p2{static_cast<float>(end.x), static_cast<float>(end.y)};

      // For odd widths, shift the line down by 0.5 to align with pixel grid
      if (static_cast<int>(thickness) % 2) {
        p1.y += 0.5f;
        p2.y += 0.5f;
      }

      if (!StrokeIsDashed(thickness, geometry.style)) {
        // Thick dotted lines use round endcaps (circles), which extend beyond
        // the line endpoints. Move start and end in by half the thickness.
        p1.x += thickness / 2.f;
        p2.x -= thickness / 2.f;
      }

      // Emit stroke line op
      ops_.ops.emplace_back(DrawStrokeLineOp{
          "DrawStrokeLineOp",
          p1, p2,
          thickness,
          geometry.style,
          color,
          geometry.antialias,
          state_ids_.transform_id,
          state_ids_.clip_id,
          state_ids_.effect_id
      });
      break;
    }

    case StrokeStyle::kSolidStroke:
    case StrokeStyle::kDoubleStroke: {
      // Solid and double lines are drawn as filled rectangles
      const RectF snapped_line_rect = SnapYAxis(geometry.line);

      ops_.ops.emplace_back(DrawLineOp{
          "DrawLineOp",
          snapped_line_rect,
          color,
          true,  // snapped
          state_ids_.transform_id,
          state_ids_.clip_id,
          state_ids_.effect_id
      });

      if (geometry.style == StrokeStyle::kDoubleStroke) {
        RectF second_line_rect = geometry.line;
        second_line_rect.y += geometry.double_offset;
        const RectF snapped_second = SnapYAxis(second_line_rect);

        ops_.ops.emplace_back(DrawLineOp{
            "DrawLineOp",
            snapped_second,
            color,
            true,  // snapped
            state_ids_.transform_id,
            state_ids_.clip_id,
            state_ids_.effect_id
        });
      }
      break;
    }

    case StrokeStyle::kNoStroke:
      break;
  }
}

void DecorationLinePainter::PaintWavyTextDecoration(
    const DecorationGeometry& geometry,
    const Color& color) {
  const WaveDefinition& wave = geometry.wavy_wave;
  Path tile_path = WavyPath(wave);
  RectF pattern_bounds = ComputeWavyPatternRect(geometry.Thickness(), wave,
                                                 tile_path);

  // The paint rect is where we'll tile the wave pattern
  RectF paint_rect = ComputeWavyPaintRect(geometry, pattern_bounds);

  // The tile rect is one wavelength
  RectF tile_rect{0, 0, wave.wavelength, pattern_bounds.height};

  ops_.ops.emplace_back(DrawWavyLineOp{
      "DrawWavyLineOp",
      paint_rect,
      tile_rect,
      tile_path,
      geometry.Thickness(),
      color,
      wave,
      state_ids_.transform_id,
      state_ids_.clip_id,
      state_ids_.effect_id
  });
}
