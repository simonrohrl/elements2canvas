#ifndef TEXT_PAINTER_TEXT_PAINTER_H_
#define TEXT_PAINTER_TEXT_PAINTER_H_

#include "types.h"
#include "draw_commands.h"

// Input context for text painting - all data needed to paint text
// This mirrors the data that TextFragmentPainter::Paint receives from Chromium
struct TextPaintInput {
  // === Core text data ===
  TextFragmentPaintInfo fragment;  // Text content and shape result
  RectF box;                       // Physical box rect for the text
  TextPaintStyle style;            // Paint style (colors, stroke, shadow)
  PaintPhase paint_phase = PaintPhase::kForeground;
  DOMNodeId node_id = kInvalidDOMNodeId;
  GraphicsStateIds state_ids;      // transform_id, clip_id, effect_id

  // === Visibility ===
  Visibility visibility = Visibility::kVisible;

  // === Writing mode ===
  WritingMode writing_mode = WritingMode::kHorizontalTb;
  bool is_horizontal = true;

  // === SVG text ===
  std::optional<SvgTextInfo> svg_info;

  // === Text combine (vertical CJK) ===
  std::optional<TextCombineInfo> text_combine;

  // === Emphasis marks ===
  std::optional<EmphasisMarkInfo> emphasis_mark;

  // === Text decorations ===
  std::vector<TextDecoration> decorations;

  // === Symbol marker (list bullets) ===
  std::optional<SymbolMarkerInfo> symbol_marker;

  // === Dark mode ===
  AutoDarkMode dark_mode;

  // === Flags ===
  bool is_ellipsis = false;
  bool is_line_break = false;
  bool is_flow_control = false;
};

// Pure functional text painter
// Takes input context and produces paint operations
// This follows the same logic as Chromium's TextFragmentPainter::Paint
class TextPainter {
 public:
  // Main entry point - produces paint ops from input
  static PaintOpList Paint(const TextPaintInput& input);

 private:
  // Convert GlyphRun to TextBlobRun
  static TextBlobRun ConvertRun(const GlyphRun& run);

  // Compute text origin from box and font metrics
  static PointF ComputeTextOrigin(const RectF& box, const ShapeResult& shape,
                                  float scaling_factor = 1.0f,
                                  const TextCombineInfo* text_combine = nullptr);

  // Build PaintFlags from style
  static PaintFlags BuildPaintFlags(const TextPaintStyle& style);

  // Compute rotation transform for vertical writing mode
  static AffineTransform ComputeWritingModeRotation(const RectF& box,
                                                     WritingMode writing_mode);

  // Paint symbol markers (disc, circle, square, disclosure triangles)
  static void PaintSymbolMarker(PaintOpList& ops, const SymbolMarkerInfo& marker,
                                const GraphicsStateIds& state_ids);

  // Paint text decorations (underline, overline) - uses TextDecorationPainter
  static void PaintDecorationsExceptLineThrough(
      PaintOpList& ops,
      const std::vector<TextDecoration>& decorations,
      const RectF& box,
      float font_size,
      float ascent,
      float descent,
      const GraphicsStateIds& state_ids,
      const std::optional<std::vector<ShadowData>>& shadows = std::nullopt,
      float scaling_factor = 1.0f,
      std::optional<float> font_underline_position = std::nullopt,
      std::optional<float> font_underline_thickness = std::nullopt);

  // Paint line-through decorations - uses TextDecorationPainter
  static void PaintDecorationsLineThrough(
      PaintOpList& ops,
      const std::vector<TextDecoration>& decorations,
      const RectF& box,
      float font_size,
      float ascent,
      float descent,
      const GraphicsStateIds& state_ids,
      const std::optional<std::vector<ShadowData>>& shadows = std::nullopt,
      float scaling_factor = 1.0f,
      std::optional<float> font_underline_position = std::nullopt,
      std::optional<float> font_underline_thickness = std::nullopt);

  // Paint text shadows
  static void PaintShadows(PaintOpList& ops,
                           const std::vector<ShadowData>& shadows);

  // Paint emphasis marks
  static void PaintEmphasisMarks(PaintOpList& ops,
                                 const EmphasisMarkInfo& emphasis,
                                 const ShapeResult& shape,
                                 const PointF& origin,
                                 const Color& color,
                                 const GraphicsStateIds& state_ids);

  // Get disclosure triangle path points
  static std::vector<PointF> GetDisclosurePathPoints(PhysicalDirection direction,
                                                      const RectF& rect);
};

#endif  // TEXT_PAINTER_TEXT_PAINTER_H_
