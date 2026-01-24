# Layout Painter Call Diagram

## Main Paint Flow

```
FramePainter::Paint()
|
+-- [Early returns: throttling, inactive document, no layout view]
|
+-- PaintLayerPainter(*root_layer).Paint()
    |
    +-- [Early returns: needs layout, throttled, fragmentless box]
    |
    +-- [Skip if non-self-painting without descendants]
    |
    +-- [Position visibility checks]
    |
    +-- [Logging: SHAPE, LAYER, STACKING, PROPERTIES at root]
    |
    +-- ScopedEffectivelyInvisible (if output invisible)
    |
    +-- ScopedPaintChunkProperties (layer_chunk_properties)
    |
    +-- PaintWithPhase(kSelfBlockBackgroundOnly) -----> PaintFragmentWithPhase()
    |
    +-- PaintChildren(kNegativeZOrderChildren)
    |   |
    |   +-- PaintLayerPaintOrderIterator
    |   +-- foreach child: PaintLayerPainter(*child).Paint() [RECURSIVE]
    |
    +-- ScopedPaintChunkProperties (foreground_properties)
    |
    +-- PaintForegroundPhases()
    |   |
    |   +-- PaintWithPhase(kDescendantBlockBackgroundsOnly)
    |   +-- PaintWithPhase(kForcedColorsModeBackplate) [if forced colors]
    |   +-- PaintWithPhase(kFloat) [if needed]
    |   +-- PaintWithPhase(kForeground)
    |   +-- PaintWithPhase(kDescendantOutlinesOnly) [if needed]
    |
    +-- PaintWithPhase(kSelfOutlineOnly) [if !video && has outline]
    |
    +-- PaintChildren(kNormalFlowAndPositiveZOrderChildren)
    |   |
    |   +-- PaintLayerPaintOrderIterator
    |   +-- foreach child: PaintLayerPainter(*child).Paint() [RECURSIVE]
    |   +-- PaintOverlayOverflowControls (if reorder needed)
    |
    +-- PaintOverlayOverflowControls() [if overlay, not reordered]
    |
    +-- PaintWithPhase(kSelfOutlineOnly) [if video && has outline]
    |
    +-- [If has paint properties:]
        +-- SVGMaskPainter::Paint() or PaintWithPhase(kMask)
        +-- ClipPathClipper::PaintClipPathAsMaskImage()
        +-- PaintTransitionPseudos()
        +-- PaintTransitionScopeSnapshotIfNeeded()
```

## PaintWithPhase Detail

```
PaintWithPhase(phase)
|
+-- FragmentDataIterator (iterate all fragments)
|
+-- foreach fragment:
    |
    +-- GetPhysicalFragment() [if NG box]
    |
    +-- ScopedDisplayItemFragment [if not first fragment]
    |
    +-- PaintFragmentWithPhase(phase, fragment, physical_fragment)
        |
        +-- [Early return if cull rect empty]
        |
        +-- ScopedPaintChunkProperties (chunk_properties)
        |   |
        |   +-- [For kMask: use Mask effect and clip]
        |
        +-- PaintInfo construction
        |
        +-- [Dispatch based on content type:]
        |
        +-- BoxFragmentPainter(*physical_fragment).Paint(paint_info)
        |   [NG box fragment path]
        |
        +-- InlineBoxFragmentPainter::PaintAllFragments()
        |   [NG inline path]
        |
        +-- layout_object_.Paint(paint_info)
            [Legacy path with fragment_data_override]
```

## PaintChildren Detail

```
PaintChildren(children_to_visit)
|
+-- [Early return if no self-painting descendants]
|
+-- [Early return if child painting blocked by display lock]
|
+-- [Early return if canvas element (prevent fallback)]
|
+-- PaintLayerPaintOrderIterator(paint_layer_, children_to_visit)
|
+-- while (child = iterator.Next()):
    |
    +-- [Skip if replaced normal flow stacking]
    |
    +-- [Skip view-transition pseudo (painted separately)]
    |
    +-- PaintLayerPainter(*child).Paint(context, paint_flags)
    |   [RECURSIVE CALL]
    |
    +-- [Handle overlay overflow controls reordering:]
        +-- foreach layer in LayersPaintingOverlayOverflowControlsAfter:
            +-- PaintLayerPainter(*layer).PaintOverlayOverflowControls()
```

## ObjectPainter Dispatch

```
ObjectPainter::PaintAllPhasesAtomically(paint_info)
|
+-- [If kSelectionDragImage or kTextClip:]
|   +-- layout_object_.Paint(paint_info)
|   +-- return
|
+-- [If not kForeground: return]
|
+-- [Paint all phases sequentially:]
    +-- paint phase = kBlockBackground
    +-- layout_object_.Paint(info)
    +-- paint phase = kForcedColorsModeBackplate
    +-- layout_object_.Paint(info)
    +-- paint phase = kFloat
    +-- layout_object_.Paint(info)
    +-- paint phase = kForeground
    +-- layout_object_.Paint(info)
    +-- paint phase = kOutline
    +-- layout_object_.Paint(info)
```

## ViewPainter Flow

```
ViewPainter::PaintBoxDecorationBackground(paint_info)
|
+-- [Early returns: visibility, paginated root]
|
+-- [Determine what to paint:]
|   +-- paints_hit_test_data
|   +-- paints_element_tracking/region_capture
|   +-- paints_scroll_hit_test
|   +-- is_represented_via_pseudo_elements
|
+-- [Calculate background_rect]
|
+-- [If should_paint_background && painting_background_in_contents_space:]
|   +-- [Compare document_element_state vs root_element_background_painting_state]
|   +-- PaintRootGroup() [if states differ]
|
+-- ScopedPaintChunkProperties [if contents space]
|
+-- PaintRootElementGroup() [if should_paint_background]
|
+-- ObjectPainter::RecordHitTestData() [if paints_hit_test_data]
|
+-- BoxPainter::RecordTrackedElementAndRegionCaptureData() [if needed]
|
+-- BoxPainter::RecordScrollHitTestData() [if paints_scroll_hit_test]
```

## FrameSetPainter Flow

```
FrameSetPainter::PaintObject(paint_info, paint_offset)
|
+-- [Early returns: not foreground phase, no children, not visible]
|
+-- PaintInfo paint_info_for_descendants = paint_info.ForDescendants()
|
+-- PaintChildren(paint_info_for_descendants)
|   |
|   +-- [Early return if painting blocked]
|   +-- foreach child in box_fragment_.Children():
|       +-- [Skip self-painting layers]
|       +-- BoxFragmentPainter::PaintFragment(child_fragment, paint_info)
|
+-- PaintBorders(paint_info, paint_offset)
    |
    +-- DrawingRecorder
    +-- foreach row/col:
        +-- PaintColumnBorder() or PaintRowBorder()
        |   +-- context.FillRect() [fill]
        |   +-- context.FillRect() [start edge]
        |   +-- context.FillRect() [end edge]
```

## Hit Test Data Recording

```
ObjectPainter::RecordHitTestData(paint_info, paint_rect, background_client)
|
+-- [Early return if omitting compositing info]
|
+-- [Early return if not visible]
|
+-- paint_info.context.GetPaintController().RecordHitTestData(
        background_client,
        paint_rect,
        layout_object_.EffectiveAllowedTouchAction(),
        layout_object_.InsideBlockingWheelEventHandler(),
        GetHitTestOpaqueness())

ObjectPainter::GetHitTestOpaqueness()
|
+-- [If not visible to hit testing or frame not visible:]
|   return kTransparent
|
+-- [If has border radius:]
|   return kMixed
|
+-- [If SVG child:]
|   return kMixed
|
+-- return kOpaque
```

## Subsequence Caching

```
ShouldCreateSubsequence(paint_layer, context, paint_flags)
|
+-- [Early returns:]
|   +-- printing/preview: NO
|   +-- skipping cache: NO
|   +-- !SupportsSubsequenceCaching: NO
|   +-- kOmitCompositingInfo flag: NO
|
+-- [YES if: different properties would create new chunk]
|
+-- [YES if: at least 2 descendants]
|
+-- [YES if: hit test opaqueness mismatch]
|
+-- [YES if: merged bounds would be too empty]
|
+-- return false

In Paint():
+-- if should_create_subsequence:
    +-- [Try cache: SubsequenceRecorder::UseCachedSubsequenceIfPossible]
    +-- SubsequenceRecorder subsequence_recorder(context, paint_layer_)
```

## Legend

```
[condition]     - Conditional check
+--             - Call or step
---->           - Delegation/dispatch
[RECURSIVE]     - Recursive call back to same function
```
