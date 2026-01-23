#ifndef BORDER_PAINTER_JSON_PARSER_H_
#define BORDER_PAINTER_JSON_PARSER_H_

#include <string>

#include "border_painter.h"
#include "draw_commands.h"

namespace border_painter {

// Parse input JSON file into BorderPaintInput
BorderPaintInput ParseInput(const std::string& json_str);

// Serialize paint operations to JSON string
std::string SerializeOps(const PaintOpList& ops);

}  // namespace border_painter

#endif  // BORDER_PAINTER_JSON_PARSER_H_
