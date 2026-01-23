#ifndef BLOCK_PAINTER_DRAW_COMMANDS_H_
#define BLOCK_PAINTER_DRAW_COMMANDS_H_

#include "types.h"

#include <array>
#include <string>
#include <variant>
#include <vector>

// Shadow data for paint flags (matches Chromium's SkDrawLooper format)
struct ShadowFlag {
  float offset_x = 0.0f;
  float offset_y = 0.0f;
  float blur_sigma = 0.0f;
  Color color;
  int flags = 2;  // kShadowOffsetFlag | kShadowBlurFlag
};

// Paint flags for draw operations
struct DrawFlags {
  float r = 0.0f;
  float g = 0.0f;
  float b = 0.0f;
  float a = 1.0f;
  int style = 0;  // 0 = Fill, 1 = Stroke
  float stroke_width = 0.0f;
  int stroke_cap = 0;
  int stroke_join = 0;
  std::vector<ShadowFlag> shadows;

  void SetColor(const Color& c) {
    r = c.R();
    g = c.G();
    b = c.B();
    a = c.A();
  }
};

// DrawRectOp - simple rectangle fill (no border radius)
struct DrawRectOp {
  std::string type = "DrawRectOp";
  std::array<float, 4> rect;  // [left, top, right, bottom]
  DrawFlags flags;
  int transform_id = 0;
  int clip_id = 0;
  int effect_id = 0;
};

// DrawRRectOp - rounded rectangle fill
struct DrawRRectOp {
  std::string type = "DrawRRectOp";
  std::array<float, 4> rect;  // [left, top, right, bottom]
  BorderRadii radii;          // [tl_x, tl_y, tr_x, tr_y, br_x, br_y, bl_x, bl_y]
  DrawFlags flags;
  int transform_id = 0;
  int clip_id = 0;
  int effect_id = 0;
};

// ClipRRectOp - rounded rect clipping
struct ClipRRectOp {
  std::string type = "ClipRRectOp";
  std::array<float, 4> rect;
  BorderRadii radii;
  bool anti_alias = true;
  int clip_op = 0;  // 0 = intersect
  int transform_id = 0;
  int clip_id = 0;
  int effect_id = 0;
};

// SaveOp
struct SaveOp {
  std::string type = "SaveOp";
  int transform_id = 0;
  int clip_id = 0;
  int effect_id = 0;
};

// RestoreOp
struct RestoreOp {
  std::string type = "RestoreOp";
  int transform_id = 0;
  int clip_id = 0;
  int effect_id = 0;
};

// Variant type for all paint operations
using PaintOp = std::variant<
    DrawRectOp,
    DrawRRectOp,
    ClipRRectOp,
    SaveOp,
    RestoreOp>;

// Container for paint operations
struct PaintOpList {
  std::vector<PaintOp> ops;

  void DrawRect(const std::array<float, 4>& rect, const DrawFlags& flags,
                int transform_id, int clip_id, int effect_id) {
    DrawRectOp op;
    op.rect = rect;
    op.flags = flags;
    op.transform_id = transform_id;
    op.clip_id = clip_id;
    op.effect_id = effect_id;
    ops.emplace_back(op);
  }

  void DrawRRect(const std::array<float, 4>& rect, const BorderRadii& radii,
                 const DrawFlags& flags, int transform_id, int clip_id, int effect_id) {
    DrawRRectOp op;
    op.rect = rect;
    op.radii = radii;
    op.flags = flags;
    op.transform_id = transform_id;
    op.clip_id = clip_id;
    op.effect_id = effect_id;
    ops.emplace_back(op);
  }

  void ClipRRect(const std::array<float, 4>& rect, const BorderRadii& radii,
                 bool anti_alias, int transform_id, int clip_id, int effect_id) {
    ClipRRectOp op;
    op.rect = rect;
    op.radii = radii;
    op.anti_alias = anti_alias;
    op.transform_id = transform_id;
    op.clip_id = clip_id;
    op.effect_id = effect_id;
    ops.emplace_back(op);
  }

  void Save(int transform_id, int clip_id, int effect_id) {
    SaveOp op;
    op.transform_id = transform_id;
    op.clip_id = clip_id;
    op.effect_id = effect_id;
    ops.emplace_back(op);
  }

  void Restore(int transform_id, int clip_id, int effect_id) {
    RestoreOp op;
    op.transform_id = transform_id;
    op.clip_id = clip_id;
    op.effect_id = effect_id;
    ops.emplace_back(op);
  }

  bool empty() const { return ops.empty(); }
  size_t size() const { return ops.size(); }
};

#endif  // BLOCK_PAINTER_DRAW_COMMANDS_H_
