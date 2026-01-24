# ScrollableAreaPainter Class Hierarchy

## Overview

`ScrollableAreaPainter` is a stack-allocated painter class responsible for rendering overflow controls (scrollbars, scroll corners, and resizers) for scrollable areas in the Blink rendering engine.

## Class Definition

```
Location: third_party/blink/renderer/core/paint/scrollable_area_painter.h
```

```cpp
class ScrollableAreaPainter {
  STACK_ALLOCATED();

public:
  explicit ScrollableAreaPainter(const PaintLayerScrollableArea& scrollable_area);

  // Public paint methods
  bool PaintOverflowControls(const PaintInfo&, const PhysicalOffset&, const FragmentData*);
  void PaintResizer(GraphicsContext&, const PhysicalOffset&, const CullRect&);
  void RecordResizerScrollHitTestData(GraphicsContext&, const PhysicalOffset&);

private:
  void PaintScrollbar(GraphicsContext&, Scrollbar&, const PhysicalOffset&, const CullRect&);
  void PaintScrollCorner(GraphicsContext&, const PhysicalOffset&, const CullRect&);
  void DrawPlatformResizerImage(GraphicsContext&, const gfx::Rect&);
  void PaintNativeScrollbar(GraphicsContext&, Scrollbar&, gfx::Rect);

  const PaintLayerScrollableArea& scrollable_area_;
};
```

## Associated Classes

### Core Dependencies

```
PaintLayerScrollableArea
    |
    +-- Contains scroll state, scrollbars, resizers
    |
    +-- Methods used:
        - GetLayoutBox()
        - HorizontalScrollbar()
        - VerticalScrollbar()
        - ScrollCorner()
        - Resizer()
        - ScrollCornerRect()
        - ResizerCornerRect()
        - GetScrollCornerDisplayItemClient()
        - ShouldOverflowControlsPaintAsOverlay()
        - HasOverlayScrollbars()
        - NeedsScrollCorner()
        - MayCompositeScrollbar()
        - ScaleFromDIP()
```

### Scrollbar Hierarchy

```
Scrollbar (base)
    |
    +-- CustomScrollbar
    |       |
    |       +-- CSS-styled scrollbars
    |       +-- Uses CustomScrollbarTheme
    |
    +-- Native Scrollbar
            |
            +-- Platform-specific rendering
            +-- Uses ScrollbarTheme
```

### Theme Classes

```
ScrollbarTheme (base)
    |
    +-- CustomScrollbarTheme
    |       |
    |       +-- Renders CSS ::-webkit-scrollbar parts
    |       +-- Uses LayoutCustomScrollbarPart
    |
    +-- Platform-specific themes
            |
            +-- ScrollbarThemeMac
            +-- ScrollbarThemeAura
            +-- etc.
```

## Class Relationships

```
                                PaintLayerPainter
                                       |
                                       | calls via
                                       | PaintOverlayOverflowControls()
                                       v
+-------------------+           ScrollableAreaPainter
|                   |                  |
| PaintLayer        |<-----------------+  references
| ScrollableArea    |                  |
|                   |                  v
+-------------------+           +------+------+
                                |             |
                                v             v
                          CustomScrollbar  Scrollbar
                                |             |
                                v             v
                        CustomScrollbar   ScrollbarTheme
                           Theme         (platform)
```

## Paint Property Nodes Used

### Transform Properties
- `ScrollTranslation` - For composited scrollbars
- `TransformNodeForViewportScrollbars` - For global root scroller

### Clip Properties
- `OverflowControlsClip` - Clips overflow controls to visible area

### Effect Properties
- `HorizontalScrollbarEffect` - Effect for horizontal scrollbar
- `VerticalScrollbarEffect` - Effect for vertical scrollbar
- `ScrollCornerEffect` - Effect for scroll corner

## Display Item Types

```
DisplayItem::kOverflowControls     - Chunk marker for overflow controls
DisplayItem::kScrollbarHorizontal  - Horizontal scrollbar display item
DisplayItem::kScrollbarVertical    - Vertical scrollbar display item
DisplayItem::kScrollCorner         - Scroll corner display item
DisplayItem::kResizer              - Resizer display item
DisplayItem::kResizerScrollHitTest - Hit test data for resizer
DisplayItem::kScrollbarHitTest     - Hit test data for custom scrollbars
```

## Integration with PaintLayerPainter

`ScrollableAreaPainter` is invoked from `PaintLayerPainter` in the following scenarios:

1. **Overlay overflow controls** (lines 1111-1119 in paint_layer_painter.cc):
   ```cpp
   if (should_paint_content && paint_layer_.GetScrollableArea() &&
       paint_layer_.GetScrollableArea()->ShouldOverflowControlsPaintAsOverlay()) {
     if (!paint_layer_.NeedsReorderOverlayOverflowControls()) {
       PaintOverlayOverflowControls(context, paint_flags);
     }
   }
   ```

2. **Reordered overlay overflow controls** (painted after scrolling children):
   ```cpp
   PaintLayerPainter(*reparent_overflow_controls_layer)
       .PaintOverlayOverflowControls(context, paint_flags);
   ```

3. **Non-overlay overflow controls** are painted during `PaintPhase::kBackground`

## Member Variable

```cpp
const PaintLayerScrollableArea& scrollable_area_;
```

This reference provides access to:
- The associated `LayoutBox`
- Scrollbar instances (horizontal and vertical)
- Scroll corner and resizer layout objects
- Scrollable area dimensions and state
- Display item clients for caching
