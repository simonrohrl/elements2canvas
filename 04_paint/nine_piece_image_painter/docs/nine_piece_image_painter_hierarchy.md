# NinePieceImagePainter Class Hierarchy

## Overview

The `NinePieceImagePainter` is responsible for painting CSS border-image and mask-image properties using the 9-slice scaling algorithm. It takes a single image and slices it into 9 regions, painting each according to CSS rules.

## Class Structure

```
NinePieceImagePainter (static class - no instantiation)
    |
    +-- Paint() [static method - main entry point]
           |
           +-- NinePieceImageGrid (helper class)
           |       |
           |       +-- NinePieceDrawInfo (drawing info for each piece)
           |       +-- Edge (slice/width data for each side)
           |
           +-- PaintPieces() [anonymous namespace helper]
                   |
                   +-- GraphicsContext::DrawImage()
                   +-- GraphicsContext::DrawImageTiled()
```

## Source Files

| File | Location | Purpose |
|------|----------|---------|
| `nine_piece_image_painter.h` | `core/paint/` | Class declaration |
| `nine_piece_image_painter.cc` | `core/paint/` | Implementation |
| `nine_piece_image_grid.h` | `core/paint/` | 9-slice grid computation |
| `nine_piece_image_grid.cc` | `core/paint/` | Grid algorithm implementation |
| `nine_piece_image.h` | `core/style/` | CSS property data structure |

## The 9-Slice Model

The CSS border-image specification defines a 9-slice model:

```
       |         |
   +---+---------+---+          +------------------+
   | 1 |    7    | 4 |          |      border      |
---+---+---------+---+---       |  +------------+  |
   |   |         |   |          |  |            |  |
   | 3 |    9    | 6 |          |  |    css     |  |
   |   |  image  |   |          |  |    box     |  |
   |   |         |   |          |  |            |  |
---+---+---------+---+---       |  |            |  |
   | 2 |    8    | 5 |          |  +------------+  |
   +---+---------+---+          |                  |
       |         |              +------------------+

Piece Numbers (NinePiece enum):
  1 = kTopLeftPiece     (corner)
  2 = kBottomLeftPiece  (corner)
  3 = kLeftPiece        (edge)
  4 = kTopRightPiece    (corner)
  5 = kBottomRightPiece (corner)
  6 = kRightPiece       (edge)
  7 = kTopPiece         (edge)
  8 = kBottomPiece      (edge)
  9 = kMiddlePiece      (center)
```

## NinePiece Enum

```cpp
enum NinePiece {
  kMinPiece = 0,
  kTopLeftPiece = kMinPiece,  // 0 - top-left corner
  kBottomLeftPiece,           // 1 - bottom-left corner
  kLeftPiece,                 // 2 - left edge
  kTopRightPiece,             // 3 - top-right corner
  kBottomRightPiece,          // 4 - bottom-right corner
  kRightPiece,                // 5 - right edge
  kTopPiece,                  // 6 - top edge
  kBottomPiece,               // 7 - bottom edge
  kMiddlePiece,               // 8 - center
  kMaxPiece                   // 9 - sentinel value
};
```

## ENinePieceImageRule Enum

Controls how edge and center pieces are tiled:

```cpp
enum ENinePieceImageRule {
  kStretchImageRule,  // Scale image to fit destination
  kRoundImageRule,    // Tile with whole tiles, scaling to fit
  kSpaceImageRule,    // Tile with spacing between tiles
  kRepeatImageRule    // Tile without scaling, centered
};
```

## Key Data Structures

### NinePieceImage (Style Data)

Stores the CSS property values:
- `image` - The source image
- `image_slices` - LengthBox defining slice lines (border-image-slice)
- `fill` - Whether to paint the center (border-image-slice: fill)
- `border_slices` - BorderImageLengthBox defining output widths (border-image-width)
- `outset` - BorderImageLengthBox for outset (border-image-outset)
- `horizontal_rule` - Tiling rule for horizontal edges
- `vertical_rule` - Tiling rule for vertical edges

### NinePieceImageGrid::Edge

Stores computed values for each edge:

```cpp
struct Edge {
  float slice;  // Slice position in image pixels
  int width;    // Destination width in device pixels

  bool IsDrawable() const { return slice > 0 && width > 0; }
  float Scale() const { return IsDrawable() ? width / slice : 1; }
};
```

### NinePieceImageGrid::NinePieceDrawInfo

Drawing information for a single piece:

```cpp
struct NinePieceDrawInfo {
  bool is_drawable;       // Whether this piece should be drawn
  bool is_corner_piece;   // True for corners, false for edges/center
  gfx::RectF destination; // Where to draw in device space
  gfx::RectF source;      // Source rectangle in image space

  // For edges and center only:
  gfx::Vector2dF tile_scale;  // Scale factor for tiling
  struct {
    ENinePieceImageRule horizontal;
    ENinePieceImageRule vertical;
  } tile_rule;
};
```

## Relationship to CSS Properties

| CSS Property | Data Member | Purpose |
|--------------|-------------|---------|
| `border-image-source` | `NinePieceImage::image` | Source image URL |
| `border-image-slice` | `NinePieceImage::image_slices` | Where to slice the image |
| `border-image-width` | `NinePieceImage::border_slices` | Width of border areas |
| `border-image-outset` | `NinePieceImage::outset` | How far to extend beyond border box |
| `border-image-repeat` | `horizontal_rule`, `vertical_rule` | How to tile edges/center |

## Callers

The `NinePieceImagePainter::Paint()` method is called from:

1. **BoxPainterBase::PaintBorder()** - For `border-image` CSS property
2. **BoxPainterBase::PaintMaskImages()** - For `-webkit-mask-box-image` CSS property

## Dependencies

### Style Layer
- `ComputedStyle` - Provides image orientation, zoom, interpolation quality
- `NinePieceImage` - CSS property values
- `StyleImage` - Image resource wrapper

### Paint Layer
- `NinePieceImageGrid` - Computes the 9 pieces
- `GraphicsContext` - Performs actual drawing

### Platform Layer
- `Image` - Platform image abstraction
- `gfx::RectF`, `gfx::SizeF` - Geometry primitives
