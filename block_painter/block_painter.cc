#include "block_painter.h"

PaintOpList BlockPainter::Paint(const BlockPaintInput& input) {
  PaintOpList ops;

  // 1. Check visibility - skip if not visible
  if (input.visibility != Visibility::kVisible) {
    return ops;
  }

  // 2. If no background color, nothing to paint
  if (!input.background_color.has_value()) {
    return ops;
  }

  // 3. Build paint flags with color and shadows
  DrawFlags flags = BuildFlags(input);

  // 4. Convert geometry to LTRB rect format
  std::array<float, 4> rect = input.geometry.ToLTRB();

  // 5. Output DrawRectOp or DrawRRectOp based on border radius
  if (HasBorderRadius(input)) {
    ops.DrawRRect(rect, *input.border_radii, flags,
                  input.state_ids.transform_id,
                  input.state_ids.clip_id,
                  input.state_ids.effect_id);
  } else {
    ops.DrawRect(rect, flags,
                 input.state_ids.transform_id,
                 input.state_ids.clip_id,
                 input.state_ids.effect_id);
  }

  return ops;
}

bool BlockPainter::HasBorderRadius(const BlockPaintInput& input) {
  if (!input.border_radii.has_value()) {
    return false;
  }
  return !IsZeroRadii(*input.border_radii);
}

DrawFlags BlockPainter::BuildFlags(const BlockPaintInput& input) {
  DrawFlags flags;

  // Set color
  if (input.background_color.has_value()) {
    flags.SetColor(*input.background_color);
  }

  // Set style to Fill
  flags.style = 0;
  flags.stroke_width = 0.0f;

  // Add shadows
  for (const auto& shadow : input.box_shadow) {
    // Skip inset shadows for now (they're painted differently)
    if (shadow.inset) {
      continue;
    }

    ShadowFlag sf;
    sf.offset_x = shadow.offset_x;
    sf.offset_y = shadow.offset_y;
    sf.blur_sigma = shadow.BlurAsSigma();
    sf.color = shadow.color;
    sf.flags = 2;  // Standard shadow flags
    flags.shadows.push_back(sf);
  }

  return flags;
}
