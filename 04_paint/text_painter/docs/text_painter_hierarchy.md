# Text Painter Class Hierarchy

## Overview

The text painting system in Blink is triggered from `PaintLayerPainter` and flows through several specialized painter classes. This document describes the class hierarchy and relationships.

## Entry Point: PaintLayerPainter

**File:** `paint_layer_painter.cc`

### Paint Phase Triggers

Text painting is triggered during the `PaintPhase::kForeground` phase:

1. `PaintLayerPainter::Paint()` is called on each self-painting layer
2. For foreground painting, it calls `PaintForegroundPhases()`
3. `PaintForegroundPhases()` calls `PaintWithPhase(PaintPhase::kForeground, ...)`
4. This eventually reaches `BoxFragmentPainter` which paints text items

### Context Passed to Painters

- `GraphicsContext&` - The drawing context for all paint operations
- `PaintInfo` - Contains:
  - `phase` - Current paint phase (kForeground, kSelectionDragImage, kTextClip, etc.)
  - `context` - The GraphicsContext reference
  - Various flags and cull rect information

## Class Hierarchy

```
TextFragmentPainter (main entry point for text)
    |
    +-- TextPainter (text drawing operations)
    |       |
    |       +-- TextCombinePainter (vertical text combining)
    |
    +-- TextDecorationPainter (underlines, overlines, line-through)
    |       |
    |       +-- DecorationLinePainter (actual line drawing)
    |
    +-- HighlightPainter (selection, spelling/grammar, custom highlights)
            |
            +-- SelectionPaintState (selection state management)
            +-- HighlightOverlay (overlay computation)
            +-- StyleableMarkerPainter (composition/suggestion markers)
```

## Key Classes

### TextFragmentPainter
- **File:** `text_fragment_painter.cc`, `text_fragment_painter.h`
- **Role:** Main entry point for painting text fragments
- **Responsibilities:**
  - Coordinates text, decoration, and highlight painting
  - Handles writing mode rotations
  - Manages symbol markers (list bullets)

### TextPainter
- **File:** `text_painter.cc`, `text_painter.h`
- **Role:** Core text drawing operations
- **Responsibilities:**
  - Drawing text glyphs via `GraphicsContext::DrawText()`
  - Drawing emphasis marks via `GraphicsContext::DrawEmphasisMarks()`
  - SVG text painting with fill/stroke
  - Text shadow handling via DrawLooper

### TextCombinePainter
- **File:** `text_combine_painter.cc`, `text_combine_painter.h`
- **Role:** Handles `text-combine-upright` CSS property
- **Inherits:** TextPainter
- **Responsibilities:**
  - Paints combined text in vertical writing modes
  - Handles emphasis marks for combined text

### TextDecorationPainter
- **File:** `text_decoration_painter.cc`, `text_decoration_painter.h`
- **Role:** Text decoration rendering
- **Responsibilities:**
  - Underlines, overlines, line-through
  - Spelling/grammar error decorations
  - Coordinates decoration painting order

### DecorationLinePainter
- **File:** `decoration_line_painter.cc`, `decoration_line_painter.h`
- **Role:** Low-level decoration line drawing
- **Responsibilities:**
  - Solid, dashed, dotted, double, wavy lines
  - Wavy pattern generation for spelling/grammar
  - SVG decoration support

### HighlightPainter
- **File:** `highlight_painter.cc`, `highlight_painter.h`
- **Role:** CSS highlight pseudo-element painting
- **Responsibilities:**
  - Selection (`::selection`)
  - Spelling errors (`::spelling-error`)
  - Grammar errors (`::grammar-error`)
  - Search highlights (`::search-text`)
  - Custom highlights (`::highlight(name)`)
  - Target text (`::target-text`)

### HighlightOverlay
- **File:** `highlight_overlay.cc`, `highlight_overlay.h`
- **Role:** Compute highlight layers and parts
- **Responsibilities:**
  - Layer ordering (originating < custom < target < search < spelling < grammar < selection)
  - Edge computation for overlapping highlights
  - Part computation for painting

### StyleableMarkerPainter
- **File:** `styleable_marker_painter.cc`, `styleable_marker_painter.h`
- **Role:** IME composition and suggestion markers
- **Responsibilities:**
  - Composition underlines (wavy, dotted, etc.)
  - Suggestion markers
  - Platform-specific marker styles (macOS vs others)

### TextShadowPainter
- **File:** `text_shadow_painter.cc`, `text_shadow_painter.h`
- **Role:** Text shadow filter creation
- **Responsibilities:**
  - Creates PaintFilter for text shadows
  - Handles multiple shadow layers

## Paint Order

Within a text fragment, painting occurs in this order:

1. **Non-CSS marker backgrounds** (composition, suggestion)
2. **Writing mode rotation** (if needed)
3. **Selection background expansion** (for bounds recording)
4. **Text and decorations** (based on HighlightPainter::Case):
   - Shadows first (if decorations present)
   - Decorations except line-through
   - Text proper
   - Line-through decorations
5. **Selection background** (kFastSelection case)
6. **Non-CSS marker foregrounds**
7. **Selection foreground**
