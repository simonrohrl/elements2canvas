#include "json_parser.h"

#include <sstream>
#include <stdexcept>

namespace border_painter {
namespace {

// Simple JSON tokenizer
class JsonTokenizer {
 public:
  explicit JsonTokenizer(const std::string& json) : json_(json), pos_(0) {}

  void SkipWhitespace() {
    while (pos_ < json_.size() &&
           (json_[pos_] == ' ' || json_[pos_] == '\n' ||
            json_[pos_] == '\r' || json_[pos_] == '\t')) {
      ++pos_;
    }
  }

  char Peek() {
    SkipWhitespace();
    return pos_ < json_.size() ? json_[pos_] : '\0';
  }

  char Consume() {
    SkipWhitespace();
    return pos_ < json_.size() ? json_[pos_++] : '\0';
  }

  void Expect(char c) {
    SkipWhitespace();
    if (pos_ >= json_.size() || json_[pos_] != c) {
      throw std::runtime_error(std::string("Expected '") + c + "'");
    }
    ++pos_;
  }

  std::string ReadString() {
    Expect('"');
    std::string result;
    while (pos_ < json_.size() && json_[pos_] != '"') {
      if (json_[pos_] == '\\' && pos_ + 1 < json_.size()) {
        ++pos_;
        switch (json_[pos_]) {
          case 'n': result += '\n'; break;
          case 't': result += '\t'; break;
          case 'r': result += '\r'; break;
          case '"': result += '"'; break;
          case '\\': result += '\\'; break;
          default: result += json_[pos_]; break;
        }
      } else {
        result += json_[pos_];
      }
      ++pos_;
    }
    Expect('"');
    return result;
  }

  double ReadNumber() {
    SkipWhitespace();
    size_t start = pos_;
    if (pos_ < json_.size() && json_[pos_] == '-') ++pos_;
    while (pos_ < json_.size() &&
           (std::isdigit(json_[pos_]) || json_[pos_] == '.' ||
            json_[pos_] == 'e' || json_[pos_] == 'E' ||
            json_[pos_] == '+' || json_[pos_] == '-')) {
      ++pos_;
    }
    return std::stod(json_.substr(start, pos_ - start));
  }

  bool ReadBool() {
    SkipWhitespace();
    if (json_.substr(pos_, 4) == "true") {
      pos_ += 4;
      return true;
    }
    if (json_.substr(pos_, 5) == "false") {
      pos_ += 5;
      return false;
    }
    throw std::runtime_error("Expected boolean");
  }

  void SkipValue() {
    SkipWhitespace();
    char c = Peek();
    if (c == '"') {
      ReadString();
    } else if (c == '{') {
      SkipObject();
    } else if (c == '[') {
      SkipArray();
    } else if (c == 't' || c == 'f') {
      ReadBool();
    } else if (c == 'n') {
      pos_ += 4;  // "null"
    } else {
      ReadNumber();
    }
  }

  void SkipObject() {
    Expect('{');
    if (Peek() != '}') {
      do {
        ReadString();
        Expect(':');
        SkipValue();
      } while (Peek() == ',' && (Consume(), true));
    }
    Expect('}');
  }

  void SkipArray() {
    Expect('[');
    if (Peek() != ']') {
      do {
        SkipValue();
      } while (Peek() == ',' && (Consume(), true));
    }
    Expect(']');
  }

 private:
  const std::string& json_;
  size_t pos_;
};

Color ParseColor(JsonTokenizer& tok) {
  Color color;
  tok.Expect('{');
  while (tok.Peek() != '}') {
    std::string key = tok.ReadString();
    tok.Expect(':');
    if (key == "r") {
      color.r = static_cast<float>(tok.ReadNumber());
    } else if (key == "g") {
      color.g = static_cast<float>(tok.ReadNumber());
    } else if (key == "b") {
      color.b = static_cast<float>(tok.ReadNumber());
    } else if (key == "a") {
      color.a = static_cast<float>(tok.ReadNumber());
    } else {
      tok.SkipValue();
    }
    if (tok.Peek() == ',') tok.Consume();
  }
  tok.Expect('}');
  return color;
}

RectF ParseGeometry(JsonTokenizer& tok) {
  RectF rect;
  tok.Expect('{');
  while (tok.Peek() != '}') {
    std::string key = tok.ReadString();
    tok.Expect(':');
    if (key == "x") {
      rect.x = static_cast<float>(tok.ReadNumber());
    } else if (key == "y") {
      rect.y = static_cast<float>(tok.ReadNumber());
    } else if (key == "width") {
      rect.width = static_cast<float>(tok.ReadNumber());
    } else if (key == "height") {
      rect.height = static_cast<float>(tok.ReadNumber());
    } else {
      tok.SkipValue();
    }
    if (tok.Peek() == ',') tok.Consume();
  }
  tok.Expect('}');
  return rect;
}

BorderWidths ParseBorderWidths(JsonTokenizer& tok) {
  BorderWidths bw;
  tok.Expect('{');
  while (tok.Peek() != '}') {
    std::string key = tok.ReadString();
    tok.Expect(':');
    if (key == "top") {
      bw.top = static_cast<float>(tok.ReadNumber());
    } else if (key == "right") {
      bw.right = static_cast<float>(tok.ReadNumber());
    } else if (key == "bottom") {
      bw.bottom = static_cast<float>(tok.ReadNumber());
    } else if (key == "left") {
      bw.left = static_cast<float>(tok.ReadNumber());
    } else {
      tok.SkipValue();
    }
    if (tok.Peek() == ',') tok.Consume();
  }
  tok.Expect('}');
  return bw;
}

BorderColors ParseBorderColors(JsonTokenizer& tok) {
  BorderColors bc;
  tok.Expect('{');
  while (tok.Peek() != '}') {
    std::string key = tok.ReadString();
    tok.Expect(':');
    if (key == "top") {
      bc.top = ParseColor(tok);
    } else if (key == "right") {
      bc.right = ParseColor(tok);
    } else if (key == "bottom") {
      bc.bottom = ParseColor(tok);
    } else if (key == "left") {
      bc.left = ParseColor(tok);
    } else {
      tok.SkipValue();
    }
    if (tok.Peek() == ',') tok.Consume();
  }
  tok.Expect('}');
  return bc;
}

BorderRadii ParseBorderRadii(JsonTokenizer& tok) {
  BorderRadii radii = {};
  tok.Expect('[');
  size_t i = 0;
  while (tok.Peek() != ']' && i < 8) {
    radii[i++] = static_cast<float>(tok.ReadNumber());
    if (tok.Peek() == ',') tok.Consume();
  }
  tok.Expect(']');
  return radii;
}

EBorderStyle ParseBorderStyle(const std::string& style_str) {
  if (style_str == "none") return EBorderStyle::kNone;
  if (style_str == "hidden") return EBorderStyle::kHidden;
  if (style_str == "inset") return EBorderStyle::kInset;
  if (style_str == "groove") return EBorderStyle::kGroove;
  if (style_str == "outset") return EBorderStyle::kOutset;
  if (style_str == "ridge") return EBorderStyle::kRidge;
  if (style_str == "dotted") return EBorderStyle::kDotted;
  if (style_str == "dashed") return EBorderStyle::kDashed;
  if (style_str == "solid") return EBorderStyle::kSolid;
  if (style_str == "double") return EBorderStyle::kDouble;
  return EBorderStyle::kSolid;  // Default
}

BorderStyles ParseBorderStyles(JsonTokenizer& tok) {
  BorderStyles bs;
  tok.Expect('{');
  while (tok.Peek() != '}') {
    std::string key = tok.ReadString();
    tok.Expect(':');
    std::string style_str = tok.ReadString();
    if (key == "top") {
      bs.top = ParseBorderStyle(style_str);
    } else if (key == "right") {
      bs.right = ParseBorderStyle(style_str);
    } else if (key == "bottom") {
      bs.bottom = ParseBorderStyle(style_str);
    } else if (key == "left") {
      bs.left = ParseBorderStyle(style_str);
    }
    if (tok.Peek() == ',') tok.Consume();
  }
  tok.Expect('}');
  return bs;
}

GraphicsStateIds ParseStateIds(JsonTokenizer& tok) {
  GraphicsStateIds ids;
  tok.Expect('{');
  while (tok.Peek() != '}') {
    std::string key = tok.ReadString();
    tok.Expect(':');
    if (key == "transform_id") {
      ids.transform_id = static_cast<int>(tok.ReadNumber());
    } else if (key == "clip_id") {
      ids.clip_id = static_cast<int>(tok.ReadNumber());
    } else if (key == "effect_id") {
      ids.effect_id = static_cast<int>(tok.ReadNumber());
    } else {
      tok.SkipValue();
    }
    if (tok.Peek() == ',') tok.Consume();
  }
  tok.Expect('}');
  return ids;
}

std::string FloatToString(float f) {
  std::ostringstream oss;
  oss.precision(10);
  oss << f;
  return oss.str();
}

}  // namespace

BorderPaintInput ParseInput(const std::string& json_str) {
  BorderPaintInput input;
  JsonTokenizer tok(json_str);

  tok.Expect('{');
  while (tok.Peek() != '}') {
    std::string key = tok.ReadString();
    tok.Expect(':');

    if (key == "geometry") {
      input.geometry = ParseGeometry(tok);
    } else if (key == "border_widths") {
      input.border_widths = ParseBorderWidths(tok);
    } else if (key == "border_colors") {
      input.border_colors = ParseBorderColors(tok);
    } else if (key == "border_radii") {
      input.border_radii = ParseBorderRadii(tok);
    } else if (key == "border_styles") {
      input.border_styles = ParseBorderStyles(tok);
    } else if (key == "visibility") {
      std::string vis = tok.ReadString();
      if (vis == "hidden") {
        input.visibility = Visibility::kHidden;
      } else if (vis == "collapse") {
        input.visibility = Visibility::kCollapse;
      } else {
        input.visibility = Visibility::kVisible;
      }
    } else if (key == "node_id") {
      input.node_id = static_cast<DOMNodeId>(tok.ReadNumber());
    } else if (key == "state_ids") {
      input.state_ids = ParseStateIds(tok);
    } else if (key == "match_type") {
      std::string mt = tok.ReadString();
      if (mt == "stroked_rect") {
        input.render_hint = BorderRenderHint::kStrokedRect;
      } else if (mt == "draw_line") {
        input.render_hint = BorderRenderHint::kDrawLine;
      } else if (mt == "filled_thin_rect") {
        input.render_hint = BorderRenderHint::kFilledThinRect;
      } else if (mt == "double_stroked") {
        input.render_hint = BorderRenderHint::kDoubleStroked;
      } else if (mt == "dotted_lines") {
        input.render_hint = BorderRenderHint::kDottedLines;
      } else if (mt == "groove_ridge") {
        input.render_hint = BorderRenderHint::kGrooveRidge;
      } else {
        input.render_hint = BorderRenderHint::kAuto;
      }
    } else {
      tok.SkipValue();
    }

    if (tok.Peek() == ',') tok.Consume();
  }
  tok.Expect('}');

  return input;
}

std::string SerializeOps(const PaintOpList& ops) {
  std::ostringstream out;
  out << "[\n";

  bool first = true;
  for (const auto& op : ops.ops()) {
    if (!first) out << ",\n";
    first = false;

    std::visit([&out](auto&& arg) {
      using T = std::decay_t<decltype(arg)>;

      if constexpr (std::is_same_v<T, DrawRectOp>) {
        out << "  {\n";
        out << "    \"type\": \"DrawRectOp\",\n";
        out << "    \"rect\": [" << FloatToString(arg.rect[0]) << ", "
            << FloatToString(arg.rect[1]) << ", "
            << FloatToString(arg.rect[2]) << ", "
            << FloatToString(arg.rect[3]) << "],\n";
        out << "    \"flags\": {\n";
        out << "      \"r\": " << FloatToString(arg.flags.color.r) << ",\n";
        out << "      \"g\": " << FloatToString(arg.flags.color.g) << ",\n";
        out << "      \"b\": " << FloatToString(arg.flags.color.b) << ",\n";
        out << "      \"a\": " << FloatToString(arg.flags.color.a) << ",\n";
        out << "      \"style\": " << static_cast<int>(arg.flags.style) << ",\n";
        out << "      \"strokeWidth\": " << FloatToString(arg.flags.stroke_width) << ",\n";
        out << "      \"strokeCap\": " << static_cast<int>(arg.flags.stroke_cap) << ",\n";
        out << "      \"strokeJoin\": " << static_cast<int>(arg.flags.stroke_join) << "\n";
        out << "    },\n";
        out << "    \"transform_id\": " << arg.transform_id << ",\n";
        out << "    \"clip_id\": " << arg.clip_id << ",\n";
        out << "    \"effect_id\": " << arg.effect_id << "\n";
        out << "  }";
      } else if constexpr (std::is_same_v<T, DrawRRectOp>) {
        out << "  {\n";
        out << "    \"type\": \"DrawRRectOp\",\n";
        out << "    \"rect\": [" << FloatToString(arg.rect[0]) << ", "
            << FloatToString(arg.rect[1]) << ", "
            << FloatToString(arg.rect[2]) << ", "
            << FloatToString(arg.rect[3]) << "],\n";
        out << "    \"radii\": [";
        for (size_t i = 0; i < arg.radii.size(); ++i) {
          if (i > 0) out << ", ";
          out << FloatToString(arg.radii[i]);
        }
        out << "],\n";
        out << "    \"flags\": {\n";
        out << "      \"r\": " << FloatToString(arg.flags.color.r) << ",\n";
        out << "      \"g\": " << FloatToString(arg.flags.color.g) << ",\n";
        out << "      \"b\": " << FloatToString(arg.flags.color.b) << ",\n";
        out << "      \"a\": " << FloatToString(arg.flags.color.a) << ",\n";
        out << "      \"style\": " << static_cast<int>(arg.flags.style) << ",\n";
        out << "      \"strokeWidth\": " << FloatToString(arg.flags.stroke_width) << ",\n";
        out << "      \"strokeCap\": " << static_cast<int>(arg.flags.stroke_cap) << ",\n";
        out << "      \"strokeJoin\": " << static_cast<int>(arg.flags.stroke_join) << "\n";
        out << "    },\n";
        out << "    \"transform_id\": " << arg.transform_id << ",\n";
        out << "    \"clip_id\": " << arg.clip_id << ",\n";
        out << "    \"effect_id\": " << arg.effect_id << "\n";
        out << "  }";
      } else if constexpr (std::is_same_v<T, DrawLineOp>) {
        out << "  {\n";
        out << "    \"type\": \"DrawLineOp\",\n";
        out << "    \"x0\": " << FloatToString(arg.x0) << ",\n";
        out << "    \"y0\": " << FloatToString(arg.y0) << ",\n";
        out << "    \"x1\": " << FloatToString(arg.x1) << ",\n";
        out << "    \"y1\": " << FloatToString(arg.y1) << ",\n";
        out << "    \"flags\": {\n";
        out << "      \"r\": " << FloatToString(arg.flags.color.r) << ",\n";
        out << "      \"g\": " << FloatToString(arg.flags.color.g) << ",\n";
        out << "      \"b\": " << FloatToString(arg.flags.color.b) << ",\n";
        out << "      \"a\": " << FloatToString(arg.flags.color.a) << ",\n";
        out << "      \"style\": " << static_cast<int>(arg.flags.style) << ",\n";
        out << "      \"strokeWidth\": " << FloatToString(arg.flags.stroke_width) << ",\n";
        out << "      \"strokeCap\": " << static_cast<int>(arg.flags.stroke_cap) << ",\n";
        out << "      \"strokeJoin\": " << static_cast<int>(arg.flags.stroke_join) << "\n";
        out << "    },\n";
        out << "    \"transform_id\": " << arg.transform_id << ",\n";
        out << "    \"clip_id\": " << arg.clip_id << ",\n";
        out << "    \"effect_id\": " << arg.effect_id << "\n";
        out << "  }";
      } else if constexpr (std::is_same_v<T, DrawDRRectOp>) {
        out << "  {\n";
        out << "    \"type\": \"DrawDRRectOp\",\n";
        out << "    \"outer_rect\": [" << FloatToString(arg.outer_rect[0]) << ", "
            << FloatToString(arg.outer_rect[1]) << ", "
            << FloatToString(arg.outer_rect[2]) << ", "
            << FloatToString(arg.outer_rect[3]) << "],\n";
        out << "    \"outer_radii\": [";
        for (size_t i = 0; i < arg.outer_radii.size(); ++i) {
          if (i > 0) out << ", ";
          out << FloatToString(arg.outer_radii[i]);
        }
        out << "],\n";
        out << "    \"inner_rect\": [" << FloatToString(arg.inner_rect[0]) << ", "
            << FloatToString(arg.inner_rect[1]) << ", "
            << FloatToString(arg.inner_rect[2]) << ", "
            << FloatToString(arg.inner_rect[3]) << "],\n";
        out << "    \"inner_radii\": [";
        for (size_t i = 0; i < arg.inner_radii.size(); ++i) {
          if (i > 0) out << ", ";
          out << FloatToString(arg.inner_radii[i]);
        }
        out << "],\n";
        out << "    \"flags\": {\n";
        out << "      \"r\": " << FloatToString(arg.flags.color.r) << ",\n";
        out << "      \"g\": " << FloatToString(arg.flags.color.g) << ",\n";
        out << "      \"b\": " << FloatToString(arg.flags.color.b) << ",\n";
        out << "      \"a\": " << FloatToString(arg.flags.color.a) << ",\n";
        out << "      \"style\": " << static_cast<int>(arg.flags.style) << "\n";
        out << "    },\n";
        out << "    \"transform_id\": " << arg.transform_id << ",\n";
        out << "    \"clip_id\": " << arg.clip_id << ",\n";
        out << "    \"effect_id\": " << arg.effect_id << "\n";
        out << "  }";
      }
    }, op);
  }

  out << "\n]\n";
  return out.str();
}

}  // namespace border_painter
