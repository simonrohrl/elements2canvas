#ifndef BLOCK_PAINTER_BLOCK_PAINTER_H_
#define BLOCK_PAINTER_BLOCK_PAINTER_H_

#include "types.h"
#include "draw_commands.h"

#include <optional>
#include <vector>

// Input context for block painting
struct BlockPaintInput {
  // Geometry (x, y, width, height)
  RectF geometry;

  // Border radii [tl_x, tl_y, tr_x, tr_y, br_x, br_y, bl_x, bl_y]
  std::optional<BorderRadii> border_radii;

  // Background color
  std::optional<Color> background_color;

  // Box shadows
  std::vector<BoxShadowData> box_shadow;

  // Visibility
  Visibility visibility = Visibility::kVisible;

  // DOM node ID
  DOMNodeId node_id = kInvalidDOMNodeId;

  // Property tree state IDs
  GraphicsStateIds state_ids;
};

// Pure functional block painter
// Takes input context and produces paint operations
class BlockPainter {
 public:
  // Main entry point - produces paint ops from input
  static PaintOpList Paint(const BlockPaintInput& input);

 private:
  // Check if input has non-zero border radius
  static bool HasBorderRadius(const BlockPaintInput& input);

  // Build DrawFlags from input
  static DrawFlags BuildFlags(const BlockPaintInput& input);
};

#endif  // BLOCK_PAINTER_BLOCK_PAINTER_H_
