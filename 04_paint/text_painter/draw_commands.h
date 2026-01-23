#ifndef TEXT_PAINTER_DRAW_COMMANDS_H_
#define TEXT_PAINTER_DRAW_COMMANDS_H_

#include "types.h"
#include <string>
#include <variant>
#include <vector>

// Drawing operations matching Chromium's paint op format

// Font info for serialization in a run
struct RunFont {
  float size = 16.0f;
  float scale_x = 1.0f;
  float skew_x = 0.0f;
  bool embolden = false;
  bool linear_metrics = true;
  bool subpixel = true;
  bool force_auto_hinting = false;
  std::string family;
  int typeface_id = 0;
  int weight = 400;
  int width = 5;
  int slant = 0;
};

// A text blob run (from HarfBuzz shaping)
struct TextBlobRun {
  size_t glyph_count = 0;
  std::vector<uint16_t> glyphs;
  int positioning = 1;  // 1 = horizontal positions only
  float offset_x = 0.0f;
  float offset_y = 0.0f;
  std::vector<float> positions;
  RunFont font;
};

// DrawTextBlobOp - main text drawing operation
struct DrawTextBlobOp {
  std::string type = "DrawTextBlobOp";
  float x = 0.0f;
  float y = 0.0f;
  DOMNodeId node_id = 0;
  PaintFlags flags;
  std::array<float, 4> bounds;  // [left, top, right, bottom] relative to origin
  std::vector<TextBlobRun> runs;
  int transform_id = 0;
  int clip_id = 0;
  int effect_id = 0;
};

// SaveOp
struct SaveOp {
  std::string type = "SaveOp";
};

// RestoreOp
struct RestoreOp {
  std::string type = "RestoreOp";
};

// ClipRectOp
struct ClipRectOp {
  std::string type = "ClipRectOp";
  RectF rect;
  bool antialias = true;
};

// TranslateOp
struct TranslateOp {
  std::string type = "TranslateOp";
  float dx = 0.0f;
  float dy = 0.0f;
};

// ScaleOp
struct ScaleOp {
  std::string type = "ScaleOp";
  float sx = 1.0f;
  float sy = 1.0f;
};

// ConcatOp (for arbitrary transforms)
struct ConcatOp {
  std::string type = "ConcatOp";
  std::array<float, 6> matrix;  // [a, b, c, d, e, f]
};

// SetMatrixOp
struct SetMatrixOp {
  std::string type = "SetMatrixOp";
  std::array<float, 9> matrix;  // 3x3 matrix
};

// DrawLineOp - for solid/double line decorations (drawn as rect)
struct DrawLineOp {
  std::string type = "DrawLineOp";
  RectF rect;  // The line rect
  Color color;
  bool snapped = true;  // Whether Y axis is snapped to pixel grid
  int transform_id = 0;
  int clip_id = 0;
  int effect_id = 0;
};

// DrawStrokeLineOp - for dotted/dashed decorations (drawn as stroke)
struct DrawStrokeLineOp {
  std::string type = "DrawStrokeLineOp";
  PointF p1;
  PointF p2;
  float thickness = 1.0f;
  StrokeStyle style = StrokeStyle::kDottedStroke;
  Color color;
  bool antialias = true;
  int transform_id = 0;
  int clip_id = 0;
  int effect_id = 0;
};

// Path command types for bezier paths
enum class PathCommandType {
  kMoveTo,
  kLineTo,
  kCubicTo,  // Cubic bezier curve
  kQuadTo,   // Quadratic bezier curve
  kClose
};

// A single path command
struct PathCommand {
  PathCommandType type = PathCommandType::kMoveTo;
  std::vector<PointF> points;  // Points depend on command type

  static PathCommand MoveTo(float x, float y) {
    return {PathCommandType::kMoveTo, {{x, y}}};
  }
  static PathCommand LineTo(float x, float y) {
    return {PathCommandType::kLineTo, {{x, y}}};
  }
  static PathCommand CubicTo(float cp1x, float cp1y, float cp2x, float cp2y,
                             float x, float y) {
    return {PathCommandType::kCubicTo, {{cp1x, cp1y}, {cp2x, cp2y}, {x, y}}};
  }
  static PathCommand Close() {
    return {PathCommandType::kClose, {}};
  }
};

// A complete path (sequence of commands)
struct Path {
  std::vector<PathCommand> commands;

  void MoveTo(float x, float y) {
    commands.push_back(PathCommand::MoveTo(x, y));
  }
  void LineTo(float x, float y) {
    commands.push_back(PathCommand::LineTo(x, y));
  }
  void CubicTo(float cp1x, float cp1y, float cp2x, float cp2y,
               float x, float y) {
    commands.push_back(PathCommand::CubicTo(cp1x, cp1y, cp2x, cp2y, x, y));
  }
  void Close() {
    commands.push_back(PathCommand::Close());
  }
};

// DrawWavyLineOp - for wavy decorations (drawn as tiled bezier pattern)
struct DrawWavyLineOp {
  std::string type = "DrawWavyLineOp";
  RectF paint_rect;        // The area to paint
  RectF tile_rect;         // The tile size (one wavelength)
  Path tile_path;          // The bezier path for one tile
  float stroke_thickness = 1.0f;
  Color color;
  WaveDefinition wave;     // Wave parameters
  int transform_id = 0;
  int clip_id = 0;
  int effect_id = 0;
};

// DrawDecorationLineOp - generic decoration line (dispatches to specific ops)
// This is kept for backward compatibility but now includes all info
struct DrawDecorationLineOp {
  std::string type = "DrawDecorationLineOp";
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float thickness = 1.0f;
  TextDecorationLine line_type = TextDecorationLine::kUnderline;
  TextDecorationStyle style = TextDecorationStyle::kSolid;
  Color color;
  float double_offset = 0.0f;  // For double decorations
  WaveDefinition wave;         // For wavy decorations
  bool antialias = false;
  int transform_id = 0;
  int clip_id = 0;
  int effect_id = 0;
};

// DrawEmphasisMarksOp - for emphasis marks (Japanese/Chinese dots)
struct DrawEmphasisMarksOp {
  std::string type = "DrawEmphasisMarksOp";
  float x = 0.0f;
  float y = 0.0f;
  std::string mark;  // The emphasis mark glyph
  std::vector<float> positions;  // X positions for each mark
  Color color;
  float font_size = 16.0f;
  int transform_id = 0;
  int clip_id = 0;
  int effect_id = 0;
};

// FillEllipseOp - for disc markers
struct FillEllipseOp {
  std::string type = "FillEllipseOp";
  RectF rect;
  Color color;
  int transform_id = 0;
  int clip_id = 0;
  int effect_id = 0;
};

// StrokeEllipseOp - for circle markers
struct StrokeEllipseOp {
  std::string type = "StrokeEllipseOp";
  RectF rect;
  Color color;
  float stroke_width = 1.0f;
  int transform_id = 0;
  int clip_id = 0;
  int effect_id = 0;
};

// FillRectOp - for square markers and backgrounds
struct FillRectOp {
  std::string type = "FillRectOp";
  RectF rect;
  Color color;
  int transform_id = 0;
  int clip_id = 0;
  int effect_id = 0;
};

// FillPathOp - for disclosure triangles
struct FillPathOp {
  std::string type = "FillPathOp";
  std::vector<PointF> points;  // Path points (closed polygon)
  Color color;
  int transform_id = 0;
  int clip_id = 0;
  int effect_id = 0;
};

// SaveLayerAlphaOp - for shadow layers
struct SaveLayerAlphaOp {
  std::string type = "SaveLayerAlphaOp";
  RectF bounds;
  float alpha = 1.0f;
};

// DrawShadowOp - for text shadows (rendered as separate text with offset/blur)
struct DrawShadowOp {
  std::string type = "DrawShadowOp";
  float offset_x = 0.0f;
  float offset_y = 0.0f;
  float blur_sigma = 0.0f;
  Color color;
  // The shadow is applied to the following DrawTextBlobOp
};

// ClearShadowOp - clears any active shadow state
struct ClearShadowOp {
  std::string type = "ClearShadowOp";
};

using PaintOp = std::variant<
    SaveOp,
    RestoreOp,
    ClipRectOp,
    TranslateOp,
    ScaleOp,
    ConcatOp,
    SetMatrixOp,
    DrawTextBlobOp,
    DrawLineOp,
    DrawStrokeLineOp,
    DrawWavyLineOp,
    DrawDecorationLineOp,
    DrawEmphasisMarksOp,
    FillEllipseOp,
    StrokeEllipseOp,
    FillRectOp,
    FillPathOp,
    SaveLayerAlphaOp,
    DrawShadowOp,
    ClearShadowOp>;

// Container for all paint operations
struct PaintOpList {
  std::vector<PaintOp> ops;

  void Save() { ops.emplace_back(SaveOp{}); }
  void Restore() { ops.emplace_back(RestoreOp{}); }

  void ClipRect(const RectF& rect, bool antialias = true) {
    ops.emplace_back(ClipRectOp{"ClipRectOp", rect, antialias});
  }

  void Translate(float dx, float dy) {
    ops.emplace_back(TranslateOp{"TranslateOp", dx, dy});
  }

  void Scale(float sx, float sy) {
    ops.emplace_back(ScaleOp{"ScaleOp", sx, sy});
  }

  void DrawTextBlob(float x, float y, DOMNodeId node_id,
                    const PaintFlags& flags,
                    const std::array<float, 4>& bounds,
                    const std::vector<TextBlobRun>& runs,
                    int transform_id, int clip_id, int effect_id) {
    ops.emplace_back(DrawTextBlobOp{
        "DrawTextBlobOp", x, y, node_id, flags, bounds, runs,
        transform_id, clip_id, effect_id});
  }

  void Concat(const AffineTransform& transform) {
    ops.emplace_back(ConcatOp{"ConcatOp", transform.ToArray()});
  }

  void DrawDecorationLine(float x, float y, float width, float thickness,
                          TextDecorationLine line_type, TextDecorationStyle style,
                          const Color& color, int transform_id, int clip_id, int effect_id) {
    DrawDecorationLineOp op;
    op.x = x;
    op.y = y;
    op.width = width;
    op.thickness = thickness;
    op.line_type = line_type;
    op.style = style;
    op.color = color;
    op.transform_id = transform_id;
    op.clip_id = clip_id;
    op.effect_id = effect_id;
    ops.emplace_back(op);
  }

  void DrawEmphasisMarks(float x, float y, const std::string& mark,
                         const std::vector<float>& positions, const Color& color,
                         float font_size, int transform_id, int clip_id, int effect_id) {
    ops.emplace_back(DrawEmphasisMarksOp{
        "DrawEmphasisMarksOp", x, y, mark, positions, color, font_size,
        transform_id, clip_id, effect_id});
  }

  void FillEllipse(const RectF& rect, const Color& color,
                   int transform_id, int clip_id, int effect_id) {
    ops.emplace_back(FillEllipseOp{"FillEllipseOp", rect, color,
                                    transform_id, clip_id, effect_id});
  }

  void StrokeEllipse(const RectF& rect, const Color& color, float stroke_width,
                     int transform_id, int clip_id, int effect_id) {
    ops.emplace_back(StrokeEllipseOp{"StrokeEllipseOp", rect, color, stroke_width,
                                      transform_id, clip_id, effect_id});
  }

  void FillRect(const RectF& rect, const Color& color,
                int transform_id, int clip_id, int effect_id) {
    ops.emplace_back(FillRectOp{"FillRectOp", rect, color,
                                 transform_id, clip_id, effect_id});
  }

  void FillPath(const std::vector<PointF>& points, const Color& color,
                int transform_id, int clip_id, int effect_id) {
    ops.emplace_back(FillPathOp{"FillPathOp", points, color,
                                 transform_id, clip_id, effect_id});
  }

  void SaveLayerAlpha(const RectF& bounds, float alpha) {
    ops.emplace_back(SaveLayerAlphaOp{"SaveLayerAlphaOp", bounds, alpha});
  }

  void AddShadow(float offset_x, float offset_y, float blur_sigma, const Color& color) {
    ops.emplace_back(DrawShadowOp{"DrawShadowOp", offset_x, offset_y, blur_sigma, color});
  }

  void ClearShadow() {
    ops.emplace_back(ClearShadowOp{"ClearShadowOp"});
  }
};

#endif  // TEXT_PAINTER_DRAW_COMMANDS_H_
