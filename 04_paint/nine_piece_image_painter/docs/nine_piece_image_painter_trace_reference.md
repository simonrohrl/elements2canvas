# NinePieceImagePainter Trace Reference

## File: nine_piece_image_painter.cc

### Line-by-Line Trace of Paint() Method

```cpp
// Lines 181-240
bool NinePieceImagePainter::Paint(
    GraphicsContext& graphics_context,
    const ImageResourceObserver& observer,
    const Document& document,
    Node* node,
    const PhysicalRect& rect,
    const ComputedStyle& style,
    const NinePieceImage& nine_piece_image,
    PhysicalBoxSides sides_to_include)
```

**Parameters:**
- `graphics_context` - Target for draw operations
- `observer` - For image loading callbacks
- `document` - Document context
- `node` - DOM node being painted (can be null)
- `rect` - Border box rectangle
- `style` - Computed style for rendering hints
- `nine_piece_image` - The border-image CSS property data
- `sides_to_include` - Which sides to paint (for border-image-outset clipping)

**Return Value:**
- `true` - Image was handled (painted or intentionally skipped)
- `false` - Fallback to regular border painting

---

### Step 1: Get and Validate StyleImage (Lines 189-198)

```cpp
StyleImage* style_image = nine_piece_image.GetImage();
if (!style_image)
  return false;  // No image - paint fallback borders

if (!style_image->IsLoaded())
  return true;   // Loading - don't paint anything, don't fallback

if (!style_image->CanRender())
  return false;  // Can't render (e.g., invalid image) - paint fallback
```

**Logic:**
- A null image means no `border-image-source` was specified
- An unloaded image should not trigger fallback borders during loading
- An unrenderable image should fall back to regular borders

---

### Step 2: Compute Border Image Rectangle (Lines 203-205)

```cpp
PhysicalRect rect_with_outsets = rect;
rect_with_outsets.Expand(style.ImageOutsets(nine_piece_image));
PhysicalRect border_image_rect = rect_with_outsets;
```

**Logic:**
- `ImageOutsets()` computes the `border-image-outset` values
- The border image can extend beyond the border box

---

### Step 3: Resolve Image Size (Lines 211-224)

```cpp
const RespectImageOrientationEnum respect_orientation =
    style.ImageOrientation();
const gfx::SizeF default_object_size(border_image_rect.size);

// Get zoomed image size (for raster: native pixels; for SVG: scaled)
gfx::SizeF image_size = style_image->ImageSize(
    style.EffectiveZoom(), default_object_size, respect_orientation);

const Node* ref_node = node;
if (!node) {
  ref_node = &document;
}

// Get actual Image object at resolved size
scoped_refptr<Image> image =
    style_image->GetImage(observer, *ref_node, style, image_size);
if (!image) {
  return true;  // Image resolved to null - don't fallback
}
```

**Logic:**
- For raster images: size is native resolution (includes device pixel ratio)
- For SVG/generated: size is scaled by effective zoom
- The `ref_node` is needed for context-dependent images

---

### Step 4: Compute Unzoomed Image Size (Lines 229-231)

```cpp
gfx::SizeF unzoomed_image_size = style_image->ImageSize(
    1, gfx::ScaleSize(default_object_size, 1 / style.EffectiveZoom()),
    respect_orientation);
```

**Logic:**
- This gives the size in CSS pixels (not device pixels)
- Used for interpreting `border-image-slice` values

---

### Step 5: Emit DevTools Trace Event (Lines 233-236)

```cpp
DEVTOOLS_TIMELINE_TRACE_EVENT_WITH_CATEGORIES(
    TRACE_DISABLED_BY_DEFAULT("devtools.timeline"), "PaintImage",
    inspector_paint_image_event::Data, node, *style_image,
    gfx::RectF(image->Rect()), gfx::RectF(border_image_rect));
```

**Trace Event:** `PaintImage`
- Category: `devtools.timeline`
- Data: node, style_image, source rect, destination rect

---

### Step 6: Paint All Pieces (Lines 237-238)

```cpp
PaintPieces(graphics_context, border_image_rect, style, nine_piece_image,
            *image, unzoomed_image_size, sides_to_include);
return true;
```

---

## PaintPieces() Function Trace (Lines 87-177)

### Step 1: Compute Scale Factor (Lines 94-106)

```cpp
const RespectImageOrientationEnum respect_orientation =
    style.ImageOrientation();
const gfx::SizeF image_size = image.SizeAsFloat(respect_orientation);

// Compute DPR/zoom scale: image_pixels / css_pixels
gfx::Vector2dF slice_scale(
    image_size.width() / unzoomed_image_size.width(),
    image_size.height() / unzoomed_image_size.height());
```

**Purpose:** Maps CSS pixel slice values to image pixel coordinates

---

### Step 2: Create NinePieceImageGrid (Lines 108-114)

```cpp
auto border_widths =
    gfx::Outsets()
        .set_left_right(style.BorderLeftWidth(), style.BorderRightWidth())
        .set_top_bottom(style.BorderTopWidth(), style.BorderBottomWidth());

NinePieceImageGrid grid(
    nine_piece_image, image_size, slice_scale, style.EffectiveZoom(),
    ToPixelSnappedRect(border_image_rect), border_widths, sides_to_include);
```

**NinePieceImageGrid Constructor Actions:**
1. Extract slice values from `border-image-slice`
2. Convert slices to image pixels using slice_scale
3. Extract width values from `border-image-width`
4. Apply auto-width resolution using slice values
5. Scale down if widths overflow the box
6. Snap to device pixels

---

### Step 3: Configure Image Rendering (Lines 117-120)

```cpp
auto image_auto_dark_mode = ImageAutoDarkMode::Disabled();

ScopedImageRenderingSettings image_rendering_settings_scope(
    context, style.GetInterpolationQuality(), style.GetDynamicRangeLimit());
```

**Settings:**
- Dark mode is disabled for border images
- Interpolation quality from CSS `image-rendering` property
- Dynamic range limit for HDR content

---

### Step 4: Iterate Through All 9 Pieces (Lines 121-176)

```cpp
for (NinePiece piece = kMinPiece; piece < kMaxPiece; ++piece) {
```

**Iteration Order:**
1. `kTopLeftPiece` (0) - top-left corner
2. `kBottomLeftPiece` (1) - bottom-left corner
3. `kLeftPiece` (2) - left edge
4. `kTopRightPiece` (3) - top-right corner
5. `kBottomRightPiece` (4) - bottom-right corner
6. `kRightPiece` (5) - right edge
7. `kTopPiece` (6) - top edge
8. `kBottomPiece` (7) - bottom edge
9. `kMiddlePiece` (8) - center

---

### Step 4a: Get Draw Info (Lines 122-125)

```cpp
NinePieceImageGrid::NinePieceDrawInfo draw_info =
    grid.GetNinePieceDrawInfo(piece);
if (!draw_info.is_drawable)
  continue;
```

**NinePieceDrawInfo Fields:**
- `is_drawable` - false if slice=0, width=0, or (for center) fill=false
- `is_corner_piece` - true for corners
- `source` - Rectangle in image space
- `destination` - Rectangle in screen space
- `tile_scale` - Scale factor for tiling
- `tile_rule` - Horizontal and vertical tiling rules

---

### Step 4b: Draw Non-Tiled Pieces (Lines 127-145)

```cpp
if (!ShouldTile(draw_info)) {
  gfx::RectF src_rect = draw_info.source;
  if (respect_orientation && !image.HasDefaultOrientation()) {
    src_rect = image.CorrectSrcRectForImageOrientation(image_size, src_rect);
  }
  context.DrawImage(image, Image::kSyncDecode, image_auto_dark_mode,
                    ImagePaintTimingInfo(), draw_info.destination,
                    &src_rect, SkBlendMode::kSrcOver, respect_orientation);
  continue;
}
```

**ShouldTile() Returns False When:**
- Piece is a corner (always stretched)
- Both horizontal and vertical rules are `kStretchImageRule`

**DrawImage Parameters:**
- `kSyncDecode` - Decode image synchronously
- `ImagePaintTimingInfo()` - Default paint timing (for LCP)
- `SkBlendMode::kSrcOver` - Standard alpha compositing

---

### Step 4c: Compute Tile Parameters (Lines 148-155)

```cpp
const std::optional<TileParameters> h_tile = ComputeTileParameters(
    draw_info.tile_rule.horizontal, draw_info.destination.width(),
    draw_info.source.width() * draw_info.tile_scale.x());
const std::optional<TileParameters> v_tile = ComputeTileParameters(
    draw_info.tile_rule.vertical, draw_info.destination.height(),
    draw_info.source.height() * draw_info.tile_scale.y());
if (!h_tile || !v_tile)
  continue;  // Cannot tile (e.g., space rule with no room for tiles)
```

---

### Step 4d: Build ImageTilingInfo (Lines 157-170)

```cpp
ImageTilingInfo tiling_info;
tiling_info.image_rect = draw_info.source;
tiling_info.scale = gfx::ScaleVector2d(
    draw_info.tile_scale, h_tile->scale_factor, v_tile->scale_factor);

// Phase calculation: origin of full image in destination space
gfx::PointF tile_origin_in_dest_space = draw_info.source.origin();
tile_origin_in_dest_space.Scale(tiling_info.scale.x(), tiling_info.scale.y());
tiling_info.phase =
    draw_info.destination.origin() +
    (gfx::PointF(h_tile->phase, v_tile->phase) - tile_origin_in_dest_space);
tiling_info.spacing = gfx::SizeF(h_tile->spacing, v_tile->spacing);
```

**ImageTilingInfo Fields:**
- `image_rect` - Which part of the image to tile
- `scale` - How much to scale each tile
- `phase` - Where the first tile starts
- `spacing` - Gap between tiles (for `space` rule)

---

### Step 4e: Draw Tiled Image (Lines 173-175)

```cpp
context.DrawImageTiled(image, draw_info.destination, tiling_info,
                       image_auto_dark_mode, ImagePaintTimingInfo(),
                       SkBlendMode::kSrcOver, respect_orientation);
```

---

## Tile Rule Algorithms (Lines 23-73)

### CalculateSpaceNeeded() (Lines 23-36)

```cpp
std::optional<float> CalculateSpaceNeeded(const float destination,
                                          const float source) {
  float repeat_tiles_count = floorf(destination / source);
  if (!repeat_tiles_count)
    return std::nullopt;  // Can't fit even one tile

  float space = destination;
  space -= source * repeat_tiles_count;
  space /= repeat_tiles_count + 1.0;  // Distribute evenly
  return space;
}
```

**Space Rule Algorithm:**
1. Calculate how many whole tiles fit
2. Calculate leftover space
3. Divide by (tiles + 1) for n+1 gaps around n tiles

---

### ComputeTileParameters() (Lines 45-73)

```cpp
std::optional<TileParameters> ComputeTileParameters(
    ENinePieceImageRule tile_rule,
    float dst_extent,
    float src_extent)
```

**kRoundImageRule:**
```cpp
const float repetitions = std::max(1.0f, roundf(dst_extent / src_extent));
const float scale_factor = dst_extent / (src_extent * repetitions);
return TileParameters{scale_factor, 0, 0};
```
- Round to nearest whole number of tiles
- Scale tiles to fill exactly

**kRepeatImageRule:**
```cpp
const float phase = (dst_extent - src_extent) / 2;
return TileParameters{1, phase, 0};
```
- No scaling
- Center the pattern

**kSpaceImageRule:**
```cpp
const std::optional<float> spacing = CalculateSpaceNeeded(dst_extent, src_extent);
if (!spacing)
  return std::nullopt;
return TileParameters{1, *spacing, *spacing};
```
- No scaling
- Distribute space evenly between tiles

**kStretchImageRule:**
```cpp
return TileParameters{1, 0, 0};
```
- No scaling here (handled by DrawImage)

---

## NinePieceImageGrid Key Methods

### SetDrawInfoCorner() (nine_piece_image_grid.cc:205-238)

Maps corner slices to corner destinations:

| Piece | Source | Destination |
|-------|--------|-------------|
| TopLeft | `(0, 0, left_.slice, top_.slice)` | `(0, 0, left_.width, top_.width)` |
| TopRight | `(-right_.slice, 0, right_.slice, top_.slice)` | `(-right_.width, 0, right_.width, top_.width)` |
| BottomLeft | `(0, -bottom_.slice, left_.slice, bottom_.slice)` | `(0, -bottom_.width, left_.width, bottom_.width)` |
| BottomRight | `(-right_.slice, -bottom_.slice, right_.slice, bottom_.slice)` | `(-right_.width, -bottom_.width, right_.width, bottom_.width)` |

---

### SetDrawInfoEdge() (nine_piece_image_grid.cc:272-317)

Maps edge slices to edge destinations:

| Piece | Source (excluding corners) | Destination |
|-------|---------------------------|-------------|
| Left | `(0, top_.slice, left_.slice, edge_height)` | `(0, top_.width, left_.width, edge_height)` |
| Right | `(-right_.slice, top_.slice, right_.slice, edge_height)` | `(-right_.width, top_.width, right_.width, edge_height)` |
| Top | `(left_.slice, 0, edge_width, top_.slice)` | `(left_.width, 0, edge_width, top_.width)` |
| Bottom | `(left_.slice, -bottom_.slice, edge_width, bottom_.slice)` | `(left_.width, -bottom_.width, edge_width, bottom_.width)` |

**Tile Rules:**
- Horizontal edges: `{horizontal_rule, kStretchImageRule}`
- Vertical edges: `{kStretchImageRule, vertical_rule}`

---

### SetDrawInfoMiddle() (nine_piece_image_grid.cc:319-366)

```cpp
draw_info.is_drawable = fill_ && !source_size.IsEmpty() && !destination_size.IsEmpty();
```

- Only drawn if `border-image-slice: fill` is specified
- Source: center of image (excluding all slices)
- Destination: center of border box (excluding all widths)
- Tile scale derived from edges, or from zoom if edges not drawable

---

## GraphicsContext Draw Operations Summary

| Method | Used For | Key Parameters |
|--------|----------|----------------|
| `DrawImage()` | Corners, stretch-only edges | source rect, destination rect |
| `DrawImageTiled()` | Edges/center with tiling | ImageTilingInfo (rect, scale, phase, spacing) |

**Both methods use:**
- `Image::kSyncDecode` - No async decoding
- `ImageAutoDarkMode::Disabled()` - No dark mode filtering
- `SkBlendMode::kSrcOver` - Standard alpha blending
- `RespectImageOrientationEnum` - From style
