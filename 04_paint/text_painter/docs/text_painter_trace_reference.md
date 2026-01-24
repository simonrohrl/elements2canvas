# Text Painter Trace Reference

This document provides a detailed line-by-line trace of the key functions in the text painting system, noting decision points and graphics context operations.

## TextFragmentPainter::Paint()

**File:** `text_fragment_painter.cc:221-617`

### Early Exit Checks (Lines 223-236)

| Line | Condition | Action |
|------|-----------|--------|
| 225-226 | `!text_item.TextLength()` | Return early (empty text) |
| 228-231 | `!text_item.TextShapeResult() && !text_item.IsLineBreak()` | Return early (no glyphs, not line break) |
| 234-236 | `style.Visibility() != EVisibility::kVisible` | Return early (invisible) |

### Geometry Setup (Lines 238-275)

| Line | Operation | Notes |
|------|-----------|-------|
| 238-239 | Get `TextFragmentPaintInfo` | Contains text, offsets, shape result |
| 246-249 | Compute `physical_box` | Via `PhysicalBoxRect()` helper |
| 267-275 | Compute `rotation` | Only for non-horizontal writing modes |
| 270-271 | Create `rotated_box` | `LineRelativeRect::CreateFromLineBox()` |

### Selection State (Lines 277-304)

| Line | Condition | Action |
|------|-----------|--------|
| 281-283 | Not printing, not rendering resource, not text clip, is selected | Create `SelectionPaintState` |
| 292-293 | `selection_for_bounds_recording->Status().HasValidRange()` | Set `selection` pointer |
| 295-299 | No selection | Early exit if kSelectionDragImage phase |
| 302-303 | `text_item.IsFlowControl()` | Early exit (line break, tab, wbr) |

### Hit Test Data (Lines 256-263)

| Line | Condition | Action |
|------|-----------|--------|
| 256 | `paint_info.phase == PaintPhase::kForeground` | Check if should record hit test |
| 258-262 | `object_painter.ShouldRecordSpecialHitTestData()` | Call `RecordHitTestData()` |

### Symbol Marker Handling (Lines 387-391)

| Line | Condition | Action |
|------|-----------|--------|
| 387 | `text_item.IsSymbolMarker()` | Paint bullet/number via `PaintSymbol()` |
| 388-390 | | Return after painting symbol |

### Drawing Recorder Setup (Lines 371-385)

| Line | Condition | Action |
|------|-----------|--------|
| 376 | `paint_info.phase != PaintPhase::kTextClip` | Text clips don't need recorder |
| 377-381 | `!context.InDrawingRecorder()` | Check cache, create recorder |

### Painter Object Creation (Lines 428-435)

| Line | Object | Parameters |
|------|--------|------------|
| 428-429 | `TextPainter` | context, svg_paints, font, visual_rect, text_origin |
| 430-432 | `TextDecorationPainter` | text_painter, inline_context, paint_info, style, text_style, rotated_box, selection |
| 433-435 | `HighlightPainter` | fragment_info, text_painter, decoration_painter, paint_info, cursor, fragment_item, offset, style, text_style, selection |

### SVG Text Setup (Lines 450-467)

| Line | Condition | Action |
|------|-----------|--------|
| 450 | `svg_inline_text` | Set SVG state on text_painter |
| 455-458 | `scaling_factor != 1.0f` | Scale context, update shader transform |
| 460-466 | `HasSvgTransformForPaint()` | Concat fragment transform |

### Non-CSS Marker Backgrounds (Lines 485-494)

| Line | Condition | Action |
|------|-----------|--------|
| 485-487 | Not drag image, not text clip, not printing | `paint_marker_backgrounds = true` |
| 492-493 | `paint_marker_backgrounds` | `highlight_painter.PaintNonCssMarkers(kBackground)` |

### Rotation Application (Lines 496-503)

| Line | Condition | Action |
|------|-----------|--------|
| 496-498 | `rotation` exists | `context.ConcatCTM(*rotation)` |
| 499-502 | SVG state exists | Update shader transform |

### Emphasis Mark Setup (Lines 522-526)

| Line | Condition | Action |
|------|-----------|--------|
| 522 | `ShouldPaintEmphasisMark()` | Check mark style, ruby annotations |
| 523-525 | | `text_painter.SetEmphasisMark()` |

### Main Paint Switch (Lines 539-577)

#### Case: kNoHighlights / kFastSpellingGrammar (Lines 541-561)

| Line | Operation | Conditional |
|------|-----------|-------------|
| 546-548 | Check if shadows should paint first | `text_style.shadow && style.HasAppliedTextDecorations()` |
| 548-549 | `highlight_painter.PaintOriginatingShadow()` | If paint_shadows_first |
| 551 | `decoration_painter.Begin(kOriginating)` | Always |
| 552 | `decoration_painter.PaintExceptLineThrough()` | Always |
| 553-556 | `text_painter.Paint()` | With appropriate shadow mode |
| 557 | `decoration_painter.PaintOnlyLineThrough()` | Always |
| 558-559 | `highlight_painter.FastPaintSpellingGrammarDecorations()` | If kFastSpellingGrammar |

#### Case: kFastSelection (Lines 562-566)

| Line | Operation |
|------|-----------|
| 563-565 | `selection.PaintSuppressingTextProperWhereSelected()` |

#### Case: kOverlay (Lines 567-573)

| Line | Operation |
|------|-----------|
| 569 | `highlight_painter.PaintOriginatingShadow()` |
| 571-572 | `highlight_painter.PaintHighlightOverlays()` |

#### Case: kSelectionOnly (Lines 574-576)

No action here, selection painted later.

### Selection Background (Lines 579-585)

| Line | Condition | Action |
|------|-----------|--------|
| 580-581 | selection exists, paint_marker_backgrounds, kFastSelection | Paint selection background |

### Non-CSS Marker Foregrounds (Lines 587-591)

| Line | Condition | Action |
|------|-----------|--------|
| 589-590 | `paint_info.phase == PaintPhase::kForeground` | `highlight_painter.PaintNonCssMarkers(kForeground)` |

### Selection Foreground (Lines 593-616)

| Line | Case | Action |
|------|------|--------|
| 596-599 | kFastSelection | `selection.PaintSelectedText()` |
| 600-608 | kSelectionOnly | Begin decorations, paint except line-through, paint selected text, paint line-through |
| 609-611 | kOverlay | Already painted by PaintHighlightOverlays |

---

## HighlightPainter::ComputePaintCase()

**File:** `highlight_painter.cc:647-693`

### Decision Points

| Line | Condition | Return Value |
|------|-----------|--------------|
| 648-649 | `selection && ShouldPaintSelectedTextOnly()` | `kSelectionOnly` |
| 654-655 | `!target_.empty() || !search_.empty() || !custom_.empty()` | `kOverlay` |
| 658-668 | selection only, no spelling/grammar | Check decorations -> `kFastSelection` or `kOverlay` |
| 671-674 | spelling/grammar + selection | `kOverlay` |
| 679-687 | spelling/grammar, trivial styles | `kFastSpellingGrammar` or `kOverlay` |
| 692 | No highlights | `kNoHighlights` |

---

## TextPainter::Paint()

**File:** `text_painter.cc:361-415`

### Graphics Context Operations

| Line | Operation | Condition |
|------|-----------|-----------|
| 367-369 | Early exit | No shape result |
| 372-374 | Early exit | kShadowsOnly without shadow |
| 378-380 | `UpdateGraphicsContext()` | Sets colors, stroke, draw looper |
| 383-387 | `PaintSvgTextFragment()` | SVG text |
| 389-391 | `context.DrawText()` | Non-SVG text |
| 394-400 | `context.DrawEmphasisMarks()` | If emphasis mark set |
| 407-409 | `SetTextPainted()` | If non-whitespace |
| 413 | `NotifyTextPaint()` | If font should paint |

---

## TextPainter::SetEmphasisMark()

**File:** `text_painter.cc:498-523`

### Offset Calculation

| Line | Condition | Offset Calculation |
|------|-----------|-------------------|
| 507 | `LineLogicalSide::kOver` | `-ascent - descent` |
| 510-512 | Over + over annotation | Subtract annotation ascent |
| 515 | `LineLogicalSide::kUnder` | `+descent + ascent` |
| 518-520 | Under + under annotation | Add annotation descent |

---

## HighlightPainter::PaintHighlightOverlays()

**File:** `highlight_painter.cc:832-977`

### Layer Loop (Lines 847-921)

For each layer except originating:

| Line | Operation | Notes |
|------|-----------|-------|
| 854-856 | Skip selection layer if not painting backgrounds | |
| 861-890 | Paint backgrounds | Merge adjacent parts with same color |
| 894-921 | Paint text shadows | Merge adjacent parts |

### Part Loop (Lines 925-976)

For each part:

| Line | Operation | Notes |
|------|-----------|-------|
| 926 | Compute part_rect | LineRelativeWorldRect |
| 928 | PaintDecorationsExceptLineThrough | |
| 931-972 | Paint text proper | Clip, expand for glyphs, paint |
| 975 | PaintDecorationsOnlyLineThrough | |

---

## TextDecorationPainter::PaintUnderOrOverLineDecorations()

**File:** `text_decoration_painter.cc:143-204`

### Graphics Operations

| Line | Operation | Condition |
|------|-----------|-----------|
| 148-149 | `context.Scale()` | If rendering resource subtree |
| 156-200 | For each decoration | Loop through applied decorations |
| 161-170 | Paint spelling/grammar | If HasSpellingOrGrammarError |
| 174-186 | Paint underline | If HasUnderline, optionally clip for ink skip |
| 188-200 | Paint overline | If HasOverline, optionally clip for ink skip |

---

## DecorationLinePainter::Paint()

**File:** `decoration_line_painter.cc:310-349`

### Style Switch

| Line | Style | Method |
|------|-------|--------|
| 321-322 | kWavyStroke | `PaintWavyTextDecoration()` |
| 323-334 | kDottedStroke, kDashedStroke | `DrawLineAsStroke()` |
| 335-348 | kSolidStroke, kDoubleStroke | `DrawLineAsRect()` |

### Wavy Decoration (Lines 351-371)

| Line | Operation |
|------|-----------|
| 355 | Get cached wavy geometry |
| 358-359 | Compute paint rect and tile rect |
| 361-365 | Create shader from tile record |
| 367-370 | Translate and draw rect |

---

## Key GraphicsContext Methods Used

| Method | Location | Purpose |
|--------|----------|---------|
| `DrawText()` | `text_painter.cc:389` | Draw text glyphs |
| `DrawEmphasisMarks()` | `text_painter.cc:397` | Draw emphasis marks |
| `FillRect()` | `highlight_painter.cc:74` | Fill highlight backgrounds |
| `FillEllipse()` | `text_fragment_painter.cc:205` | Disc list marker |
| `StrokeEllipse()` | `text_fragment_painter.cc:208` | Circle list marker |
| `FillPath()` | `text_fragment_painter.cc:215` | Disclosure marker |
| `DrawLine()` | `decoration_line_painter.cc:69` | Dashed/dotted lines |
| `DrawRect()` | `decoration_line_painter.cc:79,89` | Solid lines, wavy tiles |
| `Clip()` | `highlight_painter.cc:1092` | Clip to part rect |
| `ClipOut()` | `text_painter.cc:480` | Clip out selection |
| `Scale()` | `text_fragment_painter.cc:457` | SVG scaling |
| `ConcatCTM()` | `text_fragment_painter.cc:498` | Rotation, SVG transforms |
| `SetFillColor()` | Multiple | Set fill for text/decorations |
| `SetStrokeColor()` | Multiple | Set stroke for decorations |
| `SetDrawLooper()` | `text_painter.cc:156` | Set up shadows |
| `BeginLayer()` | `text_shadow_painter.cc:73` | Begin shadow layer |
