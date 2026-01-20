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

// Split array elements (simple: assumes no nested arrays in elements)
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
      // Trim whitespace
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

bool JsonParser::IsNull(const std::string& json, const std::string& key) {
  std::string search = "\"" + key + "\"";
  size_t pos = json.find(search);
  if (pos == std::string::npos) return true;

  pos = json.find(':', pos + search.length());
  if (pos == std::string::npos) return true;

  pos = SkipWhitespace(json, pos + 1);
  if (pos >= json.length()) return true;

  return json.substr(pos, 4) == "null";
}

std::vector<uint16_t> JsonParser::ParseIntArray(const std::string& array_str) {
  std::vector<uint16_t> result;
  std::istringstream iss(array_str);
  std::string token;
  while (std::getline(iss, token, ',')) {
    size_t first = token.find_first_not_of(" \t\n\r[]");
    if (first != std::string::npos) {
      result.push_back(static_cast<uint16_t>(std::atoi(token.c_str() + first)));
    }
  }
  return result;
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

GlyphRun JsonParser::ParseGlyphRun(const std::string& json) {
  GlyphRun run;

  // Parse font
  std::string font_json = ExtractObject(json, "font");
  if (!font_json.empty()) {
    run.font.family = ExtractString(font_json, "family");
    run.font.size = ExtractFloat(font_json, "size", 16.0f);
    run.font.weight = ExtractInt(font_json, "weight", 400);
    run.font.width = ExtractInt(font_json, "width", 5);
    run.font.slant = ExtractInt(font_json, "slant", 0);
    run.font.scale_x = ExtractFloat(font_json, "scaleX", 1.0f);
    run.font.skew_x = ExtractFloat(font_json, "skewX", 0.0f);
    run.font.embolden = ExtractBool(font_json, "embolden", false);
    run.font.linear_metrics = ExtractBool(font_json, "linearMetrics", true);
    run.font.subpixel = ExtractBool(font_json, "subpixel", true);
    run.font.force_auto_hinting = ExtractBool(font_json, "forceAutoHinting", false);
    run.font.typeface_id = ExtractInt(font_json, "typefaceId", 0);
    run.font.ascent = ExtractFloat(font_json, "ascent", 0.0f);
    run.font.descent = ExtractFloat(font_json, "descent", 0.0f);

    // Optional font-supplied underline metrics
    if (!IsNull(font_json, "underline_position")) {
      run.font.underline_position = ExtractFloat(font_json, "underline_position", 0.0f);
    }
    if (!IsNull(font_json, "underline_thickness")) {
      run.font.underline_thickness = ExtractFloat(font_json, "underline_thickness", 0.0f);
    }
  }

  // Parse glyphs
  std::string glyphs_str = ExtractArray(json, "glyphs");
  run.glyphs = ParseIntArray(glyphs_str);

  // Parse positions
  std::string positions_str = ExtractArray(json, "positions");
  run.positions = ParseFloatArray(positions_str);

  run.offset_x = ExtractFloat(json, "offsetX", 0.0f);
  run.offset_y = ExtractFloat(json, "offsetY", 0.0f);
  run.positioning = ExtractInt(json, "positioning", 1);

  return run;
}

bool JsonParser::ParseInput(const std::string& json, TextPaintInput& output) {
  // Parse fragment
  std::string fragment = ExtractObject(json, "fragment");
  if (!fragment.empty()) {
    output.fragment.text = ExtractString(fragment, "text");
    output.fragment.from = ExtractInt(fragment, "from", 0);
    output.fragment.to = ExtractInt(fragment, "to", 0);

    // Parse shape_result
    std::string shape_result = ExtractObject(fragment, "shape_result");
    if (!shape_result.empty()) {
      // Parse bounds
      std::string bounds = ExtractObject(shape_result, "bounds");
      if (!bounds.empty()) {
        output.fragment.shape_result.bounds.x = ExtractFloat(bounds, "x", 0.0f);
        output.fragment.shape_result.bounds.y = ExtractFloat(bounds, "y", 0.0f);
        output.fragment.shape_result.bounds.width = ExtractFloat(bounds, "width", 0.0f);
        output.fragment.shape_result.bounds.height = ExtractFloat(bounds, "height", 0.0f);
      }

      // Parse runs
      std::string runs_str = ExtractArray(shape_result, "runs");
      if (!runs_str.empty()) {
        auto run_elements = SplitArrayElements(runs_str);
        for (const auto& run_json : run_elements) {
          output.fragment.shape_result.runs.push_back(ParseGlyphRun(run_json));
        }
      }
    }
  }

  // Parse box
  std::string box = ExtractObject(json, "box");
  if (!box.empty()) {
    output.box.x = ExtractFloat(box, "x", 0.0f);
    output.box.y = ExtractFloat(box, "y", 0.0f);
    output.box.width = ExtractFloat(box, "width", 0.0f);
    output.box.height = ExtractFloat(box, "height", 0.0f);
  }

  // Parse style
  std::string style = ExtractObject(json, "style");
  if (!style.empty()) {
    output.style.fill_color = Color::FromHex(ExtractString(style, "fill_color"));
    output.style.stroke_color = Color::FromHex(ExtractString(style, "stroke_color"));
    output.style.stroke_width = ExtractFloat(style, "stroke_width", 0.0f);
    output.style.emphasis_mark_color =
        Color::FromHex(ExtractString(style, "emphasis_mark_color"));
    output.style.current_color = Color::FromHex(ExtractString(style, "current_color"));

    std::string color_scheme = ExtractString(style, "color_scheme");
    output.style.color_scheme =
        (color_scheme == "dark") ? ColorScheme::kDark : ColorScheme::kLight;

    std::string paint_order = ExtractString(style, "paint_order");
    if (paint_order == "stroke_fill") {
      output.style.paint_order = EPaintOrder::kPaintOrderStrokeFillMarkers;
    } else {
      output.style.paint_order = EPaintOrder::kPaintOrderNormal;
    }

    output.style.shadow = std::nullopt;
  }

  // Parse paint_phase
  std::string phase = ExtractString(json, "paint_phase");
  output.paint_phase =
      (phase == "text_clip") ? PaintPhase::kTextClip : PaintPhase::kForeground;

  // Parse node_id
  output.node_id = ExtractInt(json, "node_id", kInvalidDOMNodeId);

  // Parse state_ids
  std::string state_ids = ExtractObject(json, "state_ids");
  if (!state_ids.empty()) {
    output.state_ids.transform_id = ExtractInt(state_ids, "transform_id", 0);
    output.state_ids.clip_id = ExtractInt(state_ids, "clip_id", 0);
    output.state_ids.effect_id = ExtractInt(state_ids, "effect_id", 0);
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

  // Parse writing_mode
  std::string writing_mode = ExtractString(json, "writing_mode");
  if (writing_mode == "vertical-rl") {
    output.writing_mode = WritingMode::kVerticalRl;
    output.is_horizontal = false;
  } else if (writing_mode == "vertical-lr") {
    output.writing_mode = WritingMode::kVerticalLr;
    output.is_horizontal = false;
  } else {
    output.writing_mode = WritingMode::kHorizontalTb;
    output.is_horizontal = true;
  }

  // Parse is_horizontal override if present
  if (!IsNull(json, "is_horizontal")) {
    output.is_horizontal = ExtractBool(json, "is_horizontal", output.is_horizontal);
  }

  // Parse decorations
  std::string decorations_str = ExtractArray(json, "decorations");
  if (!decorations_str.empty()) {
    auto decoration_elements = SplitArrayElements(decorations_str);
    for (const auto& dec_json : decoration_elements) {
      TextDecoration decoration;

      std::string line_str = ExtractString(dec_json, "line");
      if (line_str == "underline") {
        decoration.line = TextDecorationLine::kUnderline;
      } else if (line_str == "overline") {
        decoration.line = TextDecorationLine::kOverline;
      } else if (line_str == "line-through") {
        decoration.line = TextDecorationLine::kLineThrough;
      } else if (line_str == "spelling-error") {
        decoration.line = TextDecorationLine::kSpellingError;
      } else if (line_str == "grammar-error") {
        decoration.line = TextDecorationLine::kGrammarError;
      }

      std::string style_str = ExtractString(dec_json, "style");
      if (style_str == "double") {
        decoration.style = TextDecorationStyle::kDouble;
      } else if (style_str == "dotted") {
        decoration.style = TextDecorationStyle::kDotted;
      } else if (style_str == "dashed") {
        decoration.style = TextDecorationStyle::kDashed;
      } else if (style_str == "wavy") {
        decoration.style = TextDecorationStyle::kWavy;
      } else {
        decoration.style = TextDecorationStyle::kSolid;
      }

      decoration.color = Color::FromHex(ExtractString(dec_json, "color"));
      decoration.thickness = ExtractFloat(dec_json, "thickness", 1.0f);
      decoration.underline_offset = ExtractFloat(dec_json, "underline_offset", 0.0f);

      output.decorations.push_back(decoration);
    }
  }

  // Parse emphasis_mark
  std::string emphasis = ExtractObject(json, "emphasis_mark");
  if (!emphasis.empty()) {
    EmphasisMarkInfo info;
    info.mark = ExtractString(emphasis, "mark");
    info.offset = ExtractFloat(emphasis, "offset", 0.0f);
    std::string side = ExtractString(emphasis, "side");
    info.side = (side == "under") ? LineLogicalSide::kUnder : LineLogicalSide::kOver;
    info.has_annotation_on_same_side = ExtractBool(emphasis, "has_annotation_on_same_side", false);
    output.emphasis_mark = info;
  }

  // Parse symbol_marker
  std::string marker = ExtractObject(json, "symbol_marker");
  if (!marker.empty()) {
    SymbolMarkerInfo info;
    std::string type_str = ExtractString(marker, "type");
    if (type_str == "disc") {
      info.type = SymbolMarkerType::kDisc;
    } else if (type_str == "circle") {
      info.type = SymbolMarkerType::kCircle;
    } else if (type_str == "square") {
      info.type = SymbolMarkerType::kSquare;
    } else if (type_str == "disclosure-open") {
      info.type = SymbolMarkerType::kDisclosureOpen;
      info.is_open = true;
    } else if (type_str == "disclosure-closed") {
      info.type = SymbolMarkerType::kDisclosureClosed;
      info.is_open = false;
    }

    std::string rect = ExtractObject(marker, "rect");
    if (!rect.empty()) {
      info.marker_rect.x = ExtractFloat(rect, "x", 0.0f);
      info.marker_rect.y = ExtractFloat(rect, "y", 0.0f);
      info.marker_rect.width = ExtractFloat(rect, "width", 0.0f);
      info.marker_rect.height = ExtractFloat(rect, "height", 0.0f);
    }
    info.color = Color::FromHex(ExtractString(marker, "color"));
    output.symbol_marker = info;
  }

  // Parse svg_info
  std::string svg = ExtractObject(json, "svg_info");
  if (!svg.empty()) {
    SvgTextInfo info;
    info.scaling_factor = ExtractFloat(svg, "scaling_factor", 1.0f);
    info.has_transform = ExtractBool(svg, "has_transform", false);
    info.length_adjust_scale = ExtractFloat(svg, "length_adjust_scale", 1.0f);
    // Transform matrix parsing would go here if needed
    output.svg_info = info;
  }

  // Parse text_combine
  std::string combine = ExtractObject(json, "text_combine");
  if (!combine.empty()) {
    TextCombineInfo info;
    info.is_combined = ExtractBool(combine, "is_combined", false);
    info.compressed_font_scale = ExtractFloat(combine, "compressed_font_scale", 1.0f);
    info.text_left_adjustment = ExtractFloat(combine, "text_left_adjustment", 0.0f);
    info.text_top_adjustment = ExtractFloat(combine, "text_top_adjustment", 0.0f);
    output.text_combine = info;
  }

  // Parse dark_mode
  std::string dark_mode = ExtractObject(json, "dark_mode");
  if (!dark_mode.empty()) {
    output.dark_mode.enabled = ExtractBool(dark_mode, "enabled", false);
  }

  // Parse flags
  output.is_ellipsis = ExtractBool(json, "is_ellipsis", false);
  output.is_line_break = ExtractBool(json, "is_line_break", false);
  output.is_flow_control = ExtractBool(json, "is_flow_control", false);

  // Parse shadows in style
  if (!style.empty()) {
    std::string shadows_str = ExtractArray(style, "shadow");
    if (!shadows_str.empty()) {
      std::vector<ShadowData> shadows;
      auto shadow_elements = SplitArrayElements(shadows_str);
      for (const auto& shadow_json : shadow_elements) {
        ShadowData shadow;
        shadow.offset_x = ExtractFloat(shadow_json, "offset_x", 0.0f);
        shadow.offset_y = ExtractFloat(shadow_json, "offset_y", 0.0f);
        shadow.blur = ExtractFloat(shadow_json, "blur", 0.0f);
        shadow.color = Color::FromHex(ExtractString(shadow_json, "color"));
        shadows.push_back(shadow);
      }
      if (!shadows.empty()) {
        output.style.shadow = shadows;
      }
    }
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

          if constexpr (std::is_same_v<T, SaveOp>) {
            oss << "  { \"type\": \"SaveOp\" }";
          } else if constexpr (std::is_same_v<T, RestoreOp>) {
            oss << "  { \"type\": \"RestoreOp\" }";
          } else if constexpr (std::is_same_v<T, ClipRectOp>) {
            oss << "  { \"type\": \"ClipRectOp\", \"rect\": { \"x\": "
                << arg.rect.x << ", \"y\": " << arg.rect.y
                << ", \"width\": " << arg.rect.width
                << ", \"height\": " << arg.rect.height << " } }";
          } else if constexpr (std::is_same_v<T, TranslateOp>) {
            oss << "  { \"type\": \"TranslateOp\", \"dx\": " << arg.dx
                << ", \"dy\": " << arg.dy << " }";
          } else if constexpr (std::is_same_v<T, ScaleOp>) {
            oss << "  { \"type\": \"ScaleOp\", \"sx\": " << arg.sx
                << ", \"sy\": " << arg.sy << " }";
          } else if constexpr (std::is_same_v<T, ConcatOp>) {
            oss << "  { \"type\": \"ConcatOp\", \"matrix\": ["
                << arg.matrix[0] << ", " << arg.matrix[1] << ", "
                << arg.matrix[2] << ", " << arg.matrix[3] << ", "
                << arg.matrix[4] << ", " << arg.matrix[5] << "] }";
          } else if constexpr (std::is_same_v<T, SetMatrixOp>) {
            oss << "  { \"type\": \"SetMatrixOp\", \"matrix\": ["
                << arg.matrix[0] << ", " << arg.matrix[1] << ", "
                << arg.matrix[2] << ", " << arg.matrix[3] << ", "
                << arg.matrix[4] << ", " << arg.matrix[5] << ", "
                << arg.matrix[6] << ", " << arg.matrix[7] << ", "
                << arg.matrix[8] << "] }";
          } else if constexpr (std::is_same_v<T, DrawLineOp>) {
            oss << "  {\n"
                << "    \"type\": \"DrawLineOp\",\n"
                << "    \"rect\": { \"x\": " << arg.rect.x << ", \"y\": " << arg.rect.y
                << ", \"width\": " << arg.rect.width << ", \"height\": " << arg.rect.height << " },\n"
                << "    \"color\": \"" << arg.color.ToHex() << "\",\n"
                << "    \"snapped\": " << (arg.snapped ? "true" : "false") << ",\n"
                << "    \"transform_id\": " << arg.transform_id << ",\n"
                << "    \"clip_id\": " << arg.clip_id << ",\n"
                << "    \"effect_id\": " << arg.effect_id << "\n"
                << "  }";
          } else if constexpr (std::is_same_v<T, DrawStrokeLineOp>) {
            oss << "  {\n"
                << "    \"type\": \"DrawStrokeLineOp\",\n"
                << "    \"p1\": { \"x\": " << arg.p1.x << ", \"y\": " << arg.p1.y << " },\n"
                << "    \"p2\": { \"x\": " << arg.p2.x << ", \"y\": " << arg.p2.y << " },\n"
                << "    \"thickness\": " << arg.thickness << ",\n"
                << "    \"style\": " << static_cast<int>(arg.style) << ",\n"
                << "    \"color\": \"" << arg.color.ToHex() << "\",\n"
                << "    \"antialias\": " << (arg.antialias ? "true" : "false") << ",\n"
                << "    \"transform_id\": " << arg.transform_id << ",\n"
                << "    \"clip_id\": " << arg.clip_id << ",\n"
                << "    \"effect_id\": " << arg.effect_id << "\n"
                << "  }";
          } else if constexpr (std::is_same_v<T, DrawWavyLineOp>) {
            oss << "  {\n"
                << "    \"type\": \"DrawWavyLineOp\",\n"
                << "    \"paintRect\": { \"x\": " << arg.paint_rect.x << ", \"y\": " << arg.paint_rect.y
                << ", \"width\": " << arg.paint_rect.width << ", \"height\": " << arg.paint_rect.height << " },\n"
                << "    \"tileRect\": { \"x\": " << arg.tile_rect.x << ", \"y\": " << arg.tile_rect.y
                << ", \"width\": " << arg.tile_rect.width << ", \"height\": " << arg.tile_rect.height << " },\n"
                << "    \"wave\": { \"wavelength\": " << arg.wave.wavelength
                << ", \"controlPointDistance\": " << arg.wave.control_point_distance
                << ", \"phase\": " << arg.wave.phase << " },\n"
                << "    \"strokeThickness\": " << arg.stroke_thickness << ",\n"
                << "    \"color\": \"" << arg.color.ToHex() << "\",\n"
                << "    \"path\": [";
            for (size_t k = 0; k < arg.tile_path.commands.size(); ++k) {
              const auto& cmd = arg.tile_path.commands[k];
              if (k > 0) oss << ", ";
              oss << "{ \"type\": " << static_cast<int>(cmd.type) << ", \"points\": [";
              for (size_t j = 0; j < cmd.points.size(); ++j) {
                if (j > 0) oss << ", ";
                oss << "{ \"x\": " << cmd.points[j].x << ", \"y\": " << cmd.points[j].y << " }";
              }
              oss << "] }";
            }
            oss << "],\n"
                << "    \"transform_id\": " << arg.transform_id << ",\n"
                << "    \"clip_id\": " << arg.clip_id << ",\n"
                << "    \"effect_id\": " << arg.effect_id << "\n"
                << "  }";
          } else if constexpr (std::is_same_v<T, DrawDecorationLineOp>) {
            oss << "  {\n"
                << "    \"type\": \"DrawDecorationLineOp\",\n"
                << "    \"x\": " << arg.x << ",\n"
                << "    \"y\": " << arg.y << ",\n"
                << "    \"width\": " << arg.width << ",\n"
                << "    \"thickness\": " << arg.thickness << ",\n"
                << "    \"lineType\": " << static_cast<int>(arg.line_type) << ",\n"
                << "    \"style\": " << static_cast<int>(arg.style) << ",\n"
                << "    \"color\": \"" << arg.color.ToHex() << "\",\n"
                << "    \"transform_id\": " << arg.transform_id << ",\n"
                << "    \"clip_id\": " << arg.clip_id << ",\n"
                << "    \"effect_id\": " << arg.effect_id << "\n"
                << "  }";
          } else if constexpr (std::is_same_v<T, DrawEmphasisMarksOp>) {
            oss << "  {\n"
                << "    \"type\": \"DrawEmphasisMarksOp\",\n"
                << "    \"x\": " << arg.x << ",\n"
                << "    \"y\": " << arg.y << ",\n"
                << "    \"mark\": \"" << arg.mark << "\",\n"
                << "    \"positions\": [";
            for (size_t k = 0; k < arg.positions.size(); ++k) {
              if (k > 0) oss << ", ";
              oss << arg.positions[k];
            }
            oss << "],\n"
                << "    \"color\": \"" << arg.color.ToHex() << "\",\n"
                << "    \"fontSize\": " << arg.font_size << ",\n"
                << "    \"transform_id\": " << arg.transform_id << ",\n"
                << "    \"clip_id\": " << arg.clip_id << ",\n"
                << "    \"effect_id\": " << arg.effect_id << "\n"
                << "  }";
          } else if constexpr (std::is_same_v<T, FillEllipseOp>) {
            oss << "  {\n"
                << "    \"type\": \"FillEllipseOp\",\n"
                << "    \"rect\": { \"x\": " << arg.rect.x << ", \"y\": " << arg.rect.y
                << ", \"width\": " << arg.rect.width << ", \"height\": " << arg.rect.height << " },\n"
                << "    \"color\": \"" << arg.color.ToHex() << "\",\n"
                << "    \"transform_id\": " << arg.transform_id << ",\n"
                << "    \"clip_id\": " << arg.clip_id << ",\n"
                << "    \"effect_id\": " << arg.effect_id << "\n"
                << "  }";
          } else if constexpr (std::is_same_v<T, StrokeEllipseOp>) {
            oss << "  {\n"
                << "    \"type\": \"StrokeEllipseOp\",\n"
                << "    \"rect\": { \"x\": " << arg.rect.x << ", \"y\": " << arg.rect.y
                << ", \"width\": " << arg.rect.width << ", \"height\": " << arg.rect.height << " },\n"
                << "    \"color\": \"" << arg.color.ToHex() << "\",\n"
                << "    \"strokeWidth\": " << arg.stroke_width << ",\n"
                << "    \"transform_id\": " << arg.transform_id << ",\n"
                << "    \"clip_id\": " << arg.clip_id << ",\n"
                << "    \"effect_id\": " << arg.effect_id << "\n"
                << "  }";
          } else if constexpr (std::is_same_v<T, FillRectOp>) {
            oss << "  {\n"
                << "    \"type\": \"FillRectOp\",\n"
                << "    \"rect\": { \"x\": " << arg.rect.x << ", \"y\": " << arg.rect.y
                << ", \"width\": " << arg.rect.width << ", \"height\": " << arg.rect.height << " },\n"
                << "    \"color\": \"" << arg.color.ToHex() << "\",\n"
                << "    \"transform_id\": " << arg.transform_id << ",\n"
                << "    \"clip_id\": " << arg.clip_id << ",\n"
                << "    \"effect_id\": " << arg.effect_id << "\n"
                << "  }";
          } else if constexpr (std::is_same_v<T, FillPathOp>) {
            oss << "  {\n"
                << "    \"type\": \"FillPathOp\",\n"
                << "    \"points\": [";
            for (size_t k = 0; k < arg.points.size(); ++k) {
              if (k > 0) oss << ", ";
              oss << "{ \"x\": " << arg.points[k].x << ", \"y\": " << arg.points[k].y << " }";
            }
            oss << "],\n"
                << "    \"color\": \"" << arg.color.ToHex() << "\",\n"
                << "    \"transform_id\": " << arg.transform_id << ",\n"
                << "    \"clip_id\": " << arg.clip_id << ",\n"
                << "    \"effect_id\": " << arg.effect_id << "\n"
                << "  }";
          } else if constexpr (std::is_same_v<T, SaveLayerAlphaOp>) {
            oss << "  {\n"
                << "    \"type\": \"SaveLayerAlphaOp\",\n"
                << "    \"bounds\": { \"x\": " << arg.bounds.x << ", \"y\": " << arg.bounds.y
                << ", \"width\": " << arg.bounds.width << ", \"height\": " << arg.bounds.height << " },\n"
                << "    \"alpha\": " << arg.alpha << "\n"
                << "  }";
          } else if constexpr (std::is_same_v<T, DrawShadowOp>) {
            oss << "  {\n"
                << "    \"type\": \"DrawShadowOp\",\n"
                << "    \"offsetX\": " << arg.offset_x << ",\n"
                << "    \"offsetY\": " << arg.offset_y << ",\n"
                << "    \"blurSigma\": " << arg.blur_sigma << ",\n"
                << "    \"color\": \"" << arg.color.ToHex() << "\"\n"
                << "  }";
          } else if constexpr (std::is_same_v<T, ClearShadowOp>) {
            oss << "  {\n"
                << "    \"type\": \"ClearShadowOp\"\n"
                << "  }";
          } else if constexpr (std::is_same_v<T, DrawTextBlobOp>) {
            oss << "  {\n"
                << "    \"type\": \"DrawTextBlobOp\",\n"
                << "    \"x\": " << arg.x << ",\n"
                << "    \"y\": " << arg.y << ",\n"
                << "    \"nodeId\": " << arg.node_id << ",\n"
                << "    \"flags\": {\n"
                << "      \"r\": " << arg.flags.R() << ",\n"
                << "      \"g\": " << arg.flags.G() << ",\n"
                << "      \"b\": " << arg.flags.B() << ",\n"
                << "      \"a\": " << arg.flags.A() << ",\n"
                << "      \"style\": " << static_cast<int>(arg.flags.style) << ",\n"
                << "      \"strokeWidth\": " << arg.flags.stroke_width << "\n"
                << "    },\n"
                << "    \"bounds\": [" << arg.bounds[0] << ", " << arg.bounds[1]
                << ", " << arg.bounds[2] << ", " << arg.bounds[3] << "],\n"
                << "    \"runs\": [\n";

            for (size_t j = 0; j < arg.runs.size(); ++j) {
              if (j > 0) oss << ",\n";
              const auto& run = arg.runs[j];
              oss << "      {\n"
                  << "        \"glyphCount\": " << run.glyph_count << ",\n"
                  << "        \"glyphs\": [";
              for (size_t k = 0; k < run.glyphs.size(); ++k) {
                if (k > 0) oss << ", ";
                oss << run.glyphs[k];
              }
              oss << "],\n"
                  << "        \"positioning\": " << run.positioning << ",\n"
                  << "        \"offsetX\": " << run.offset_x << ",\n"
                  << "        \"offsetY\": " << run.offset_y << ",\n"
                  << "        \"positions\": [";
              for (size_t k = 0; k < run.positions.size(); ++k) {
                if (k > 0) oss << ", ";
                oss << run.positions[k];
              }
              oss << "],\n"
                  << "        \"font\": {\n"
                  << "          \"size\": " << run.font.size << ",\n"
                  << "          \"scaleX\": " << run.font.scale_x << ",\n"
                  << "          \"skewX\": " << run.font.skew_x << ",\n"
                  << "          \"embolden\": " << (run.font.embolden ? "true" : "false") << ",\n"
                  << "          \"linearMetrics\": " << (run.font.linear_metrics ? "true" : "false") << ",\n"
                  << "          \"subpixel\": " << (run.font.subpixel ? "true" : "false") << ",\n"
                  << "          \"forceAutoHinting\": " << (run.font.force_auto_hinting ? "true" : "false") << ",\n"
                  << "          \"family\": \"" << run.font.family << "\",\n"
                  << "          \"typefaceId\": " << run.font.typeface_id << ",\n"
                  << "          \"weight\": " << run.font.weight << ",\n"
                  << "          \"width\": " << run.font.width << ",\n"
                  << "          \"slant\": " << run.font.slant << "\n"
                  << "        }\n"
                  << "      }";
            }

            oss << "\n    ],\n"
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
