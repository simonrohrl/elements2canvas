// Copyright 2025 The Chromium Authors (adapted)
// Border Painter Types - Standalone implementation following Chromium patterns

#ifndef BORDER_PAINTER_TYPES_H_
#define BORDER_PAINTER_TYPES_H_

#include <array>
#include <cstdint>
#include <optional>
#include <cmath>

namespace border_painter {

// DOM Node identifier
using DOMNodeId = int64_t;
constexpr DOMNodeId kInvalidDOMNodeId = -1;

// RGBA color with float components [0.0, 1.0]
struct Color {
  float r = 0.0f;
  float g = 0.0f;
  float b = 0.0f;
  float a = 1.0f;

  bool operator==(const Color& other) const {
    return r == other.r && g == other.g && b == other.b && a == other.a;
  }

  bool IsFullyTransparent() const { return a == 0.0f; }
  bool IsOpaque() const { return a == 1.0f; }

  // Darken color for inset/outset/ridge/groove borders
  Color Dark() const {
    return Color{r * 0.7f, g * 0.7f, b * 0.7f, a};
  }

  // Lighten color
  Color Light() const {
    return Color{
        std::min(1.0f, r + (1.0f - r) * 0.33f),
        std::min(1.0f, g + (1.0f - g) * 0.33f),
        std::min(1.0f, b + (1.0f - b) * 0.33f),
        a
    };
  }
};

// Rectangle with float coordinates
struct RectF {
  float x = 0.0f;
  float y = 0.0f;
  float width = 0.0f;
  float height = 0.0f;

  float Right() const { return x + width; }
  float Bottom() const { return y + height; }
  bool IsEmpty() const { return width <= 0 || height <= 0; }
};

// Point with float coordinates
struct PointF {
  float x = 0.0f;
  float y = 0.0f;
};

// Border side enumeration
enum class BoxSide {
  kTop = 0,
  kRight = 1,
  kBottom = 2,
  kLeft = 3
};

// Border style enumeration (following CSS spec)
// https://www.w3.org/TR/css-backgrounds-3/#border-style
enum class EBorderStyle {
  kNone = 0,
  kHidden = 1,
  kInset = 2,
  kGroove = 3,
  kOutset = 4,
  kRidge = 5,
  kDotted = 6,
  kDashed = 7,
  kSolid = 8,
  kDouble = 9
};

// Border edge data for one side
struct BorderEdge {
  float width = 0.0f;
  Color color;
  EBorderStyle style = EBorderStyle::kNone;

  bool ShouldRender() const {
    return width > 0 && style != EBorderStyle::kNone &&
           style != EBorderStyle::kHidden && !color.IsFullyTransparent();
  }

  bool PresentButInvisible() const {
    return width > 0 && (style == EBorderStyle::kHidden ||
                         color.IsFullyTransparent());
  }

  // Effective style - thin borders become solid
  static EBorderStyle EffectiveStyle(EBorderStyle style, int width) {
    if (style == EBorderStyle::kDouble && width < 3) {
      return EBorderStyle::kSolid;
    }
    if ((style == EBorderStyle::kRidge || style == EBorderStyle::kGroove) &&
        width < 2) {
      return EBorderStyle::kSolid;
    }
    return style;
  }

  bool SharesColorWith(const BorderEdge& other) const {
    return color == other.color;
  }
};

// Border widths for each side
struct BorderWidths {
  float top = 0.0f;
  float right = 0.0f;
  float bottom = 0.0f;
  float left = 0.0f;

  bool IsUniform() const {
    return top == right && right == bottom && bottom == left;
  }

  bool IsZero() const {
    return top == 0.0f && right == 0.0f && bottom == 0.0f && left == 0.0f;
  }
};

// Border colors for each side
struct BorderColors {
  Color top;
  Color right;
  Color bottom;
  Color left;

  bool IsUniform() const {
    return top == right && right == bottom && bottom == left;
  }
};

// Border styles for each side
struct BorderStyles {
  EBorderStyle top = EBorderStyle::kSolid;
  EBorderStyle right = EBorderStyle::kSolid;
  EBorderStyle bottom = EBorderStyle::kSolid;
  EBorderStyle left = EBorderStyle::kSolid;

  bool IsUniform() const {
    return top == right && right == bottom && bottom == left;
  }

  bool AllSolid() const {
    return top == EBorderStyle::kSolid && right == EBorderStyle::kSolid &&
           bottom == EBorderStyle::kSolid && left == EBorderStyle::kSolid;
  }
};

// Border radii: [tl_x, tl_y, tr_x, tr_y, br_x, br_y, bl_x, bl_y]
using BorderRadii = std::array<float, 8>;

// Check if radii are all zero
inline bool IsZeroRadii(const BorderRadii& radii) {
  for (float r : radii) {
    if (r > 0.0f) return false;
  }
  return true;
}

// Visibility enum
enum class Visibility {
  kVisible,
  kHidden,
  kCollapse
};

// Property tree state IDs
struct GraphicsStateIds {
  int transform_id = 0;
  int clip_id = 0;
  int effect_id = 0;
};

// Paint style
enum class PaintStyle {
  kFill = 0,
  kStroke = 1,
  kStrokeAndFill = 2
};

// Stroke cap style
enum class StrokeCap {
  kButt = 0,
  kRound = 1,
  kSquare = 2
};

// Stroke join style
enum class StrokeJoin {
  kMiter = 0,
  kRound = 1,
  kBevel = 2
};

// Dash pattern for dashed/dotted borders
struct DashPattern {
  float intervals[2] = {0, 0};
  float phase = 0;
  bool has_pattern = false;
};

}  // namespace border_painter

#endif  // BORDER_PAINTER_TYPES_H_
