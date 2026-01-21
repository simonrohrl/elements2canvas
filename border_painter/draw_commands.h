// Copyright 2025 The Chromium Authors (adapted)
// Border Painter Draw Commands

#ifndef BORDER_PAINTER_DRAW_COMMANDS_H_
#define BORDER_PAINTER_DRAW_COMMANDS_H_

#include <array>
#include <memory>
#include <variant>
#include <vector>

#include "types.h"

namespace border_painter {

// Draw flags for paint operations
struct DrawFlags {
  Color color;
  PaintStyle style = PaintStyle::kFill;
  float stroke_width = 0.0f;
  StrokeCap stroke_cap = StrokeCap::kButt;
  StrokeJoin stroke_join = StrokeJoin::kMiter;
  DashPattern dash_pattern;
};

// Draw a stroked rectangle (no border radius)
struct DrawRectOp {
  std::string type = "DrawRectOp";
  std::array<float, 4> rect;  // [left, top, right, bottom]
  DrawFlags flags;
  int transform_id = 0;
  int clip_id = 0;
  int effect_id = 0;
};

// Draw a stroked rounded rectangle
struct DrawRRectOp {
  std::string type = "DrawRRectOp";
  std::array<float, 4> rect;  // [left, top, right, bottom]
  BorderRadii radii;
  DrawFlags flags;
  int transform_id = 0;
  int clip_id = 0;
  int effect_id = 0;
};

// Draw a line (for individual border edges)
struct DrawLineOp {
  std::string type = "DrawLineOp";
  float x0 = 0.0f;
  float y0 = 0.0f;
  float x1 = 0.0f;
  float y1 = 0.0f;
  DrawFlags flags;
  int transform_id = 0;
  int clip_id = 0;
  int effect_id = 0;
};

// Fill a double rounded rect (outer - inner)
// Used for filled borders (solid, double outer/inner stripes)
struct DrawDRRectOp {
  std::string type = "DrawDRRectOp";
  std::array<float, 4> outer_rect;  // [left, top, right, bottom]
  BorderRadii outer_radii;
  std::array<float, 4> inner_rect;
  BorderRadii inner_radii;
  DrawFlags flags;
  int transform_id = 0;
  int clip_id = 0;
  int effect_id = 0;
};

// Paint operation variant
using PaintOp = std::variant<DrawRectOp, DrawRRectOp, DrawLineOp, DrawDRRectOp>;

// Container for paint operations
class PaintOpList {
 public:
  void AddDrawRect(DrawRectOp op) {
    ops_.push_back(std::move(op));
  }

  void AddDrawRRect(DrawRRectOp op) {
    ops_.push_back(std::move(op));
  }

  void AddDrawLine(DrawLineOp op) {
    ops_.push_back(std::move(op));
  }

  void AddDrawDRRect(DrawDRRectOp op) {
    ops_.push_back(std::move(op));
  }

  const std::vector<PaintOp>& ops() const { return ops_; }
  bool empty() const { return ops_.empty(); }
  size_t size() const { return ops_.size(); }

 private:
  std::vector<PaintOp> ops_;
};

}  // namespace border_painter

#endif  // BORDER_PAINTER_DRAW_COMMANDS_H_
