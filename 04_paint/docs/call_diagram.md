CHROMIUM PAINT CALL HIERARCHY - VISUAL DIAGRAM
===============================================

                        ┌─────────────────────────────┐
                        │  PaintLayerPainter::Paint() │
                        │  (Main entry point)         │
                        └────────────┬────────────────┘
                                     │
                 ┌───────────────────┼───────────────────┐
                 │                   │                   │
                 ▼                   ▼                   ▼
         ┌──────────────┐   ┌──────────────┐   ┌──────────────┐
         │Paint Phase   │   │Paint Phase   │   │Paint Phase   │
         │kBackground   │   │kForeground   │   │kOutline      │
         └──────┬───────┘   └──────┬───────┘   └──────┬───────┘
                │                   │                   │
                ▼                   ▼                   ▼
        ┌─────────────────┐ ┌────────────────────┐ ┌──────────────┐
        │BlockPainter     │ │TextFragmentPainter │ │BoxBorderPainter
        │::Paint()        │ │::Paint()           │ │::Paint()
        └────┬────────────┘ └────┬───────────────┘ └──────┬───────┘
             │                   │                        │
    ┌────────┴────────┐          │                ┌───────┴────────┐
    │                 │          │                │                │
    ▼                 ▼          ▼                ▼                ▼
┌─────────┐    ┌────────────┐   │          ┌─────────────┐  ┌─────────────┐
│Check    │    │BuildFlags()│   │          │Analyze      │  │Special      │
│Visibility   │ │            │   │          │Properties  │  │Styles       │
└─────────┘    │ ├─ color    │   │          │            │  │             │
   │ return    │ ├─ shadow   │   │          │ is_uniform │  │ Double      │
   │           │ └─ style    │   │          │ width      │  │ Dotted      │
   │           └────────────┘   │          │ is_uniform │  │ Dashed      │
   │                 │          │          │ style      │  │ Groove/Ridge
   ▼                 │          │          │ is_rounded │  └─────────────┘
┌─────────┐    ┌────┴──────┐   │          └─────┬───────┘         │
│Check    │    │HasBorder- │   │                │                 │
│Background   │ │Radius()   │   │        ┌───────┴────────┐        │
│Color?      │ └──┬───┬────┘   │        │                │        │
└──┬─┬──────┘    │   │        │        ▼                ▼        │
   │ │ return    │   │        │    ┌──────────┐   ┌────────────┐ │
   │ │           │   │        │    │Fast Path │   │Slow Path   │ │
   ▼ ▼           │   │        │    │          │   │            │ │
 empty      ┌────┴───┴────┐   │    │uniform   │   │for each    │ │
           │              │   │    │→ stroke  │   │side:       │ │
        ┌──▼──┐      ┌────▼──▐   │    │  single  │   │ ├─ thin     │ │
        │Yes  │      │No     │   │    │  rect    │   │ │  → filled  │ │
        │     │      │       │   │    │  or rrect│   │ │  rect      │ │
        ▼     ▼      ▼       ▼   │    └──────────┘   │ ├─ thick    │ │
    ┌──────┐ ┌────────────┐   │        │             │ │  → stroke  │ │
    │Draw  │ │DrawRRect   │   │        │             │ │  line      │ │
    │Rect  │ │Op()        │   │        │             │ ├─ corners  │ │
    │Op()  │ │            │   │        │             │ │  mitering  │ │
    └──────┘ └────────────┘   │        │             │ └─ special  │ │
                              │        │             │   styles    │ │
                              │        ▼             └─────┬───────┘ │
                              │    ┌──────────┐           │         │
                              │    │Emit Draw │           │         │
                              │    │RectOp or │           │         │
                              │    │DrawRRect │           │         │
                              │    │Op        │           │         │
                              │    └──────────┘           │         │
                              │                           │         │
         ┌────────────────────┤                    ┌──────▼─────────┘
         │                    │                    │
         │                    │                    │
         ▼                    ▼                    ▼
    ┌────────────┐   ┌─────────────────┐    ┌──────────────┐
    │TextMarker  │   │TextPainter      │    │Draw Border   │
    │(bullets)   │   │::Paint()        │    │Operations    │
    └────┬───────┘   └────┬────────────┘    └──────────────┘
         │                │
    ┌────▼─────────┐      ▼
    │ Symbol Type? │ ┌─────────────────────┐
    │              │ │Text Decoration Flow │
    │• disc →      │ │                     │
    │  FillEllipse │ │ 1. PaintNonCssMarkers
    │• circle →    │ │    (background)
    │  StrokeEllipse
    │• square →    │ │ 2. PaintExceptLT
    │  FillRect    │ │    (underline/
    │• disclosure →│ │     overline)
    │  FillPath    │ │
    └──────────────┘ │ 3. TextPainter::Paint()
                     │    ├─ graphics_context_
                     │    │    .DrawText()
                     │    │    *** KEY OP ***
                     │    │
                     │    └─ DrawEmphasisMarks()
                     │       (ruby - INSIDE Paint)
                     │
                     │ 4. PaintOnlyLineThrough
                     │    (line-through)
                     │
                     │ 5. PaintNonCssMarkers
                     │    (foreground)
                     │
                     │ 6. PaintSelectedText()
                     │    (conditional)
                     │
                     │ NOTE: Steps 1-6 are for
                     │ kNoHighlights case. Other
                     │ HighlightPainter::Case
                     │ values have different flows.
                     │
                     └─────────────────────┘


═══════════════════════════════════════════════════════════════════════════════

PAINT OPERATION TYPES BY LAYER:
════════════════════════════════

TEXT LAYER DRAW OPS:
────────────────────
    DrawTextBlobOp
        ├─ x, y (text origin)
        ├─ glyph_runs[] (from HarfBuzz shaping)
        ├─ font_metrics
        └─ paint_flags (color, style)

DECORATION OPS:
────────────────
    DrawLineOp (solid/double)
        ├─ rect (line bounds)
        ├─ color
        └─ style (solid/double)

    DrawStrokeLineOp (dotted/dashed)
        ├─ p1, p2 (endpoints)
        ├─ thickness
        ├─ style (dotted/dashed)
        └─ color

    DrawWavyLineOp (wavy)
        ├─ paint_rect
        ├─ tile_path (bezier)
        └─ wave_definition

SYMBOL MARKER OPS:
───────────────────
    FillEllipseOp (●)
        ├─ rect (bounding box)
        └─ color

    StrokeEllipseOp (○)
        ├─ rect
        ├─ stroke_width
        └─ color

    FillPathOp (▶ ▼)
        ├─ points[] (path polygon)
        └─ color

BLOCK BACKGROUND OPS:
──────────────────────
    DrawRectOp
        ├─ rect [left, top, right, bottom]
        ├─ flags (color, shadows)
        └─ state_ids (transform, clip, effect)

    DrawRRectOp
        ├─ rect
        ├─ radii [tl_x, tl_y, tr_x, tr_y, br_x, br_y, bl_x, bl_y]
        ├─ flags
        └─ state_ids

BORDER OPS:
────────────
    DrawRectOp (simple borders)
    DrawRRectOp (rounded borders)
    DrawLineOp (per-side lines)
    DrawDRRectOp (double borders)


═══════════════════════════════════════════════════════════════════════════════

FLOW CHART: HOW A TEXT ELEMENT GETS PAINTED
═════════════════════════════════════════════

    Element: <p>Hello <u>world</u></p>

    START: RootLayer Paint
    │
    └─→ PaintLayerPainter::Paint()
        │ paint_phase = kForeground
        │
        └─→ For each text fragment:
            │
            ├─→ TextFragmentPainter::Paint()
            │   │
            │   ├─ Check visibility? YES
            │   ├─ Check text length? YES
            │   ├─ Check bounds? YES
            │   │
            │   ├─→ TextDecorationPainter::PaintExceptLineThrough()
            │   │   │
            │   │   └─→ TextPainter::PaintDecorationLine() [underline]
            │   │       │
            │   │       └─→ DrawLineOp(underline_rect, black, solid)
            │   │
            │   ├─→ TextPainter::Paint()
            │   │   │
            │   │   ├─ Build paint flags (fill=black)
            │   │   ├─ Update graphics context
            │   │   │
            │   │   ├─→ graphics_context_.DrawText()
            │   │   │   │
            │   │   │   └─→ DrawTextBlobOp(
            │   │   │           x=10, y=15,
            │   │   │           runs=[glyph_run],
            │   │   │           font=Arial,
            │   │   │           flags=black_fill)
            │   │   │
            │   │   └─→ graphics_context_.DrawEmphasisMarks()
            │   │       [If emphasis_mark_ is set - painted INSIDE Paint()]
            │   │
            │   ├─→ TextDecorationPainter::PaintOnlyLineThrough()
            │   │   │
            │   │   └─→ (no line-through in this case)
            │   │
            │   ├─→ HighlightPainter::PaintNonCssMarkers(kForeground)
            │   │
            │   └─→ HighlightPainter::PaintSelectedText()
            │       │
            │       └─→ (if text is selected, conditional on Case)
            │
            └─→ PaintOpList returned
                │
                └─→ Serialized/executed by graphics backend


═══════════════════════════════════════════════════════════════════════════════

TRACE LOGGING EXAMPLE:
══════════════════════

To trace a simple div with background and text:

    [CALL] BlockPainter::Paint()
        input: geometry={100,100,200,150}
               background_color=rgb(0,0,255)
               border_radii=[10,10,10,10,10,10,10,10]
    [RET]  [Op: DrawRRectOp(rect=[100,100,300,250], radii=[...], color=blue)]

    [CALL] TextFragmentPainter::Paint()
        text: "Click me"
        paint_info.phase: kForeground
        bounds: [110,110,180,140]
    [CALL]   TextPainter::Paint()
        fragment_paint_info.text: "Click me" (len=8)
        text_style.fill_color: black
        text_style.shadow: none
    [EMIT]    DrawTextBlobOp(x=110, y=125, glyphs=[...], font=Arial)
    [RET]   OK
    [RET]  OK

    Output PaintOpList:
    [
        DrawRRectOp(...),
        DrawTextBlobOp(...)
    ]

═══════════════════════════════════════════════════════════════════════════════
