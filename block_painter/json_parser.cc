#include "json_parser.h"

#include <cctype>
#include <cstdlib>
#include <sstream>

namespace {

// Skip whitespace
size_t SkipWhitespace(const std::string& s, size_t pos) {
  while (pos < s.length() && std::isspace(s[pos])) {
    ++pos;
  }
  return pos;
}

// Find matching closing bracket/brace
size_t FindMatchingClose(const std::string& s, size_t start, char open,
                         char close) {
  int depth = 1;
  size_t pos = start + 1;
  while (pos < s.length() && depth > 0) {
    if (s[pos] == open)
      ++depth;
    else if (s[pos] == close)
      --depth;
    else if (s[pos] == '"') {
      // Skip string
      ++pos;
      while (pos < s.length() && s[pos] != '"') {
        if (s[pos] == '\\') ++pos;
        ++pos;
      }
    }
    ++pos;
  }
  return pos - 1;
}

// Split array elements
std::vector<std::string> SplitArrayElements(const std::string& array_content) {
  std::vector<std::string> elements;
  size_t pos = 0;
  int depth = 0;
  size_t start = 0;

  while (pos < array_content.length()) {
    char c = array_content[pos];
    if (c == '{' || c == '[') {
      ++depth;
    } else if (c == '}' || c == ']') {
      --depth;
    } else if (c == ',' && depth == 0) {
      std::string elem = array_content.substr(start, pos - start);
      size_t first = elem.find_first_not_of(" \t\n\r");
      size_t last = elem.find_last_not_of(" \t\n\r");
      if (first != std::string::npos) {
        elements.push_back(elem.substr(first, last - first + 1));
      }
      start = pos + 1;
    }
    ++pos;
  }

  // Last element
  if (start < array_content.length()) {
    std::string elem = array_content.substr(start);
    size_t first = elem.find_first_not_of(" \t\n\r");
    size_t last = elem.find_last_not_of(" \t\n\r");
    if (first != std::string::npos) {
      elements.push_back(elem.substr(first, last - first + 1));
    }
  }

  return elements;
}

}  // namespace

std::string JsonParser::ExtractString(const std::string& json,
                                      const std::string& key) {
  std::string search = "\"" + key + "\"";
  size_t pos = json.find(search);
  if (pos == std::string::npos) return "";

  pos = json.find(':', pos + search.length());
  if (pos == std::string::npos) return "";

  pos = SkipWhitespace(json, pos + 1);
  if (pos >= json.length() || json[pos] != '"') return "";

  size_t start = pos + 1;
  size_t end = start;
  while (end < json.length() && json[end] != '"') {
    if (json[end] == '\\') ++end;
    ++end;
  }

  return json.substr(start, end - start);
}

float JsonParser::ExtractFloat(const std::string& json, const std::string& key,
                               float default_value) {
  std::string search = "\"" + key + "\"";
  size_t pos = json.find(search);
  if (pos == std::string::npos) return default_value;

  pos = json.find(':', pos + search.length());
  if (pos == std::string::npos) return default_value;

  pos = SkipWhitespace(json, pos + 1);
  if (pos >= json.length()) return default_value;

  if (json.substr(pos, 4) == "null") return default_value;

  return std::strtof(json.c_str() + pos, nullptr);
}

int JsonParser::ExtractInt(const std::string& json, const std::string& key,
                           int default_value) {
  std::string search = "\"" + key + "\"";
  size_t pos = json.find(search);
  if (pos == std::string::npos) return default_value;

  pos = json.find(':', pos + search.length());
  if (pos == std::string::npos) return default_value;

  pos = SkipWhitespace(json, pos + 1);
  if (pos >= json.length()) return default_value;

  if (json.substr(pos, 4) == "null") return default_value;

  return std::atoi(json.c_str() + pos);
}

bool JsonParser::ExtractBool(const std::string& json, const std::string& key,
                             bool default_value) {
  std::string search = "\"" + key + "\"";
  size_t pos = json.find(search);
  if (pos == std::string::npos) return default_value;

  pos = json.find(':', pos + search.length());
  if (pos == std::string::npos) return default_value;

  pos = SkipWhitespace(json, pos + 1);
  if (pos >= json.length()) return default_value;

  if (json.substr(pos, 4) == "true") return true;
  if (json.substr(pos, 5) == "false") return false;

  return default_value;
}

std::string JsonParser::ExtractObject(const std::string& json,
                                      const std::string& key) {
  std::string search = "\"" + key + "\"";
  size_t pos = json.find(search);
  if (pos == std::string::npos) return "";

  pos = json.find(':', pos + search.length());
  if (pos == std::string::npos) return "";

  pos = SkipWhitespace(json, pos + 1);
  if (pos >= json.length()) return "";

  if (json[pos] == '{') {
    size_t end = FindMatchingClose(json, pos, '{', '}');
    return json.substr(pos, end - pos + 1);
  }

  return "";
}

std::string JsonParser::ExtractArray(const std::string& json,
                                     const std::string& key) {
  std::string search = "\"" + key + "\"";
  size_t pos = json.find(search);
  if (pos == std::string::npos) return "";

  pos = json.find(':', pos + search.length());
  if (pos == std::string::npos) return "";

  pos = SkipWhitespace(json, pos + 1);
  if (pos >= json.length()) return "";

  if (json[pos] == '[') {
    size_t end = FindMatchingClose(json, pos, '[', ']');
    return json.substr(pos + 1, end - pos - 1);  // Return content without []
  }

  return "";
}

bool JsonParser::HasKey(const std::string& json, const std::string& key) {
  std::string search = "\"" + key + "\"";
  return json.find(search) != std::string::npos;
}

std::vector<float> JsonParser::ParseFloatArray(const std::string& array_str) {
  std::vector<float> result;
  std::istringstream iss(array_str);
  std::string token;
  while (std::getline(iss, token, ',')) {
    size_t first = token.find_first_not_of(" \t\n\r[]");
    if (first != std::string::npos) {
      result.push_back(std::strtof(token.c_str() + first, nullptr));
    }
  }
  return result;
}

BoxShadowData JsonParser::ParseBoxShadow(const std::string& json) {
  BoxShadowData shadow;
  shadow.offset_x = ExtractFloat(json, "offset_x", 0.0f);
  shadow.offset_y = ExtractFloat(json, "offset_y", 0.0f);
  shadow.blur = ExtractFloat(json, "blur", 0.0f);
  shadow.spread = ExtractFloat(json, "spread", 0.0f);
  shadow.inset = ExtractBool(json, "inset", false);

  std::string color_obj = ExtractObject(json, "color");
  if (!color_obj.empty()) {
    shadow.color = Color::FromNormalized(
        ExtractFloat(color_obj, "r", 0.0f),
        ExtractFloat(color_obj, "g", 0.0f),
        ExtractFloat(color_obj, "b", 0.0f),
        ExtractFloat(color_obj, "a", 1.0f));
  }

  return shadow;
}

bool JsonParser::ParseInput(const std::string& json, BlockPaintInput& output) {
  // Parse geometry
  std::string geometry = ExtractObject(json, "geometry");
  if (!geometry.empty()) {
    output.geometry.x = ExtractFloat(geometry, "x", 0.0f);
    output.geometry.y = ExtractFloat(geometry, "y", 0.0f);
    output.geometry.width = ExtractFloat(geometry, "width", 0.0f);
    output.geometry.height = ExtractFloat(geometry, "height", 0.0f);
  }

  // Parse border_radii
  std::string radii_str = ExtractArray(json, "border_radii");
  if (!radii_str.empty()) {
    std::vector<float> radii = ParseFloatArray(radii_str);
    if (radii.size() >= 8) {
      BorderRadii br;
      for (int i = 0; i < 8; ++i) {
        br[i] = radii[i];
      }
      output.border_radii = br;
    }
  }

  // Parse background_color
  std::string bg_color = ExtractObject(json, "background_color");
  if (!bg_color.empty()) {
    output.background_color = Color::FromNormalized(
        ExtractFloat(bg_color, "r", 0.0f),
        ExtractFloat(bg_color, "g", 0.0f),
        ExtractFloat(bg_color, "b", 0.0f),
        ExtractFloat(bg_color, "a", 1.0f));
  }

  // Parse box_shadow
  std::string shadows_str = ExtractArray(json, "box_shadow");
  if (!shadows_str.empty()) {
    auto shadow_elements = SplitArrayElements(shadows_str);
    for (const auto& shadow_json : shadow_elements) {
      output.box_shadow.push_back(ParseBoxShadow(shadow_json));
    }
  }

  // Parse visibility
  std::string visibility = ExtractString(json, "visibility");
  if (visibility == "hidden") {
    output.visibility = Visibility::kHidden;
  } else if (visibility == "collapse") {
    output.visibility = Visibility::kCollapse;
  } else {
    output.visibility = Visibility::kVisible;
  }

  // Parse node_id
  output.node_id = ExtractInt(json, "node_id", kInvalidDOMNodeId);

  // Parse state_ids
  std::string state_ids = ExtractObject(json, "state_ids");
  if (!state_ids.empty()) {
    output.state_ids.transform_id = ExtractInt(state_ids, "transform_id", 0);
    output.state_ids.clip_id = ExtractInt(state_ids, "clip_id", 0);
    output.state_ids.effect_id = ExtractInt(state_ids, "effect_id", 0);
  }

  return true;
}

std::string JsonParser::SerializeOps(const PaintOpList& ops) {
  std::ostringstream oss;
  oss << "[\n";

  for (size_t i = 0; i < ops.ops.size(); ++i) {
    if (i > 0) oss << ",\n";

    const auto& op = ops.ops[i];

    std::visit(
        [&oss](auto&& arg) {
          using T = std::decay_t<decltype(arg)>;

          if constexpr (std::is_same_v<T, DrawRectOp>) {
            oss << "  {\n"
                << "    \"type\": \"DrawRectOp\",\n"
                << "    \"rect\": [" << arg.rect[0] << ", " << arg.rect[1]
                << ", " << arg.rect[2] << ", " << arg.rect[3] << "],\n"
                << "    \"flags\": {\n"
                << "      \"r\": " << arg.flags.r << ",\n"
                << "      \"g\": " << arg.flags.g << ",\n"
                << "      \"b\": " << arg.flags.b << ",\n"
                << "      \"a\": " << arg.flags.a << ",\n"
                << "      \"style\": " << arg.flags.style << ",\n"
                << "      \"strokeWidth\": " << arg.flags.stroke_width << ",\n"
                << "      \"strokeCap\": " << arg.flags.stroke_cap << ",\n"
                << "      \"strokeJoin\": " << arg.flags.stroke_join;
            if (!arg.flags.shadows.empty()) {
              oss << ",\n      \"shadows\": [\n";
              for (size_t j = 0; j < arg.flags.shadows.size(); ++j) {
                if (j > 0) oss << ",\n";
                const auto& s = arg.flags.shadows[j];
                oss << "        {\n"
                    << "          \"offsetX\": " << s.offset_x << ",\n"
                    << "          \"offsetY\": " << s.offset_y << ",\n"
                    << "          \"blurSigma\": " << s.blur_sigma << ",\n"
                    << "          \"r\": " << s.color.R() << ",\n"
                    << "          \"g\": " << s.color.G() << ",\n"
                    << "          \"b\": " << s.color.B() << ",\n"
                    << "          \"a\": " << s.color.A() << ",\n"
                    << "          \"flags\": " << s.flags << "\n"
                    << "        }";
              }
              oss << "\n      ]";
            }
            oss << "\n    },\n"
                << "    \"transform_id\": " << arg.transform_id << ",\n"
                << "    \"clip_id\": " << arg.clip_id << ",\n"
                << "    \"effect_id\": " << arg.effect_id << "\n"
                << "  }";
          } else if constexpr (std::is_same_v<T, DrawRRectOp>) {
            oss << "  {\n"
                << "    \"type\": \"DrawRRectOp\",\n"
                << "    \"rect\": [" << arg.rect[0] << ", " << arg.rect[1]
                << ", " << arg.rect[2] << ", " << arg.rect[3] << "],\n"
                << "    \"radii\": [" << arg.radii[0] << ", " << arg.radii[1]
                << ", " << arg.radii[2] << ", " << arg.radii[3]
                << ", " << arg.radii[4] << ", " << arg.radii[5]
                << ", " << arg.radii[6] << ", " << arg.radii[7] << "],\n"
                << "    \"flags\": {\n"
                << "      \"r\": " << arg.flags.r << ",\n"
                << "      \"g\": " << arg.flags.g << ",\n"
                << "      \"b\": " << arg.flags.b << ",\n"
                << "      \"a\": " << arg.flags.a << ",\n"
                << "      \"style\": " << arg.flags.style << ",\n"
                << "      \"strokeWidth\": " << arg.flags.stroke_width << ",\n"
                << "      \"strokeCap\": " << arg.flags.stroke_cap << ",\n"
                << "      \"strokeJoin\": " << arg.flags.stroke_join;
            if (!arg.flags.shadows.empty()) {
              oss << ",\n      \"shadows\": [\n";
              for (size_t j = 0; j < arg.flags.shadows.size(); ++j) {
                if (j > 0) oss << ",\n";
                const auto& s = arg.flags.shadows[j];
                oss << "        {\n"
                    << "          \"offsetX\": " << s.offset_x << ",\n"
                    << "          \"offsetY\": " << s.offset_y << ",\n"
                    << "          \"blurSigma\": " << s.blur_sigma << ",\n"
                    << "          \"r\": " << s.color.R() << ",\n"
                    << "          \"g\": " << s.color.G() << ",\n"
                    << "          \"b\": " << s.color.B() << ",\n"
                    << "          \"a\": " << s.color.A() << ",\n"
                    << "          \"flags\": " << s.flags << "\n"
                    << "        }";
              }
              oss << "\n      ]";
            }
            oss << "\n    },\n"
                << "    \"transform_id\": " << arg.transform_id << ",\n"
                << "    \"clip_id\": " << arg.clip_id << ",\n"
                << "    \"effect_id\": " << arg.effect_id << "\n"
                << "  }";
          } else if constexpr (std::is_same_v<T, ClipRRectOp>) {
            oss << "  {\n"
                << "    \"type\": \"ClipRRectOp\",\n"
                << "    \"rect\": [" << arg.rect[0] << ", " << arg.rect[1]
                << ", " << arg.rect[2] << ", " << arg.rect[3] << "],\n"
                << "    \"radii\": [" << arg.radii[0] << ", " << arg.radii[1]
                << ", " << arg.radii[2] << ", " << arg.radii[3]
                << ", " << arg.radii[4] << ", " << arg.radii[5]
                << ", " << arg.radii[6] << ", " << arg.radii[7] << "],\n"
                << "    \"antiAlias\": " << (arg.anti_alias ? "true" : "false") << ",\n"
                << "    \"clipOp\": " << arg.clip_op << ",\n"
                << "    \"transform_id\": " << arg.transform_id << ",\n"
                << "    \"clip_id\": " << arg.clip_id << ",\n"
                << "    \"effect_id\": " << arg.effect_id << "\n"
                << "  }";
          } else if constexpr (std::is_same_v<T, SaveOp>) {
            oss << "  {\n"
                << "    \"type\": \"SaveOp\",\n"
                << "    \"transform_id\": " << arg.transform_id << ",\n"
                << "    \"clip_id\": " << arg.clip_id << ",\n"
                << "    \"effect_id\": " << arg.effect_id << "\n"
                << "  }";
          } else if constexpr (std::is_same_v<T, RestoreOp>) {
            oss << "  {\n"
                << "    \"type\": \"RestoreOp\",\n"
                << "    \"transform_id\": " << arg.transform_id << ",\n"
                << "    \"clip_id\": " << arg.clip_id << ",\n"
                << "    \"effect_id\": " << arg.effect_id << "\n"
                << "  }";
          }
        },
        op);
  }

  oss << "\n]";
  return oss.str();
}
