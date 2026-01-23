CHROMIUM PAINT - QUICK TRACE REFERENCE
======================================

CORE ENTRY POINTS TO INSTRUMENT:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

TEXT PAINTING FLOW:
─────────────────

  TextFragmentPainter::Paint()
    │ [FILE: reference/text_fragment_painter.cc:221]
    │
    ├─→ ObjectPainter::RecordHitTestData()
    │   [CONDITIONAL: only if ShouldRecordSpecialHitTestData()]
    │
    ├─→ HighlightPainter::PaintNonCssMarkers(kBackground)
    │   [Composition/spell-check backgrounds]
    │
    │   ┌─────────────────────────────────────────────────────┐
    │   │ NOTE: The following flow depends on                 │
    │   │ HighlightPainter::Case (lines 540-577):             │
    │   │ - kNoHighlights / kFastSpellingGrammar → full flow  │
    │   │ - kFastSelection → different path                   │
    │   │ - kOverlay → PaintHighlightOverlays()               │
    │   │ - kSelectionOnly → selection-specific               │
    │   └─────────────────────────────────────────────────────┘
    │
    │   [For kNoHighlights / kFastSpellingGrammar case:]
    │
    ├─→ TextDecorationPainter::Begin()
    │
    ├─→ TextDecorationPainter::PaintExceptLineThrough()
    │   [Underline/Overline - painted BEFORE text]
    │
    ├─→ TextPainter::Paint()
    │   [FILE: reference/text_painter.cc:361]
    │   │ [Main text glyph rendering]
    │   │
    │   ├─→ graphics_context_.DrawText()
    │   │   *** KEY DRAW OPERATION ***
    │   │
    │   └─→ graphics_context_.DrawEmphasisMarks()
    │       [Ruby/emphasis marks - painted INSIDE TextPainter::Paint()]
    │       [Lines 394-401 in text_painter.cc]
    │
    ├─→ TextDecorationPainter::PaintOnlyLineThrough()
    │   [Line-through - painted AFTER text for proper overlap]
    │
    ├─→ HighlightPainter::PaintNonCssMarkers(kForeground)
    │   [Composition/spell-check foregrounds]
    │
    └─→ HighlightPainter::PaintSelectedText()
        [Selection highlighting - CONDITIONAL based on Case]

BLOCK BACKGROUND FLOW:
──────────────────────

  BlockPainter::Paint()
    [FILE: src/block_painter.cc:3]
    │ [Handles: visibility → color check → shadow build → shape selection]
    │
    ├─ Check: visibility == Visible? → if not, return empty
    │
    ├─ Check: has background_color? → if not, return empty
    │
    ├─ Call: BuildFlags() → convert colors/shadows to DrawFlags
    │
    ├─ Call: HasBorderRadius() → check for rounded corners
    │
    ├─ IF rounded corners:
    │   └─→ ops.DrawRRectOp()
    │       *** KEY DRAW OPERATION ***
    │
    └─ ELSE:
        └─→ ops.DrawRect()
            *** KEY DRAW OPERATION ***

BORDER FLOW:
────────────

  BoxBorderPainter::Paint()
    [FILE: reference/box_border_painter.cc:1439]
    │ [Complex multi-pass border rendering]
    │
    ├─→ ComputeBorderProperties()
    │   [Analyzes: is_uniform_width, is_uniform_color, is_uniform_style, is_rounded]
    │
    ├─ IF fast path (uniform solid border):
    │   │ [Single stroked rect/rrect]
    │   │
    │   └─→ context_.StrokeRect() or StrokeRRect()
    │       *** KEY DRAW OPERATION ***
    │
    └─ ELSE slow path (non-uniform borders):
        │ [Per-side rendering]
        │
        ├─→ For each visible side (top, right, bottom, left):
        │   │
        │   ├─→ PaintOneBorderSide()
        │   │   │
        │   │   ├─ IF thin border (< 10px):
        │   │   │   └─→ Filled thin rectangle
        │   │   │       *** KEY DRAW OPERATION ***
        │   │   │
        │   │   └─ IF thick border (>= 10px):
        │   │       └─→ DrawCurvedBoxSide() or stroked line
        │   │           *** KEY DRAW OPERATION ***
        │   │
        │   ├─ Handle corner miters with clipping
        │   │
        │   └─ Handle special styles:
        │       ├─ Double → 2 stroked rects
        │       ├─ Dotted → stroked lines with dots
        │       ├─ Dashed → stroked lines with dashes
        │       └─ Groove/Ridge → paired thin rects


═══════════════════════════════════════════════════════════════════════════════

KEY DRAW OPERATIONS EMITTED:
────────────────────────────

TEXT PAINTER EMITS:
  • DrawTextBlobOp(x, y, glyph_runs, font_metrics, paint_flags)
  • DrawLineOp(rect, color) - solid decoration lines
  • DrawStrokeLineOp(p1, p2, thickness, style, color) - dotted/dashed
  • DrawWavyLineOp(paint_rect, wave_pattern, color) - wavy
  • FillEllipseOp(rect, color) - disc bullet (●)
  • StrokeEllipseOp(rect, color, stroke_width) - circle (○)
  • FillPathOp(points, color) - disclosure triangle (▶)

BLOCK PAINTER EMITS:
  • DrawRectOp(rect, flags, transform_id, clip_id, effect_id)
  • DrawRRectOp(rect, radii, flags, transform_id, clip_id, effect_id)

BORDER PAINTER EMITS:
  • DrawRectOp(rect, flags) - filled rectangle
  • DrawLineOp(x1, y1, x2, y2, color) - stroked line
  • DrawRRectOp(rect, radii, flags) - rounded rectangle
  • DrawDRRectOp(outer_rrect, inner_rrect, flags) - double shapes


═══════════════════════════════════════════════════════════════════════════════

PAINT PHASE ROUTING:
────────────────────

Each painter is called during specific paint phases:

  kBlockBackground
    └─→ BlockPainter::Paint() [backgrounds]

  kFloat (if applicable)
    └─→ Float content painters

  kForeground
    └─→ TextFragmentPainter::Paint() [text content]
    └─→ Content painters

  kOutline
    └─→ Outline/border painters

  kSelectionDragImage
    └─→ Selection-specific painters


═══════════════════════════════════════════════════════════════════════════════

INSTRUMENTATION CHECKLIST:
──────────────────────────

For comprehensive paint tracing, log these function entries/exits:

LEVEL 1 - TOP ENTRY POINTS:
  [ ] TextFragmentPainter::Paint()
        - Log: text content, paint_phase, bounds
  [ ] BlockPainter::Paint()
        - Log: background_color, border_radii, shadow_count
  [ ] BoxBorderPainter::Paint()
        - Log: border_widths, border_styles, is_rounded

LEVEL 2 - DISPATCH POINTS:
  [ ] TextPainter::Paint()
        - Log: fragment_length, selection info
  [ ] TextDecorationPainter::PaintExceptLineThrough()
        - Log: decoration_types (underline/overline)
  [ ] TextDecorationPainter::PaintOnlyLineThrough()
        - Log: linethrough_details
  [ ] HighlightPainter::PaintNonCssMarkers()
        - Log: marker_type (composition/spell-check)

LEVEL 3 - DECISION POINTS:
  [ ] BoxBorderPainter::ComputeBorderProperties()
        - Log: is_uniform_style, is_uniform_width, first_visible_edge
  [ ] TextPainter::SetEmphasisMark()
        - Log: mark, position (kOver/kUnder)
  [ ] TextFragmentPainter::PaintSymbol()
        - Log: symbol_type (disc/circle/square/disclosure)

LEVEL 4 - DRAW OPERATIONS:
  [ ] graphics_context_.DrawText()
        - Log: text_origin, visual_rect, font_size
  [ ] graphics_context_.DrawTextBlob()
        - Log: glyph_count, run_count
  [ ] graphics_context_.FillEllipse()
        - Log: ellipse_rect
  [ ] graphics_context_.FillPath()
        - Log: path_point_count
  [ ] graphics_context_.StrokeRect()
        - Log: rect, stroke_width, stroke_color


═══════════════════════════════════════════════════════════════════════════════

FILE LOCATIONS:
───────────────

Reference Implementation (Chromium source):
  /Users/simon/OTH/chromium-dom-tracing/04_paint/text_painter/reference/text_fragment_painter.cc
  /Users/simon/OTH/chromium-dom-tracing/04_paint/text_painter/reference/text_painter.cc
  /Users/simon/OTH/chromium-dom-tracing/04_paint/text_painter/reference/text_decoration_painter.cc
  /Users/simon/OTH/chromium-dom-tracing/04_paint/border_painter/reference/box_border_painter.cc
  /Users/simon/OTH/chromium-dom-tracing/04_paint/block_painter/src/block_painter.cc

Headers:
  /Users/simon/OTH/chromium-dom-tracing/04_paint/text_painter/reference/text_fragment_painter.h
  /Users/simon/OTH/chromium-dom-tracing/04_paint/text_painter/reference/text_painter.h
  /Users/simon/OTH/chromium-dom-tracing/04_paint/text_painter/reference/text_decoration_painter.h
  /Users/simon/OTH/chromium-dom-tracing/04_paint/border_painter/reference/box_border_painter.h
  /Users/simon/OTH/chromium-dom-tracing/04_paint/block_painter/src/block_painter.h

Draw Operations Definitions:
  /Users/simon/OTH/chromium-dom-tracing/04_paint/text_painter/src/draw_commands.h
  /Users/simon/OTH/chromium-dom-tracing/04_paint/block_painter/src/draw_commands.h
  /Users/simon/OTH/chromium-dom-tracing/04_paint/border_painter/src/draw_commands.h

═══════════════════════════════════════════════════════════════════════════════
