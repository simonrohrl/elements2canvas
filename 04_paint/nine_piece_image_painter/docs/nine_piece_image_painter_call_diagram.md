# NinePieceImagePainter Call Diagram

## Main Entry Point Flow

```
BoxPainterBase::PaintBorder() or BoxPainterBase::PaintMaskImages()
    |
    v
+------------------------------------------------------------------+
|  NinePieceImagePainter::Paint()                                  |
|  [nine_piece_image_painter.cc:181-240]                           |
+------------------------------------------------------------------+
    |
    |  1. Validate image exists and is loaded
    |  2. Compute border_image_rect with outsets
    |  3. Resolve image size (for SVG/generated images)
    |  4. Get actual Image object from StyleImage
    |
    v
+------------------------------------------------------------------+
|  PaintPieces() [anonymous namespace]                             |
|  [nine_piece_image_painter.cc:87-177]                            |
+------------------------------------------------------------------+
    |
    |  1. Compute slice scale factor
    |  2. Create NinePieceImageGrid
    |  3. Loop through all 9 pieces
    |
    v
+------------------------------------------------------------------+
|  NinePieceImageGrid::GetNinePieceDrawInfo()                      |
|  [nine_piece_image_grid.cc:368-383]                              |
+------------------------------------------------------------------+
    |
    +---> SetDrawInfoCorner() [for corners]
    |     [nine_piece_image_grid.cc:205-238]
    |
    +---> SetDrawInfoEdge() [for edges]
    |     [nine_piece_image_grid.cc:272-317]
    |
    +---> SetDrawInfoMiddle() [for center]
          [nine_piece_image_grid.cc:319-366]
```

## Detailed Paint() Method Flow

```
Paint(graphics_context, observer, document, node, rect, style, nine_piece_image, sides_to_include)
    |
    |---> style_image = nine_piece_image.GetImage()
    |     [Line 189] Get StyleImage from NinePieceImage
    |
    |---> if (!style_image) return false
    |     [Line 190-191] No image to paint
    |
    |---> if (!style_image->IsLoaded()) return true
    |     [Line 193-195] Image loading - don't paint fallback borders
    |
    |---> if (!style_image->CanRender()) return false
    |     [Line 197-198] Image cannot render
    |
    |---> rect_with_outsets = rect.Expand(style.ImageOutsets())
    |     [Line 203-204] Apply border-image-outset
    |
    |---> image_size = style_image->ImageSize(zoom, default_size, orientation)
    |     [Line 214-215] Resolve image dimensions
    |
    |---> image = style_image->GetImage(observer, ref_node, style, image_size)
    |     [Line 220-221] Get concrete Image object
    |
    |---> unzoomed_image_size = style_image->ImageSize(1, ...)
    |     [Line 229-231] Get CSS pixel size (for slice calculations)
    |
    |---> DEVTOOLS_TIMELINE_TRACE_EVENT(...)
    |     [Line 233-236] Emit DevTools trace event
    |
    +---> PaintPieces(graphics_context, border_image_rect, style,
                      nine_piece_image, *image, unzoomed_image_size, sides_to_include)
          [Line 237-238]
```

## PaintPieces() Flow

```
PaintPieces(context, border_image_rect, style, nine_piece_image, image, unzoomed_image_size, sides_to_include)
    |
    |---> image_size = image.SizeAsFloat(respect_orientation)
    |     [Line 98] Get actual image dimensions
    |
    |---> slice_scale = image_size / unzoomed_image_size
    |     [Line 104-106] Compute DPR/zoom scale factor
    |
    |---> border_widths = gfx::Outsets(left, right, top, bottom)
    |     [Line 108-111] Get CSS border widths
    |
    |---> grid = NinePieceImageGrid(nine_piece_image, image_size, slice_scale,
    |                               zoom, border_image_rect, border_widths, sides_to_include)
    |     [Line 112-114] Create the 9-slice grid
    |
    |---> ScopedImageRenderingSettings(context, quality, range_limit)
    |     [Line 119-120] Apply interpolation quality
    |
    +---> for (NinePiece piece = kMinPiece; piece < kMaxPiece; ++piece)
          [Line 121] Loop through all 9 pieces
              |
              |---> draw_info = grid.GetNinePieceDrawInfo(piece)
              |     [Line 122-123] Get drawing info for this piece
              |
              |---> if (!draw_info.is_drawable) continue
              |     [Line 124-125] Skip non-drawable pieces
              |
              +---> if (!ShouldTile(draw_info))
                    |   [Line 127] Corners or stretch-only pieces
                    |
                    +---> context.DrawImage(image, kSyncDecode, auto_dark_mode,
                          |                 ImagePaintTimingInfo(), destination,
                          |                 &src_rect, kSrcOver, respect_orientation)
                          [Line 141-143] Draw without tiling
                    |
                    else (edges with tiling)
                    |
                    |---> h_tile = ComputeTileParameters(horizontal_rule, ...)
                    |     [Line 148-150]
                    |
                    |---> v_tile = ComputeTileParameters(vertical_rule, ...)
                    |     [Line 151-153]
                    |
                    |---> tiling_info = {image_rect, scale, phase, spacing}
                    |     [Line 157-170] Compute tiling parameters
                    |
                    +---> context.DrawImageTiled(image, destination, tiling_info,
                                                 auto_dark_mode, ImagePaintTimingInfo(),
                                                 kSrcOver, respect_orientation)
                          [Line 173-175] Draw with tiling
```

## NinePieceImageGrid Construction

```
NinePieceImageGrid(nine_piece_image, image_size, slice_scale, zoom, border_image_area, border_widths, sides_to_include)
    |
    |---> Store basic parameters
    |     [Line 86-91] border_image_area_, image_size_, rules, zoom_, fill_
    |
    |---> Compute edge slices from border-image-slice
    |     [Line 92-100]
    |     top_.slice = ComputeEdgeSlice(image_slices.Top(), scale_y, height)
    |     right_.slice = ComputeEdgeSlice(image_slices.Right(), scale_x, width)
    |     bottom_.slice = ComputeEdgeSlice(image_slices.Bottom(), scale_y, height)
    |     left_.slice = ComputeEdgeSlice(image_slices.Left(), scale_x, width)
    |
    |---> Compute edge widths from border-image-width
    |     [Line 107-128]
    |     resolved_widths.top = ComputeEdgeWidth(border_slices.Top(), ...)
    |     resolved_widths.right = ComputeEdgeWidth(border_slices.Right(), ...)
    |     resolved_widths.bottom = ComputeEdgeWidth(border_slices.Bottom(), ...)
    |     resolved_widths.left = ComputeEdgeWidth(border_slices.Left(), ...)
    |
    |---> Apply scale factor if edges overflow box
    |     [Line 130-148] f = min(width/(left+right), height/(top+bottom))
    |
    +---> Snap widths to pixels
          [Line 150-156]
          top_.width = snapped_widths.top()
          right_.width = snapped_widths.right()
          bottom_.width = snapped_widths.bottom()
          left_.width = snapped_widths.left()
```

## Piece Drawing Decision Tree

```
GetNinePieceDrawInfo(piece)
    |
    |---> is_corner_piece = (piece in {TopLeft, TopRight, BottomLeft, BottomRight})
    |
    +---> if (is_corner_piece)
    |         |
    |         +---> SetDrawInfoCorner(draw_info, piece)
    |                   |
    |                   +---> source = Subrect(image_size_, slice positions)
    |                   +---> destination = Subrect(border_image_area_, width positions)
    |                   +---> is_drawable = adjacent edges both drawable
    |
    +---> else if (piece != kMiddlePiece)  [edge pieces]
    |         |
    |         +---> SetDrawInfoEdge(draw_info, piece)
    |                   |
    |                   +---> source = middle portion of edge in image
    |                   +---> destination = middle portion of border area
    |                   +---> tile_scale = edge.Scale()
    |                   +---> tile_rule = {horizontal_rule or stretch, vertical_rule or stretch}
    |
    +---> else [middle piece]
              |
              +---> SetDrawInfoMiddle(draw_info)
                        |
                        +---> is_drawable = fill_ && !empty
                        +---> source = center of image (minus all slices)
                        +---> destination = center of border area (minus all widths)
                        +---> tile_scale = computed from edges or zoom
                        +---> tile_rule = {horizontal_rule, vertical_rule}
```

## GraphicsContext Draw Operations

```
For Corners (no tiling):
    GraphicsContext::DrawImage()
        Parameters:
        - image: The source Image object
        - decode_mode: kSyncDecode
        - auto_dark_mode: Disabled
        - paint_timing_info: Default
        - destination: gfx::RectF (where to draw)
        - src_rect: gfx::RectF* (portion of source image)
        - blend_mode: kSrcOver
        - respect_orientation: From style

For Edges/Center with Tiling:
    GraphicsContext::DrawImageTiled()
        Parameters:
        - image: The source Image object
        - dest_rect: gfx::RectF (clipped drawing area)
        - tiling_info: ImageTilingInfo
            - image_rect: Source rectangle
            - scale: Tile scale factor
            - phase: Tile origin offset
            - spacing: Gap between tiles
        - auto_dark_mode: Disabled
        - paint_timing_info: Default
        - blend_mode: kSrcOver
        - respect_orientation: From style
```

## Tile Rule Computation

```
ComputeTileParameters(tile_rule, dst_extent, src_extent)
    |
    +---> kStretchImageRule:
    |         scale_factor = 1 (will be stretched by DrawImage)
    |         phase = 0, spacing = 0
    |
    +---> kRoundImageRule:
    |         repetitions = max(1, round(dst_extent / src_extent))
    |         scale_factor = dst_extent / (src_extent * repetitions)
    |         phase = 0, spacing = 0
    |
    +---> kRepeatImageRule:
    |         scale_factor = 1
    |         phase = (dst_extent - src_extent) / 2  [center the pattern]
    |         spacing = 0
    |
    +---> kSpaceImageRule:
              scale_factor = 1
              tiles = floor(dst_extent / src_extent)
              spacing = (dst_extent - src_extent * tiles) / (tiles + 1)
              phase = spacing
```
