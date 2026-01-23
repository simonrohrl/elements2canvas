#ifndef TEXT_PAINTER_TYPES_H_
#define TEXT_PAINTER_TYPES_H_

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

// Stubbed types that mirror Chromium's blink types

struct Color {
  uint8_t r = 0;
  uint8_t g = 0;
  uint8_t b = 0;
  uint8_t a = 255;

  bool operator==(const Color& other) const {
    return r == other.r && g == other.g && b == other.b && a == other.a;
  }
  bool operator!=(const Color& other) const { return !(*this == other); }

  static Color Black() { return {0, 0, 0, 255}; }

  std::string ToHex() const {
    char buf[10];
    snprintf(buf, sizeof(buf), "#%02x%02x%02x%02x", a, r, g, b);
    return buf;
  }

  // Returns normalized float values (0.0-1.0)
  float R() const { return r / 255.0f; }
  float G() const { return g / 255.0f; }
  float B() const { return b / 255.0f; }
  float A() const { return a / 255.0f; }

  static Color FromHex(const std::string& hex) {
    Color c;
    if (hex.length() == 9 && hex[0] == '#') {
      // #AARRGGBB format
      c.a = std::stoi(hex.substr(1, 2), nullptr, 16);
      c.r = std::stoi(hex.substr(3, 2), nullptr, 16);
      c.g = std::stoi(hex.substr(5, 2), nullptr, 16);
      c.b = std::stoi(hex.substr(7, 2), nullptr, 16);
    } else if (hex.length() == 7 && hex[0] == '#') {
      // #RRGGBB format
      c.a = 255;
      c.r = std::stoi(hex.substr(1, 2), nullptr, 16);
      c.g = std::stoi(hex.substr(3, 2), nullptr, 16);
      c.b = std::stoi(hex.substr(5, 2), nullptr, 16);
    }
    return c;
  }
};

struct PointF {
  float x = 0.0f;
  float y = 0.0f;
};

struct RectF {
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;

  // Returns bounds as [left, top, right, bottom] relative to origin
  std::array<float, 4> ToBounds(float origin_x = 0, float origin_y = 0) const {
    return {x - origin_x, y - origin_y, x + width - origin_x,
            y + height - origin_y};
  }
};

struct ShadowData {
  float offset_x = 0.0f;
  float offset_y = 0.0f;
  float blur = 0.0f;
  Color color;

  float BlurAsSigma() const {
    // Chromium uses blur / 2.0 for sigma approximation
    return blur / 2.0f;
  }
};

enum class ColorScheme { kLight, kDark };

enum class EPaintOrder {
  kPaintOrderNormal,
  kPaintOrderFillStrokeMarkers,
  kPaintOrderFillMarkersStroke,
  kPaintOrderStrokeFillMarkers,
  kPaintOrderStrokeMarkersFill,
  kPaintOrderMarkersFillStroke,
  kPaintOrderMarkersStrokeFill
};

enum class TextPaintOrder { kFillStroke, kStrokeFill };

// Skia paint styles
enum class PaintStyle {
  kFill = 0,
  kStroke = 1,
  kStrokeAndFill = 2
};

enum class WritingMode { kHorizontalTb, kVerticalRl, kVerticalLr };

enum class PaintPhase { kForeground, kTextClip, kSelectionDragImage };

// Visibility (CSS visibility property)
enum class Visibility { kVisible, kHidden, kCollapse };

// Text decoration line types (can be combined as flags)
enum class TextDecorationLine : unsigned {
  kNone = 0,
  kUnderline = 1 << 0,
  kOverline = 1 << 1,
  kLineThrough = 1 << 2,
  kSpellingError = 1 << 3,
  kGrammarError = 1 << 4,
};

inline TextDecorationLine operator|(TextDecorationLine a, TextDecorationLine b) {
  return static_cast<TextDecorationLine>(static_cast<unsigned>(a) | static_cast<unsigned>(b));
}

inline TextDecorationLine operator&(TextDecorationLine a, TextDecorationLine b) {
  return static_cast<TextDecorationLine>(static_cast<unsigned>(a) & static_cast<unsigned>(b));
}

inline bool HasFlag(TextDecorationLine lines, TextDecorationLine flag) {
  return (static_cast<unsigned>(lines) & static_cast<unsigned>(flag)) != 0;
}

// Text decoration style (CSS text-decoration-style)
enum class TextDecorationStyle {
  kSolid,
  kDouble,
  kDotted,
  kDashed,
  kWavy
};

// Stroke style for decoration line painting (matches Chromium's StrokeStyle)
enum class StrokeStyle {
  kNoStroke,
  kSolidStroke,
  kDottedStroke,
  kDashedStroke,
  kDoubleStroke,
  kWavyStroke
};

// Convert TextDecorationStyle to StrokeStyle
inline StrokeStyle TextDecorationStyleToStrokeStyle(TextDecorationStyle style) {
  switch (style) {
    case TextDecorationStyle::kSolid: return StrokeStyle::kSolidStroke;
    case TextDecorationStyle::kDouble: return StrokeStyle::kDoubleStroke;
    case TextDecorationStyle::kDotted: return StrokeStyle::kDottedStroke;
    case TextDecorationStyle::kDashed: return StrokeStyle::kDashedStroke;
    case TextDecorationStyle::kWavy: return StrokeStyle::kWavyStroke;
  }
  return StrokeStyle::kSolidStroke;
}

// Wave definition for wavy decorations (from Chromium's WaveDefinition)
// Describes the parameters for a cubic bezier wave pattern
struct WaveDefinition {
  float wavelength = 0.0f;
  float control_point_distance = 0.0f;
  float phase = 0.0f;

  bool operator==(const WaveDefinition& other) const {
    return wavelength == other.wavelength &&
           control_point_distance == other.control_point_distance &&
           phase == other.phase;
  }
};

// Make a wave definition based on thickness (from Chromium's MakeWave)
inline WaveDefinition MakeWave(float thickness) {
  const float clamped_thickness = std::max<float>(1.0f, thickness);
  // Setting the step to half-pixel values gives better antialiasing results
  const float wavelength = 1 + 2 * std::round(2 * clamped_thickness + 0.5f);
  // Setting the distance to half-pixel values gives better antialiasing results
  const float cp_distance = 0.5f + std::round(3 * clamped_thickness + 0.5f);
  return {
      wavelength,
      cp_distance,
      // Offset the start point, so the bezier curve starts before the current
      // line, that way we can clip it exactly the same way in both ends
      -wavelength
  };
}

// Decoration geometry - describes the geometry for painting a decoration line
// (from Chromium's DecorationGeometry)
struct DecorationGeometry {
  StrokeStyle style = StrokeStyle::kSolidStroke;
  RectF line;  // The line rect (x, y, width, thickness)
  float double_offset = 0.0f;  // Offset for second line in double decorations
  float wavy_offset = 0.0f;    // Vertical offset for wavy decoration
  WaveDefinition wavy_wave;    // Wave parameters if style is wavy
  bool antialias = false;      // Whether to use antialiasing

  float Thickness() const { return line.height; }

  static DecorationGeometry Make(StrokeStyle style, const RectF& line_rect,
                                 float double_offset, float wavy_offset,
                                 const WaveDefinition* custom_wave = nullptr) {
    DecorationGeometry geometry;
    geometry.style = style;
    geometry.line = line_rect;
    geometry.double_offset = double_offset;

    if (style == StrokeStyle::kWavyStroke) {
      geometry.wavy_wave = custom_wave ? *custom_wave : MakeWave(geometry.Thickness());
      geometry.wavy_offset = wavy_offset;
    }
    return geometry;
  }
};

// A single text decoration
struct TextDecoration {
  TextDecorationLine line = TextDecorationLine::kNone;
  TextDecorationStyle style = TextDecorationStyle::kSolid;
  Color color;
  float thickness = 1.0f;
  float underline_offset = 0.0f;  // For underline position

  // Get the stroke style for painting
  StrokeStyle GetStrokeStyle() const {
    return TextDecorationStyleToStrokeStyle(style);
  }
};

// Emphasis mark position (logical side)
enum class LineLogicalSide { kOver, kUnder };

// Text emphasis mark type
enum class TextEmphasisMark {
  kNone,
  kDot,
  kCircle,
  kDoubleCircle,
  kTriangle,
  kSesame,
  kCustom
};

// Physical direction for disclosure triangles
enum class PhysicalDirection { kLeft, kRight, kUp, kDown };

// Symbol marker types for list items
enum class SymbolMarkerType {
  kNone,
  kDisc,
  kCircle,
  kSquare,
  kDisclosureOpen,
  kDisclosureClosed
};

// Affine transform (2D transformation matrix)
// [a c e]   [scale_x  skew_x   translate_x]
// [b d f] = [skew_y   scale_y  translate_y]
// [0 0 1]   [0        0        1          ]
struct AffineTransform {
  float a = 1.0f;  // scale_x
  float b = 0.0f;  // skew_y
  float c = 0.0f;  // skew_x
  float d = 1.0f;  // scale_y
  float e = 0.0f;  // translate_x
  float f = 0.0f;  // translate_y

  static AffineTransform Identity() { return {}; }

  static AffineTransform MakeScale(float sx, float sy) {
    return {sx, 0, 0, sy, 0, 0};
  }

  static AffineTransform MakeTranslation(float tx, float ty) {
    return {1, 0, 0, 1, tx, ty};
  }

  static AffineTransform MakeRotation(float angle_degrees) {
    float rad = angle_degrees * 3.14159265358979323846f / 180.0f;
    float cos_a = std::cos(rad);
    float sin_a = std::sin(rad);
    return {cos_a, sin_a, -sin_a, cos_a, 0, 0};
  }

  AffineTransform Concat(const AffineTransform& other) const {
    return {
      a * other.a + c * other.b,
      b * other.a + d * other.b,
      a * other.c + c * other.d,
      b * other.c + d * other.d,
      a * other.e + c * other.f + e,
      b * other.e + d * other.f + f
    };
  }

  bool IsIdentity() const {
    return a == 1.0f && b == 0.0f && c == 0.0f && d == 1.0f && e == 0.0f && f == 0.0f;
  }

  std::array<float, 6> ToArray() const { return {a, b, c, d, e, f}; }
};

// SVG text-specific info
struct SvgTextInfo {
  float scaling_factor = 1.0f;
  bool has_transform = false;
  AffineTransform transform;
  float length_adjust_scale = 1.0f;
};

// Text combine info (for vertical CJK text)
struct TextCombineInfo {
  bool is_combined = false;
  float compressed_font_scale = 1.0f;
  float text_left_adjustment = 0.0f;
  float text_top_adjustment = 0.0f;
};

// Emphasis mark info (expanded)
struct EmphasisMarkInfo {
  std::string mark;                       // The mark glyph string (e.g., "•")
  LineLogicalSide side = LineLogicalSide::kOver;
  float offset = 0.0f;                    // Computed offset from text baseline
  bool has_annotation_on_same_side = false;  // Ruby annotation conflict
};

using DOMNodeId = int64_t;
constexpr DOMNodeId kInvalidDOMNodeId = 0;

// Font info for a glyph run (matches Skia font serialization)
struct FontInfo {
  std::string family;
  float size = 16.0f;
  int weight = 400;
  int width = 5;  // Normal width
  int slant = 0;  // Upright
  float scale_x = 1.0f;
  float skew_x = 0.0f;
  bool embolden = false;
  bool linear_metrics = true;
  bool subpixel = true;
  bool force_auto_hinting = false;
  int typeface_id = 0;

  // For computing text origin
  float ascent = 0.0f;
  float descent = 0.0f;

  // Font-supplied underline metrics (optional)
  // When provided, these come from the font's OS/2 or post tables
  std::optional<float> underline_position;   // Distance from baseline (positive = below)
  std::optional<float> underline_thickness;  // Thickness of underline
};

// A single glyph run from HarfBuzz shaping
struct GlyphRun {
  FontInfo font;
  std::vector<uint16_t> glyphs;       // Glyph IDs
  std::vector<float> positions;        // X positions (horizontal positioning)
  float offset_x = 0.0f;               // Run offset X
  float offset_y = 0.0f;               // Run offset Y
  int positioning = 1;                 // 1 = horizontal, 2 = full positioning

  size_t GlyphCount() const { return glyphs.size(); }
};

// Shape result from HarfBuzz - pre-computed during layout
struct ShapeResult {
  std::vector<GlyphRun> runs;
  RectF bounds;  // Ink bounds of all glyphs

  bool IsEmpty() const { return runs.empty(); }
};

// Text fragment info - what text to paint and where
struct TextFragmentPaintInfo {
  std::string text;
  unsigned from = 0;
  unsigned to = 0;
  ShapeResult shape_result;

  unsigned Length() const { return to - from; }
  bool HasShapeResult() const { return !shape_result.IsEmpty(); }
};

// Style for painting text
struct TextPaintStyle {
  Color current_color;
  Color fill_color;
  Color stroke_color;
  Color emphasis_mark_color;
  float stroke_width = 0.0f;
  ColorScheme color_scheme = ColorScheme::kLight;
  std::optional<std::vector<ShadowData>> shadow;
  EPaintOrder paint_order = EPaintOrder::kPaintOrderNormal;

  bool operator==(const TextPaintStyle& other) const {
    return current_color == other.current_color &&
           fill_color == other.fill_color &&
           stroke_color == other.stroke_color &&
           emphasis_mark_color == other.emphasis_mark_color &&
           stroke_width == other.stroke_width &&
           color_scheme == other.color_scheme &&
           paint_order == other.paint_order;
  }
};

// Dark mode settings
struct AutoDarkMode {
  bool enabled = false;
};

// Symbol marker info for list items
struct SymbolMarkerInfo {
  SymbolMarkerType type = SymbolMarkerType::kNone;
  RectF marker_rect;  // Position and size of the marker
  Color color;
  bool is_open = false;  // For disclosure triangles
};

// Graphics state IDs for property trees
struct GraphicsStateIds {
  int transform_id = 0;
  int clip_id = 0;
  int effect_id = 0;
};

// Paint flags matching Skia/Chromium format
struct PaintFlags {
  Color color;
  PaintStyle style = PaintStyle::kFill;
  float stroke_width = 0.0f;

  float R() const { return color.R(); }
  float G() const { return color.G(); }
  float B() const { return color.B(); }
  float A() const { return color.A(); }
};

#endif  // TEXT_PAINTER_TYPES_H_
