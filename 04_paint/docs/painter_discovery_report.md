# Chromium Paint System - Painter Discovery Report

## Overview

This report documents the discovery and organization of painter classes in Chromium's Blink rendering engine, starting from `paint_layer_painter.cc` as the entry point.

Source: `chromium/src/third_party/blink/renderer/core/paint/`

## Paint Flow Call Tree

The following ASCII diagram shows the call tree starting from `PaintLayerPainter::Paint()`:

```
PaintLayerPainter
|
+-- BoxFragmentPainter (main content painting)
|   |
|   +-- ObjectPainter (hit test data, all phases)
|   |   +-- OutlinePainter
|   |
|   +-- ScrollableAreaPainter (scrollbars, resizer)
|   |   +-- ObjectPainter
|   |
|   +-- FrameSetPainter (HTML frameset)
|   |   +-- BoxPainter
|   |   +-- BoxFragmentPainter
|   |
|   +-- FragmentPainter (outlines)
|   |   +-- OutlinePainter
|   |
|   +-- MathMLPainter (MathML elements)
|   |   +-- BoxFragmentPainter
|   |
|   +-- ViewPainter (root view)
|   |   +-- BoxModelObjectPainter
|   |   +-- BoxPainter
|   |   +-- ObjectPainter
|   |
|   +-- TablePainter (tables)
|   |   +-- BoxBorderPainter
|   |   +-- BoxFragmentPainter
|   |   +-- BoxModelObjectPainter
|   |   +-- BoxPainter
|   |   +-- ThemePainter
|   |
|   +-- TableSectionPainter
|   +-- TableRowPainter
|   +-- TableCellPainter
|   |
|   +-- FieldsetPainter
|   |   +-- BoxFragmentPainter
|   |   +-- ObjectPainter
|   |
|   +-- BoxBorderPainter (borders)
|   |   +-- BoxPainter
|   |   +-- ObjectPainter
|   |
|   +-- GapDecorationsPainter (grid/flex gaps)
|   |   +-- BoxBorderPainter
|   |   +-- BoxFragmentPainter
|   |
|   +-- TextFragmentPainter (text content)
|   |   |
|   |   +-- TextPainter (glyph rendering)
|   |   |   +-- DecorationLinePainter
|   |   |   +-- SVGObjectPainter
|   |   |
|   |   +-- HighlightPainter (selection, search)
|   |   |   +-- StyleableMarkerPainter
|   |   |   +-- TextDecorationPainter
|   |   |   +-- TextPainter
|   |   |
|   |   +-- TextDecorationPainter (underline, etc.)
|   |   |   +-- TextPainter
|   |   |
|   |   +-- BoxModelObjectPainter
|   |
|   +-- TextCombinePainter (vertical text combine)
|   |   +-- TextDecorationPainter
|   |
|   +-- InlineBoxFragmentPainter (inline boxes)
|   |   +-- BoxFragmentPainter
|   |   +-- NinePieceImagePainter
|   |   +-- ObjectPainter
|   |
|   +-- LineBoxFragmentPainter (line boxes)
|   |
|   +-- ThemePainter (form controls)
|
+-- InlineBoxFragmentPainter
|   +-- BoxFragmentPainter
|   +-- NinePieceImagePainter
|   +-- ObjectPainter
|
+-- ObjectPainter
|   +-- OutlinePainter
|
+-- ScrollableAreaPainter
|   +-- ObjectPainter
|
+-- SVGMaskPainter (SVG masks)
|
+-- ClipPathClipper (clip paths)
```

## All Painters Found

### Painters in Main Paint Flow (in_paint_flow)

These painters are called directly or indirectly from `PaintLayerPainter::Paint()`:

| Painter | Description |
|---------|-------------|
| PaintLayerPainter | Entry point for layer painting |
| BoxFragmentPainter | Main box/fragment painting |
| InlineBoxFragmentPainter | Inline box painting |
| LineBoxFragmentPainter | Line box painting |
| ObjectPainter | Base object painting, hit test |
| ScrollableAreaPainter | Scrollbars, scroll corners |
| TextFragmentPainter | Text fragment painting |
| TextPainter | Glyph rendering |
| TextDecorationPainter | Text decorations |
| TextCombinePainter | Vertical text combine |
| TextShadowPainter | Text shadows |
| DecorationLinePainter | Decoration lines |
| HighlightPainter | Text highlights/selection |
| StyleableMarkerPainter | Marker styling |
| BoxPainter | Box painting utilities |
| BoxModelObjectPainter | Box model painting |
| BoxBorderPainter | Border painting |
| OutlinePainter | Outline painting |
| ViewPainter | Root view painting |
| FramePainter | Frame painting |
| FrameSetPainter | Frameset painting |
| FragmentPainter | Fragment utilities |
| TablePainter | Table painting |
| TableSectionPainter | Table section painting |
| TableRowPainter | Table row painting |
| TableCellPainter | Table cell painting |
| FieldsetPainter | Fieldset painting |
| MathMLPainter | MathML painting |
| GapDecorationsPainter | Grid/Flex gap decorations |
| ThemePainter | Form control themes |
| NinePieceImagePainter | Border images |
| SVGMaskPainter | SVG masks |

### Painters Potentially Outside Main Flow (not_in_paint_flow)

These painters are specialized and may be called from specific contexts:

| Painter | Description |
|---------|-------------|
| BorderShapePainter | Border shape utilities |
| CSSMaskPainter | CSS mask painting |
| EmbeddedContentPainter | Embedded content (iframes) |
| EmbeddedObjectPainter | Embedded objects (plugins) |
| HTMLCanvasPainter | Canvas element |
| ImagePainter | Image element |
| ReplacedPainter | Replaced elements |
| SVGContainerPainter | SVG containers |
| SVGForeignObjectPainter | SVG foreignObject |
| SVGImagePainter | SVG image element |
| SVGModelObjectPainter | SVG model objects |
| SVGObjectPainter | SVG object base |
| SVGRootPainter | SVG root element |
| SVGShapePainter | SVG shapes |
| VideoPainter | Video element |

## Painter Groups

Painters have been organized into the following groups based on functionality:

### 1. text_painter (Text Rendering)

Files related to text rendering and decorations:

- `decoration_line_painter.{cc,h}`
- `highlight_painter.{cc,h}`
- `styleable_marker_painter.{cc,h}`
- `text_combine_painter.{cc,h}`
- `text_decoration_info.{cc,h}`
- `text_decoration_painter.{cc,h}`
- `text_fragment_painter.{cc,h}`
- `text_paint_style.h`
- `text_painter.{cc,h}`
- `text_shadow_painter.{cc,h}`

### 2. block_painter (Box/Block Painting)

Files related to box model and block-level painting:

- `box_fragment_painter.{cc,h}`
- `box_model_object_painter.{cc,h}`
- `box_painter.{cc,h}`
- `inline_box_fragment_painter.{cc,h}`

### 3. border_painter (Border Painting)

Files related to border rendering:

- `border_shape_painter.{cc,h}`
- `border_shape_utils.{cc,h}`
- `box_border_painter.{cc,h}`
- `contoured_border_geometry.{cc,h}`

### 4. table_painter (Table Painting)

Files related to HTML table rendering:

- `table_painters.{cc,h}` (contains TablePainter, TableSectionPainter, TableRowPainter, TableCellPainter)

### 5. svg_painter (SVG Painting)

Files related to SVG rendering:

- `svg_container_painter.{cc,h}`
- `svg_foreign_object_painter.{cc,h}`
- `svg_image_painter.{cc,h}`
- `svg_mask_painter.{cc,h}`
- `svg_model_object_painter.{cc,h}`
- `svg_object_painter.{cc,h}`
- `svg_root_painter.{cc,h}`
- `svg_shape_painter.{cc,h}`

### 6. replaced_painter (Replaced Elements)

Files related to replaced element painting (images, videos, etc.):

- `embedded_content_painter.{cc,h}`
- `embedded_object_painter.{cc,h}`
- `html_canvas_painter.{cc,h}`
- `image_painter.{cc,h}`
- `replaced_painter.{cc,h}`
- `video_painter.{cc,h}`

### 7. layout_painter (Layout/Structure Painting)

Files related to layout structure and coordination:

- `fragment_painter.{cc,h}`
- `frame_painter.{cc,h}`
- `frame_set_painter.{cc,h}`
- `object_painter.{cc,h}`
- `paint_layer_painter.{cc,h}`
- `view_painter.{cc,h}`

### 8. misc_painter (Miscellaneous)

Other specialized painters:

- `css_mask_painter.{cc,h}`
- `fieldset_painter.{cc,h}`
- `gap_decorations_painter.{cc,h}`
- `mathml_painter.{cc,h}`
- `nine_piece_image_painter.{cc,h}`
- `outline_painter.{cc,h}`
- `scrollable_area_painter.{cc,h}`
- `theme_painter.{cc,h}`

## Directories Created

The following directories were created/updated:

```
04_paint/
|-- text_painter/reference/      (updated with highlight_painter, styleable_marker_painter)
|-- block_painter/reference/     (created with box/inline painters)
|-- border_painter/reference/    (existing)
|-- table_painter/reference/     (new)
|-- svg_painter/reference/       (new)
|-- replaced_painter/reference/  (new)
|-- layout_painter/reference/    (new)
|-- misc_painter/reference/      (new)
+-- docs/
```

## Files Copied Summary

| Group | Files Count | Key Files |
|-------|-------------|-----------|
| text_painter | 19 | text_painter, text_fragment_painter, highlight_painter |
| block_painter | 8 | box_fragment_painter, box_painter, inline_box_fragment_painter |
| border_painter | 8 | box_border_painter, border_shape_painter |
| table_painter | 2 | table_painters |
| svg_painter | 16 | svg_*_painter files |
| replaced_painter | 12 | image_painter, video_painter, embedded_*_painter |
| layout_painter | 12 | paint_layer_painter, object_painter, view_painter |
| misc_painter | 16 | outline_painter, scrollable_area_painter, theme_painter |

## Key Observations

1. **PaintLayerPainter is the entry point**: All painting flows through `PaintLayerPainter::Paint()` which then dispatches to specialized painters.

2. **BoxFragmentPainter is central**: Most content painting goes through `BoxFragmentPainter`, which handles the main rendering of box model elements.

3. **Text painting is complex**: Text rendering involves multiple painters (TextPainter, TextFragmentPainter, TextDecorationPainter, HighlightPainter) working together.

4. **SVG has parallel hierarchy**: SVG elements have their own parallel set of painters (SVGContainerPainter, SVGShapePainter, etc.).

5. **ObjectPainter is foundational**: Many painters use `ObjectPainter` for common functionality like hit test data and outline painting.

6. **Recursive structure**: Several painters can call themselves recursively (BoxFragmentPainter for nested boxes) or call PaintLayerPainter for child layers.

## Paint Order

The typical paint order from `PaintLayerPainter::Paint()`:

1. Background phase (`kSelfBlockBackgroundOnly`)
2. Negative z-order children
3. Foreground phase (including text, inline content)
4. Positive z-order children
5. Outline phase
6. Overflow controls (if overlay)
7. Mask phase (if applicable)

This follows the CSS painting order specification for proper stacking context rendering.
