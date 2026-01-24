# Block Painter Call Diagram

This document provides a visual representation of the call flow through the block painter group.

## Main Call Flow from PaintLayerPainter

```
PaintLayerPainter::Paint()
    |
    +-- [kSelfBlockBackgroundOnly]
    |   PaintWithPhase(PaintPhase::kSelfBlockBackgroundOnly)
    |       |
    |       +-- PaintFragmentWithPhase()
    |               |
    |               +-- BoxFragmentPainter(*physical_fragment).Paint(paint_info)
    |
    +-- PaintChildren(kNegativeZOrderChildren)
    |       |
    |       +-- PaintLayerPainter(*child).Paint()  [recursive]
    |
    +-- [kForeground]
    |   PaintForegroundPhases()
    |       |
    |       +-- PaintWithPhase(kDescendantBlockBackgroundsOnly)
    |       +-- PaintWithPhase(kForcedColorsModeBackplate)
    |       +-- PaintWithPhase(kFloat)
    |       +-- PaintWithPhase(kForeground)
    |       +-- PaintWithPhase(kDescendantOutlinesOnly)
    |
    +-- [kSelfOutlineOnly]
    |   PaintWithPhase(PaintPhase::kSelfOutlineOnly)
    |
    +-- PaintChildren(kNormalFlowAndPositiveZOrderChildren)
    |       |
    |       +-- PaintLayerPainter(*child).Paint()  [recursive]
    |
    +-- PaintOverlayOverflowControls()
```

## BoxFragmentPainter::Paint() Internal Flow

```
BoxFragmentPainter::Paint(paint_info)
    |
    +-- [if hidden] return
    |
    +-- [if atomically painted without self-painting layer]
    |   PaintAllPhasesAtomically()
    |       |
    |       +-- PaintInternal(kBlockBackground)
    |       +-- PaintInternal(kForcedColorsModeBackplate)
    |       +-- PaintInternal(kFloat)
    |       +-- PaintInternal(kForeground)
    |       +-- PaintInternal(kOutline)
    |
    +-- [else]
        PaintInternal(paint_info)
```

## BoxFragmentPainter::PaintInternal() Flow

```
PaintInternal(paint_info)
    |
    +-- ScopedPaintState paint_state(box_fragment_, paint_info)
    |
    +-- ShouldPaint(paint_state)? [visibility/cull rect check]
    |
    +-- [kSelfBlockBackgroundOnly or kBlockBackground]
    |   |
    |   +-- PaintObject(info, paint_offset)  [border box space]
    |   |       |
    |   |       +-- PaintBoxDecorationBackground()
    |   |
    |   +-- [if scrolls overflow]
    |       PaintOverflowControls()
    |       PaintObject(info, paint_offset)  [contents space]
    |
    +-- [kDescendantBlockBackgroundsOnly]
    |   PaintObject(info, paint_offset)
    |
    +-- [other phases: kForeground, kFloat, etc.]
    |   ScopedBoxContentsPaintState contents_paint_state
    |   PaintObject(contents_paint_state.GetPaintInfo(), ...)
    |
    +-- [kSelfOutlineOnly]
    |   PaintObject(info, paint_offset)
    |
    +-- PaintCaretsIfNeeded()
    |
    +-- PaintOverflowControls() [if not painted earlier]
```

## BoxFragmentPainter::PaintObject() Flow

```
PaintObject(paint_info, paint_offset, suppress_box_decoration_background)
    |
    +-- [if FrameSet]
    |   FrameSetPainter(...).PaintObject()
    |   return
    |
    +-- IsVisibleToPaint()?
    |
    +-- [kSelfBlockBackgroundOnly]
    |   PaintBoxDecorationBackground(paint_info, paint_offset, suppress)
    |   return
    |
    +-- [kMask && visible]
    |   PaintMask(paint_info, paint_offset)
    |   return
    |
    +-- [kForeground]
    |   FragmentPainter.AddURLRectIfNeeded()
    |   MathMLPainter.Paint()  [if MathML]
    |
    +-- [Paint children - not kSelfOutlineOnly]
    |   |
    |   +-- [kDescendantBlockBackgroundsOnly && has column rule]
    |   |   PaintColumnRules()
    |   |
    |   +-- [if inline_box_cursor_]
    |   |   PaintInlineItems()  [self-painting inline box]
    |   |
    |   +-- [if items_]
    |   |   PaintLineBoxes()  [inline formatting context]
    |   |
    |   +-- [if paginated root]
    |   |   PaintCurrentPageContainer()
    |   |
    |   +-- [else - block formatting context]
    |       PaintBlockChildren()
    |
    +-- [kFloat or kSelectionDragImage or kTextClip]
    |   PaintFloats()
    |
    +-- [if table && kDescendantBlockBackgroundsOnly]
    |   TablePainter.PaintCollapsedBorders()
    |
    +-- [kSelfOutlineOnly && visible && has outline]
        FragmentPainter.PaintOutline()
```

## PaintBoxDecorationBackground() Flow

```
PaintBoxDecorationBackground(paint_info, paint_offset, suppress)
    |
    +-- [if LayoutView or PageContainer]
    |   ViewPainter.PaintBoxDecorationBackground()
    |   return
    |
    +-- [determine paint_rect and background_client]
    |   |
    |   +-- [if painting in contents space]
    |   |   ScopedBoxContentsPaintState
    |   |   paint_rect = scrollable overflow rect
    |   |
    |   +-- [else]
    |       paint_rect = border box rect
    |
    +-- [if !suppress && !skip_background]
    |   PaintBoxDecorationBackgroundWithRect()
    |       |
    |       +-- BoxDecorationData(paint_info, box_fragment_)
    |       |
    |       +-- [if can composite fixed background]
    |       |   PaintCompositeBackgroundAttachmentFixed()
    |       |   PaintBoxDecorationBackgroundWithDecorationData() [borders only]
    |       |
    |       +-- [else]
    |           PaintBoxDecorationBackgroundWithDecorationData()
    |
    +-- [if gap decorations enabled]
    |   PaintGapDecorations()
    |
    +-- [if should record hit test]
    |   ObjectPainter.RecordHitTestData()
    |
    +-- [if not painting in contents space]
        RecordScrollHitTestData()
```

## PaintBoxDecorationBackgroundWithDecorationData() Flow

```
PaintBoxDecorationBackgroundWithDecorationData()
    |
    +-- DrawingRecorder.UseCachedDrawingIfPossible()?
    |
    +-- DrawingRecorder recorder(...)
    |
    +-- [dispatch based on fragment type]
    |   |
    |   +-- [if fieldset]
    |   |   FieldsetPainter.PaintBoxDecorationBackground()
    |   |
    |   +-- [if table cell]
    |   |   TableCellPainter.PaintBoxDecorationBackground()
    |   |
    |   +-- [if table row]
    |   |   TableRowPainter.PaintBoxDecorationBackground()
    |   |
    |   +-- [if table section]
    |   |   TableSectionPainter.PaintBoxDecorationBackground()
    |   |
    |   +-- [if table]
    |   |   TablePainter.PaintBoxDecorationBackground()
    |   |
    |   +-- [else - regular box]
    |       PaintBoxDecorationBackgroundWithRectImpl()
```

## PaintBoxDecorationBackgroundWithRectImpl() - Core Painting

```
PaintBoxDecorationBackgroundWithRectImpl(paint_info, paint_rect, box_decoration_data)
    |
    +-- [1. NORMAL BOX SHADOW]
    |   [if ShouldPaintShadow()]
    |   BoxPainterBase::PaintNormalBoxShadow(paint_info, paint_rect, style, ...)
    |       |
    |       +-- ContouredBorderGeometry::PixelSnappedContouredBorder()
    |       +-- [for each shadow in reverse order]
    |           +-- [if style == kNormal]
    |               DrawLooperBuilder.AddShadow()
    |               context.FillRect() or context.FillContouredRect()
    |
    +-- [2. BACKGROUND CLIPPING (if needed)]
    |   [if BleedAvoidanceIsClipping()]
    |   ContouredBorderGeometry::PixelSnappedContouredBorder()
    |   paint_info.context.ClipContouredRect(border)
    |   [if kBackgroundBleedClipLayer]
    |       context.BeginLayer()
    |
    +-- [3. THEME PAINTING]
    |   [if HasAppearance()]
    |   ThemePainter.Paint()
    |
    +-- [4. BACKGROUND]
    |   [if ShouldPaintBackground() && !theme_painted]
    |   PaintBackground(paint_info, paint_rect, background_color, bleed_avoidance)
    |       |
    |       +-- [if transfers to view] return
    |       +-- [if obscured] return
    |       +-- BoxBackgroundPaintContext(box_fragment_)
    |       +-- PaintFillLayers(paint_info, bg_color, BackgroundLayers(), ...)
    |
    +-- [5. THEME DECORATIONS]
    |   [if HasAppearance() && !theme_painted]
    |   ThemePainter.PaintDecorations()
    |
    +-- [6. INSET BOX SHADOW]
    |   [if ShouldPaintShadow()]
    |   |
    |   +-- [if table cell]
    |   |   BoxPainterBase::PaintInsetBoxShadowWithInnerRect()
    |   |
    |   +-- [else]
    |       BoxPainterBase::PaintInsetBoxShadowWithBorderRect()
    |           |
    |           +-- ContouredBorderGeometry::PixelSnappedContouredInnerBorder()
    |           +-- [for each shadow]
    |               +-- [if style == kInset]
    |                   DrawLooperBuilder.AddShadow()
    |                   context.FillContouredRect() [inverted]
    |
    +-- [7. BORDER]
    |   [if ShouldPaintBorder() && !theme_painted]
    |   BoxPainterBase::PaintBorder(...)
    |       |
    |       +-- [if border-image]
    |       |   NinePieceImagePainter::Paint()
    |       |
    |       +-- [else]
    |           BoxBorderPainter::Paint()
    |
    +-- [8. LAYER CLEANUP]
        [if needs_end_layer]
        context.EndLayer()
```

## InlineBoxFragmentPainter Flow

```
InlineBoxFragmentPainter::Paint(paint_info, paint_offset)
    |
    +-- ScopedDisplayItemFragment(paint_info.context, inline_box_item_.FragmentId())
    |
    +-- [if not SVG inline]
    |   |
    |   +-- [kMask]
    |   |   PaintMask()
    |   |
    |   +-- [kForeground]
    |       PaintBackgroundBorderShadow()
    |
    +-- [else - SVG inline]
    |   ScopedSVGPaintState(...)
    |
    +-- InlinePaintContext::ScopedInlineItem(inline_box_item_, inline_context_)
    |
    +-- BoxFragmentPainter(...).PaintObject(paint_info, adjusted_paint_offset,
    |                                        suppress_box_decoration_background=true)
```

## PaintBackgroundBorderShadow() for Inline Boxes

```
InlineBoxFragmentPainterBase::PaintBackgroundBorderShadow()
    |
    +-- [visibility check]
    |
    +-- [record tracked element data if needed]
    |
    +-- [check HasBoxDecorationBackground() or UsesFirstLineStyle()]
    |
    +-- DrawingRecorder.UseCachedDrawingIfPossible()?
    |
    +-- DrawingRecorder recorder(...)
    |
    +-- PaintBoxDecorationBackground(box_painter, paint_info, ...)
        |
        +-- [1. Normal box shadow]
        |   PaintNormalBoxShadow()
        |
        +-- [2. Background fill layers]
        |   PaintFillLayers()
        |
        +-- [3. Inset box shadow]
        |   PaintInsetBoxShadow()
        |
        +-- [4. Border]
            [determine SlicePaintingType]
            |
            +-- [kDontPaint] return
            |
            +-- [kPaintWithoutClip]
            |   BoxPainterBase::PaintBorder()
            |
            +-- [kPaintWithClip]
                context.Clip()
                BoxPainterBase::PaintBorder() [with image strip rect]
```
