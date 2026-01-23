#ifndef BLOCK_PAINTER_TYPES_H_
#define BLOCK_PAINTER_TYPES_H_

#include <array>
#include <cstdint>
#include <cstdio>
#include <optional>
#include <string>
#include <vector>

// Basic types for block painting - mirrors Chromium's blink types

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
  static Color White() { return {255, 255, 255, 255}; }
  static Color Transparent() { return {0, 0, 0, 0}; }

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

  static Color FromNormalized(float r, float g, float b, float a) {
    Color c;
    c.r = static_cast<uint8_t>(r * 255.0f);
    c.g = static_cast<uint8_t>(g * 255.0f);
    c.b = static_cast<uint8_t>(b * 255.0f);
    c.a = static_cast<uint8_t>(a * 255.0f);
    return c;
  }

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

  float Left() const { return x; }
  float Top() const { return y; }
  float Right() const { return x + width; }
  float Bottom() const { return y + height; }

  // Convert to Chromium rect format [left, top, right, bottom]
  std::array<float, 4> ToLTRB() const {
    return {x, y, x + width, y + height};
  }
};

// Visibility (CSS visibility property)
enum class Visibility { kVisible, kHidden, kCollapse };

// Skia paint styles
enum class PaintStyle {
  kFill = 0,
  kStroke = 1,
  kStrokeAndFill = 2
};

// Box shadow data (from CSS box-shadow)
struct BoxShadowData {
  float offset_x = 0.0f;
  float offset_y = 0.0f;
  float blur = 0.0f;
  float spread = 0.0f;
  bool inset = false;
  Color color;

  // Convert blur to sigma (Chromium convention: sigma = blur / 2)
  float BlurAsSigma() const {
    return blur / 2.0f;
  }
};

// Graphics state IDs for property trees
struct GraphicsStateIds {
  int transform_id = 0;
  int clip_id = 0;
  int effect_id = 0;
};

using DOMNodeId = int64_t;
constexpr DOMNodeId kInvalidDOMNodeId = 0;

// Border radii: 8 values for [tl_x, tl_y, tr_x, tr_y, br_x, br_y, bl_x, bl_y]
using BorderRadii = std::array<float, 8>;

// Check if radii are all zero
inline bool IsZeroRadii(const BorderRadii& radii) {
  for (float r : radii) {
    if (r != 0.0f) return false;
  }
  return true;
}

#endif  // BLOCK_PAINTER_TYPES_H_
