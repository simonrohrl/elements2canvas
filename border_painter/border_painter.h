// Copyright 2025 The Chromium Authors (adapted)
// Border Painter - Standalone implementation following Chromium patterns

#ifndef BORDER_PAINTER_BORDER_PAINTER_H_
#define BORDER_PAINTER_BORDER_PAINTER_H_

#include <optional>
#include <vector>

#include "draw_commands.h"
#include "types.h"

namespace border_painter {

// Render hint - tells the painter which strategy Chromium used
enum class BorderRenderHint {
  kAuto,           // Let painter decide
  kStrokedRect,    // Uniform border as single stroked rect/rrect
  kDrawLine,       // Per-side as stroked lines
  kFilledThinRect, // Per-side as filled thin rectangles
  kDoubleStroked,  // Double border as 2 stroked rects
  kDottedLines,    // Dotted border as 4 DrawLineOp with dash pattern
  kGrooveRidge     // Groove/ridge as paired thin rects per side
};

// Input data for border painting
struct BorderPaintInput {
  RectF geometry;
  BorderWidths border_widths;
  BorderColors border_colors;
  std::optional<BorderStyles> border_styles;  // Default is solid
  std::optional<BorderRadii> border_radii;
  Visibility visibility = Visibility::kVisible;
  DOMNodeId node_id = kInvalidDOMNodeId;
  GraphicsStateIds state_ids;
  BorderRenderHint render_hint = BorderRenderHint::kAuto;  // For verification
};

// Paints borders for block-level elements
// Following Chromium's BoxBorderPainter logic
class BorderPainter {
 public:
  // Paint borders and return the list of paint operations
  static PaintOpList Paint(const BorderPaintInput& input);

 private:
  // Analyze border properties
  struct BorderProperties {
    bool is_uniform_width = true;
    bool is_uniform_color = true;
    bool is_uniform_style = true;
    bool is_rounded = false;
    bool has_transparency = false;
    unsigned visible_edge_count = 0;
    unsigned first_visible_edge = 0;
  };

  static BorderProperties AnalyzeBorder(const BorderPaintInput& input);

  // Fast path: uniform solid border with single stroked rect/rrect
  static bool PaintFastPath(const BorderPaintInput& input,
                            const BorderProperties& props,
                            PaintOpList& ops);

  // Slow path: paint individual sides
  static void PaintSides(const BorderPaintInput& input,
                         const BorderProperties& props,
                         PaintOpList& ops);

  // Paint all sides as stroked lines (for verification)
  static void PaintSidesAsLines(const BorderPaintInput& input,
                                const BorderProperties& props,
                                PaintOpList& ops);

  // Paint all sides as filled thin rects (for verification)
  static void PaintSidesAsFilledRects(const BorderPaintInput& input,
                                      const BorderProperties& props,
                                      PaintOpList& ops);

  // Paint a single border side
  static void PaintSide(const BorderPaintInput& input,
                        BoxSide side,
                        PaintOpList& ops);

  // Paint a single border side as filled thin rect (for thin borders < 10px)
  static void PaintSideAsFilledRect(const BorderPaintInput& input,
                                    BoxSide side,
                                    PaintOpList& ops);

  // Paint a single border side as stroked line (for thick borders >= 10px)
  static void PaintSideAsLine(const BorderPaintInput& input,
                              BoxSide side,
                              PaintOpList& ops);

  // Paint double border as 2 stroked rects
  static void PaintDoubleBorder(const BorderPaintInput& input,
                                const BorderProperties& props,
                                PaintOpList& ops);

  // Paint dotted border as 4 stroked lines with dash pattern
  static void PaintDottedBorder(const BorderPaintInput& input,
                                const BorderProperties& props,
                                PaintOpList& ops);

  // Paint groove/ridge border as paired thin rects per side
  static void PaintGrooveRidgeBorder(const BorderPaintInput& input,
                                     const BorderProperties& props,
                                     PaintOpList& ops);

  // Check if border has radius
  static bool HasBorderRadius(const BorderPaintInput& input);

  // Adjust radii for stroke center (subtract strokeWidth/2)
  static BorderRadii AdjustRadiiForStroke(const BorderRadii& radii,
                                          float stroke_width);

  // Calculate stroke rect (inset by strokeWidth/2 from outer edge)
  static std::array<float, 4> CalculateStrokeRect(const RectF& geometry,
                                                  float stroke_width);

  // Calculate inner rect (inset by full border width)
  static std::array<float, 4> CalculateInnerRect(const RectF& geometry,
                                                 const BorderWidths& widths);

  // Adjust radii for inner border
  static BorderRadii AdjustRadiiForInner(const BorderRadii& radii,
                                         const BorderWidths& widths);

  // Build draw flags for border stroke
  static DrawFlags BuildStrokeFlags(const Color& color, float stroke_width,
                                    EBorderStyle style = EBorderStyle::kSolid);

  // Build draw flags for border fill
  static DrawFlags BuildFillFlags(const Color& color);

  // Get edge info
  static BorderEdge GetEdge(const BorderPaintInput& input, BoxSide side);

  // Color modification for inset/outset/ridge/groove
  static Color CalculateBorderColor(const Color& color, BoxSide side,
                                    EBorderStyle style);
};

}  // namespace border_painter

#endif  // BORDER_PAINTER_BORDER_PAINTER_H_
