#ifndef TEXT_PAINTER_JSON_PARSER_H_
#define TEXT_PAINTER_JSON_PARSER_H_

#include "types.h"
#include "text_painter.h"
#include "draw_commands.h"
#include <string>

// Simple JSON parsing and serialization for text painter
// Uses a minimal approach without external dependencies

class JsonParser {
 public:
  // Parse input JSON into TextPaintInput
  static bool ParseInput(const std::string& json, TextPaintInput& output);

  // Serialize PaintOpList to JSON
  static std::string SerializeOps(const PaintOpList& ops);

 private:
  // Helper to extract string value from JSON
  static std::string ExtractString(const std::string& json,
                                   const std::string& key);

  // Helper to extract number value from JSON
  static float ExtractFloat(const std::string& json, const std::string& key,
                            float default_value = 0.0f);

  // Helper to extract int value from JSON
  static int ExtractInt(const std::string& json, const std::string& key,
                        int default_value = 0);

  // Helper to extract bool value from JSON
  static bool ExtractBool(const std::string& json, const std::string& key,
                          bool default_value = false);

  // Helper to extract object from JSON
  static std::string ExtractObject(const std::string& json,
                                   const std::string& key);

  // Helper to extract array from JSON
  static std::string ExtractArray(const std::string& json,
                                  const std::string& key);

  // Helper to check if key has null value
  static bool IsNull(const std::string& json, const std::string& key);

  // Parse a glyph run from JSON
  static GlyphRun ParseGlyphRun(const std::string& json);

  // Parse array of integers
  static std::vector<uint16_t> ParseIntArray(const std::string& array_str);

  // Parse array of floats
  static std::vector<float> ParseFloatArray(const std::string& array_str);
};

#endif  // TEXT_PAINTER_JSON_PARSER_H_
