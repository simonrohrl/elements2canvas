// WASM entry point for text_painter
//
// This module exposes the text_painter functionality to JavaScript via Emscripten.
// It provides a single function that takes JSON input and returns JSON output.
//
// Usage from JavaScript:
//   const Module = await TextPainterModule();
//   const paintText = Module.cwrap('paint_text', 'string', ['string']);
//   const outputJson = paintText(inputJson);

#include <emscripten.h>
#include "json_parser.h"
#include "text_painter.h"
#include <string>

extern "C" {

// Main entry point for text painting
// Takes JSON input string, returns JSON output string with paint operations
// The returned string is valid until the next call to paint_text
EMSCRIPTEN_KEEPALIVE
const char* paint_text(const char* input_json) {
  // Use static string to persist result across calls
  // This avoids memory management complexity for the caller
  static std::string result;

  // Validate input
  if (!input_json) {
    result = R"({"error": "Input JSON is null"})";
    return result.c_str();
  }

  // Parse input JSON
  TextPaintInput input;
  if (!JsonParser::ParseInput(input_json, input)) {
    result = R"({"error": "Failed to parse input JSON"})";
    return result.c_str();
  }

  // Run text painter to generate paint operations
  PaintOpList ops = TextPainter::Paint(input);

  // Serialize paint operations to JSON
  result = JsonParser::SerializeOps(ops);
  return result.c_str();
}

// Version info for debugging
EMSCRIPTEN_KEEPALIVE
const char* get_version() {
  return "text_painter WASM v1.0";
}

}  // extern "C"
