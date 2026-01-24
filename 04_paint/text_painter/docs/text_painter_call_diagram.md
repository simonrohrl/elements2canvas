# Text Painter Call Flow Diagram

## Main Paint Flow

```
PaintLayerPainter::Paint()
    |
    v
PaintLayerPainter::PaintForegroundPhases()
    |
    +-- PaintWithPhase(PaintPhase::kForeground)
            |
            v
        PaintFragmentWithPhase()
            |
            v
        BoxFragmentPainter::Paint()
            |
            v
        [For each FragmentItem that IsText()]
            |
            v
        TextFragmentPainter::Paint()
```

## TextFragmentPainter::Paint() Flow

```
TextFragmentPainter::Paint(paint_info, paint_offset)
    |
    +-- [Early exits: no length, no shape result, invisible]
    |
    +-- Compute physical_box rect
    |
    +-- Check paint phase (kForeground -> record hit test data)
    |
    +-- Compute writing mode rotation (if not horizontal)
    |
    +-- [IF selected] Create SelectionPaintState
    |
    +-- [IF symbol marker] PaintSymbol() -> RETURN
    |
    +-- Compute text colors via TextPainter::TextPaintingStyle()
    |
    +-- Create TextPainter(context, font, visual_rect, text_origin)
    |
    +-- Create TextDecorationPainter(text_painter, paint_info, style, ...)
    |
    +-- Create HighlightPainter(fragment_info, text_painter, decoration_painter, ...)
    |
    +-- [IF SVG text] text_painter.SetSvgState()
    |
    +-- [IF paint_marker_backgrounds]
    |       highlight_painter.PaintNonCssMarkers(kBackground)
    |
    +-- [IF rotation] Apply rotation to context
    |
    +-- [IF emphasis mark needed]
    |       text_painter.SetEmphasisMark()
    |
    +-- SWITCH (highlight_painter.PaintCase())
    |
    +---[kNoHighlights / kFastSpellingGrammar]
    |       |
    |       +-- [IF shadows && decorations] highlight_painter.PaintOriginatingShadow()
    |       +-- decoration_painter.Begin(kOriginating)
    |       +-- decoration_painter.PaintExceptLineThrough()
    |       +-- text_painter.Paint(fragment_info, text_style, node_id, ...)
    |       +-- decoration_painter.PaintOnlyLineThrough()
    |       +-- [IF kFastSpellingGrammar] highlight_painter.FastPaintSpellingGrammarDecorations()
    |
    +---[kFastSelection]
    |       |
    |       +-- selection.PaintSuppressingTextProperWhereSelected()
    |
    +---[kOverlay]
    |       |
    |       +-- highlight_painter.PaintOriginatingShadow()
    |       +-- highlight_painter.PaintHighlightOverlays()
    |
    +---[kSelectionOnly]
    |       (no action here, selection painted later)
    |
    +-- [IF selection && paint_marker_backgrounds && kFastSelection]
    |       selection.PaintSelectionBackground()
    |
    +-- [IF phase == kForeground]
    |       highlight_painter.PaintNonCssMarkers(kForeground)
    |
    +-- [IF selection]
            SWITCH (highlight_case)
            +---[kFastSelection]
            |       selection.PaintSelectedText()
            +---[kSelectionOnly]
            |       decoration_painter.Begin(kSelection)
            |       decoration_painter.PaintExceptLineThrough()
            |       selection.PaintSelectedText()
            |       decoration_painter.PaintOnlyLineThrough()
            +---[kOverlay]
                    (already painted by PaintHighlightOverlays)
```

## HighlightPainter::PaintCase Decision Tree

```
ComputePaintCase()
    |
    +-- [IF selection && ShouldPaintSelectedTextOnly]
    |       RETURN kSelectionOnly
    |
    +-- [IF has target_, search_, or custom_ markers]
    |       RETURN kOverlay
    |
    +-- [IF selection && no spelling/grammar]
    |       |
    |       +-- [IF no originating or selection decorations]
    |       |       RETURN kFastSelection
    |       +-- ELSE
    |               RETURN kOverlay
    |
    +-- [IF spelling or grammar markers]
    |       |
    |       +-- [IF also has selection]
    |       |       RETURN kOverlay
    |       +-- [IF styles are trivial (default decorations only)]
    |       |       RETURN kFastSpellingGrammar
    |       +-- ELSE
    |               RETURN kOverlay
    |
    +-- [ELSE no highlights]
            RETURN kNoHighlights
```

## TextPainter::Paint() Flow

```
TextPainter::Paint(fragment_paint_info, text_style, node_id, auto_dark_mode, shadow_mode)
    |
    +-- [IF no shape_result] RETURN
    |
    +-- [IF kShadowsOnly && no shadow] RETURN
    |
    +-- UpdateGraphicsContext(context, text_style, state_saver, shadow_mode)
    |       |
    |       +-- Set fill color
    |       +-- Set stroke color/width (if needed)
    |       +-- Set text paint order (fill-stroke vs stroke-fill)
    |       +-- [IF shadows] Create DrawLooper and set on context
    |
    +-- [IF SVG text]
    |       PaintSvgTextFragment() -> DrawText with fill/stroke paints
    |
    +-- [ELSE]
    |       context.DrawText(font, fragment_paint_info, text_origin, ...)
    |
    +-- [IF emphasis_mark not empty]
    |       [IF emphasis color differs] SetFillColor()
    |       context.DrawEmphasisMarks(font, fragment_paint_info, emphasis_mark, ...)
    |
    +-- [IF text contains non-whitespace]
    |       context.GetPaintController().SetTextPainted()
    |
    +-- [IF font should not skip drawing]
            PaintTimingDetector::NotifyTextPaint(visual_rect)
```

## TextDecorationPainter Flow

```
TextDecorationPainter::Begin(text_item, phase)
    |
    +-- UpdateDecorationInfo() -> creates TextDecorationInfo if decorations exist
    +-- Compute clip_rect if selection present

TextDecorationPainter::PaintExceptLineThrough(fragment_paint_info)
    |
    +-- [IF has underline/overline/spelling/grammar decorations]
            |
            +-- ClipIfNeeded()
            +-- PaintUnderOrOverLineDecorations()
                    |
                    +-- FOR each applied decoration:
                            |
                            +-- [IF spelling/grammar error]
                            |       SetSpellingOrGrammarErrorLineData()
                            |       text_painter.PaintDecorationLine()
                            |
                            +-- [IF underline]
                            |       SetUnderlineLineData()
                            |       [IF ink skip auto] text_painter.ClipDecorationLine()
                            |       text_painter.PaintDecorationLine()
                            |
                            +-- [IF overline]
                                    SetOverlineLineData()
                                    [IF ink skip auto] text_painter.ClipDecorationLine()
                                    text_painter.PaintDecorationLine()

TextDecorationPainter::PaintOnlyLineThrough()
    |
    +-- [IF has line-through decorations]
            |
            +-- ClipIfNeeded()
            +-- PaintLineThroughDecorations()
                    |
                    +-- FOR each applied decoration with line-through:
                            SetLineThroughLineData()
                            text_painter.PaintDecorationLine()
```

## DecorationLinePainter::Paint() Flow

```
DecorationLinePainter::Paint(geometry, color, auto_dark_mode, flags)
    |
    +-- SWITCH (geometry.style)
        |
        +---[kWavyStroke]
        |       PaintWavyTextDecoration()
        |           |
        |           +-- GetWavyGeometry() -> cached wavy path and tile
        |           +-- Create shader from tile record
        |           +-- DrawRect with shader
        |
        +---[kDottedStroke / kDashedStroke]
        |       SetupStyledStroke()
        |       DrawLineAsStroke()
        |
        +---[kSolidStroke / kDoubleStroke]
                DrawLineAsRect()
                [IF double] DrawLineAsRect() for second line
```

## HighlightPainter::PaintHighlightOverlays() Flow (kOverlay case)

```
PaintHighlightOverlays(originating_text_style, node_id, paint_marker_backgrounds, rotation)
    |
    +-- FOR each layer (except originating):
    |       |
    |       +-- [IF selection && !paint_marker_backgrounds] SKIP
    |       |
    |       +-- Paint 'background-color' (merged parts where possible)
    |       |       FOR each part with background in this layer:
    |       |           Merge adjacent parts with same color
    |       |           PaintHighlightBackground()
    |       |
    |       +-- Paint 'text-shadow' (merged parts where possible)
    |               FOR each part with text-shadow in this layer:
    |                   Merge adjacent parts
    |                   text_painter.Paint(..., kShadowsOnly)
    |
    +-- FOR each part:
            |
            +-- Compute part_rect
            |
            +-- PaintDecorationsExceptLineThrough()
            |       FOR each decoration in part:
            |           UpdateDecorationInfo()
            |           ClipToPartRect()
            |           decoration_painter.PaintExceptLineThrough()
            |
            +-- [IF has shape_result]
            |       ClipToPartRect() (expanded for stroke/ink overflow)
            |       Adjust start/end for partial glyphs
            |       text_painter.Paint(..., kTextProperOnly)
            |
            +-- PaintDecorationsOnlyLineThrough()
                    Similar to except-line-through but for line-through only
```
