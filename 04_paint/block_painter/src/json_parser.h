#ifndef BLOCK_PAINTER_JSON_PARSER_H_
#define BLOCK_PAINTER_JSON_PARSER_H_

#include "types.h"
#include "block_painter.h"
#include "draw_commands.h"

#include <string>
#include <vector>

// Simple JSON parsing and serialization for block painter
// Uses a minimal approach without external dependencies

class JsonParser {
 public:
  // Parse input JSON into BlockPaintInput
  static bool ParseInput(const std::string& json, BlockPaintInput& output);

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

  // Helper to check if key exists in JSON
  static bool HasKey(const std::string& json, const std::string& key);

  // Parse array of floats
  static std::vector<float> ParseFloatArray(const std::string& array_str);

  // Parse box shadow from JSON
  static BoxShadowData ParseBoxShadow(const std::string& json);
};

#endif  // BLOCK_PAINTER_JSON_PARSER_H_
