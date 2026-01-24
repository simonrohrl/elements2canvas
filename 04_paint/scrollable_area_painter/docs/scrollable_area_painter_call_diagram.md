# ScrollableAreaPainter Call Diagram

## Entry Points from PaintLayerPainter

### Paint Flow Overview

```
PaintLayerPainter::Paint()
    |
    +-- PaintWithPhase(kSelfBlockBackgroundOnly)
    |       |
    |       +-- (non-overlay controls painted here via BoxFragmentPainter)
    |
    +-- PaintChildren(kNegativeZOrderChildren)
    |
    +-- PaintForegroundPhases()
    |       |
    |       +-- (non-self-painting overlay controls via kForeground)
    |
    +-- PaintChildren(kNormalFlowAndPositiveZOrderChildren)
    |
    +-- PaintOverlayOverflowControls()  <-- Main entry point
            |
            +-- PaintWithPhase(kOverlayOverflowControls)
                    |
                    +-- BoxFragmentPainter::PaintOverflowControls()
                            |
                            +-- ScrollableAreaPainter::PaintOverflowControls()
```

## Main Entry Point: PaintOverflowControls

```
ScrollableAreaPainter::PaintOverflowControls(paint_info, paint_offset, fragment)
    |
    +-- [1] Validate fragment exists
    |
    +-- [2] Check box.IsScrollContainer()
    |
    +-- [3] Check visibility (EVisibility::kVisible)
    |
    +-- [4] Determine paint phase eligibility:
    |       |
    |       +-- Overlay controls + self-painting layer:
    |       |       -> kOverlayOverflowControls
    |       |
    |       +-- Overlay controls + non-self-painting:
    |       |       -> kForeground
    |       |
    |       +-- Non-overlay controls:
    |               -> kBackground (ShouldPaintSelfBlockBackground)
    |
    +-- [5] Set up property nodes:
    |       |
    |       +-- Get OverflowControlsClip from properties
    |       +-- Get TransformNodeForViewportScrollbars (for root scroller)
    |       +-- Create ScopedPaintChunkProperties if needed
    |
    +-- [6] Paint horizontal scrollbar
    |       |
    |       +-- PaintScrollbar(context, HorizontalScrollbar(), ...)
    |
    +-- [7] Paint vertical scrollbar
    |       |
    |       +-- PaintScrollbar(context, VerticalScrollbar(), ...)
    |
    +-- [8] Paint scroll corner
    |       |
    |       +-- PaintScrollCorner(context, paint_offset, cull_rect)
    |
    +-- [9] Paint resizer (painted last, on top of scroll corner)
            |
            +-- PaintResizer(context, paint_offset, cull_rect)
```

## PaintScrollbar Flow

```
ScrollableAreaPainter::PaintScrollbar(context, scrollbar, paint_offset, cull_rect)
    |
    +-- [1] Skip overlay scrollbars during printing
    |
    +-- [2] Calculate visual_rect from scrollbar.FrameRect() + paint_offset
    |
    +-- [3] Cull rect intersection test
    |
    +-- [4] Get scrollbar effect property node:
    |       |
    |       +-- HorizontalScrollbarEffect() or VerticalScrollbarEffect()
    |
    +-- [5] Create ScopedPaintChunkProperties with effect
    |
    +-- [6] Branch on scrollbar type:
            |
            +-- CustomScrollbar:
            |       |
            |       +-- To<CustomScrollbar>(scrollbar).Paint(context, paint_offset)
            |       +-- RecordScrollHitTestData() for hit testing
            |
            +-- Native Scrollbar:
                    |
                    +-- PaintNativeScrollbar(context, scrollbar, visual_rect)
```

## PaintNativeScrollbar Flow

```
ScrollableAreaPainter::PaintNativeScrollbar(context, scrollbar, visual_rect)
    |
    +-- [1] Determine DisplayItem type (kScrollbarHorizontal/kScrollbarVertical)
    |
    +-- [2] Check cached item: UseCachedItemIfPossible()
    |
    +-- [3] Get ScrollTranslation if compositable
    |       |
    |       +-- MayCompositeScrollbar() check
    |       +-- Get ScrollTranslation from properties
    |
    +-- [4] Calculate hit test opaqueness:
    |       |
    |       +-- AllowsHitTest() -> GetHitTestOpaqueness()
    |       +-- Convert kMixed to kOpaque for scrollbars
    |       +-- Or kTransparent if no hit test
    |
    +-- [5] Create ScrollbarLayerDelegate
    |
    +-- [6] ScrollbarDisplayItem::Record()
            |
            +-- Records scrollbar for compositor
            +-- Includes scroll_translation for compositing
            +-- Includes element_id for layer binding
```

## PaintScrollCorner Flow

```
ScrollableAreaPainter::PaintScrollCorner(context, paint_offset, cull_rect)
    |
    +-- [1] Get visual_rect from ScrollCornerRect() + paint_offset
    |
    +-- [2] Cull rect intersection test
    |
    +-- [3] Get display item client
    |
    +-- [4] Set up ScrollCornerEffect property if exists
    |
    +-- [5] Branch on scroll corner type:
            |
            +-- Custom ScrollCorner (CSS-styled):
            |       |
            |       +-- CustomScrollbarTheme::PaintIntoRect()
            |
            +-- Overlay scrollbars:
            |       |
            |       +-- Return early (no opaque corner needed)
            |
            +-- Native scroll corner:
                    |
                    +-- Get ScrollbarTheme from scrollbar
                    +-- theme->PaintScrollCorner(context, scrollable_area_, client, visual_rect)
```

## PaintResizer Flow

```
ScrollableAreaPainter::PaintResizer(context, paint_offset, cull_rect)
    |
    +-- [1] Check visibility and CanResize()
    |
    +-- [2] Get visual_rect from ResizerCornerRect(kResizerForPointer)
    |
    +-- [3] Cull rect intersection test
    |
    +-- [4] Get display item client
    |
    +-- [5] Branch on resizer type:
            |
            +-- Custom Resizer:
            |       |
            |       +-- CustomScrollbarTheme::PaintIntoRect(*resizer, ...)
            |
            +-- Platform Resizer:
                    |
                    +-- Check DrawingRecorder cache
                    +-- Create DrawingRecorder
                    +-- DrawPlatformResizerImage()
                    +-- (Optionally) Draw frame around resizer
```

## DrawPlatformResizerImage Flow

```
ScrollableAreaPainter::DrawPlatformResizerImage(context, resizer_corner_rect)
    |
    +-- [1] Calculate 4 points for grip lines
    |       |
    |       +-- Adjust for LTR/RTL direction
    |       +-- Scale based on ScaleFromDIP()
    |
    +-- [2] Set up paint flags:
    |       |
    |       +-- kStroke_Style
    |       +-- StrokeWidth from paint_scale
    |
    +-- [3] Set up AutoDarkMode
    |
    +-- [4] Draw dark grip lines:
    |       |
    |       +-- SkPath with two diagonal lines
    |       +-- Color: SkColorSetARGB(153, 0, 0, 0)
    |       +-- context.DrawPath(dark_line_path, ...)
    |
    +-- [5] Draw light grip lines (offset):
            |
            +-- SkPath offset by v_offset/h_offset
            +-- Color: SkColorSetARGB(153, 255, 255, 255)
            +-- context.DrawPath(light_line_path, ...)
```

## Property Tree Integration

```
                            PropertyTreeStateOrAlias
                                     |
            +------------------------+------------------------+
            |                        |                        |
    TransformNode               ClipNode               EffectNode
            |                        |                        |
            v                        v                        v
    +-------------------+   +-------------------+   +-------------------+
    | ScrollTranslation |   | OverflowControls  |   | Scrollbar Effects |
    | (for composited   |   | Clip              |   | - Horizontal      |
    |  scrollbars)      |   |                   |   | - Vertical        |
    +-------------------+   +-------------------+   | - ScrollCorner    |
            |                        |              +-------------------+
            v                        v                        |
    Viewport Scrollbar      Clips scrollbars              Isolates
    Transform (root         to visible area              opacity/blend
    scroller only)
```

## Display Item Recording

```
Paint Method                 Display Item Type              Recorder Type
------------                 -----------------              -------------
PaintNativeScrollbar    ->   kScrollbarHorizontal/Vertical  ScrollbarDisplayItem
PaintScrollCorner       ->   kScrollCorner                  DrawingRecorder (theme)
PaintResizer            ->   kResizer                       DrawingRecorder
RecordResizerScroll...  ->   kResizerScrollHitTest          RecordScrollHitTestData
PaintScrollbar(custom)  ->   kScrollbarHitTest              RecordScrollHitTestData
```
