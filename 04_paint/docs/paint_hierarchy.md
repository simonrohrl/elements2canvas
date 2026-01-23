CHROMIUM PAINT CODE HIERARCHY ANALYSIS
=====================================

Based on the exploration of the chromium-dom-tracing project's paint implementations,
this report details the paint call hierarchy, key entry points, and draw operations.

PROJECT STRUCTURE:
- 04_paint/text_painter/      - Text and decoration rendering
- 04_paint/border_painter/    - Border rendering
- 04_paint/block_painter/     - Background and shadow rendering
- reference/ directories      - Original Chromium source code

=============================================================================
SECTION 1: PAINT CALL HIERARCHY
=============================================================================

TOP LEVEL: PaintLayerPainter::Paint()
├── PaintPhase detection (kBackground, kSelfPaintingInlineBoxes, kForeground, etc.)
└── Delegates to specific painters based on content type

ENTRY POINTS BY CONTENT TYPE:

1. BLOCK CONTENT (BoxFragmentPainter or equivalent)
   ├─ Background Layer
   │  └─ BlockPainter::Paint()
   │     └─ Emits: DrawRectOp or DrawRRectOp
   │
   ├─ Border Layer
   │  └─ BoxBorderPainter::Paint()
   │     └─ Analyzes and emits border operations:
   │        ├─ Fast path: Single stroked rect/rrect
   │        └─ Slow path: Per-side rectangles/lines
   │
   └─ Content Layer
      └─ Child painters (text, images, etc.)

2. TEXT CONTENT (TextFragmentPainter)
   ├─ Symbol markers (bullets, disclosure triangles) - early return if present
   ├─ Text decorations (underline/overline - before text)
   ├─ Text glyphs + Emphasis marks
   │  └─ TextPainter::Paint()
   │     ├─ Emits: DrawTextBlobOp (glyphs)
   │     └─ Emits: DrawEmphasisMarks (ruby - inside Paint())
   └─ Line-through decoration (after text)

3. INLINE CONTENT
   └─ InlineCursor iteration
      └─ For each fragment, call appropriate painter

=============================================================================
SECTION 2: PAINT ORDER IN DETAIL
=============================================================================

CHROMIUM'S CSS PAINT ORDER (from TextFragmentPainter::Paint):

NOTE: The exact flow depends on HighlightPainter::Case (kNoHighlights,
kFastSpellingGrammar, kFastSelection, kOverlay, kSelectionOnly).
Below is the kNoHighlights / kFastSpellingGrammar case:

1. Visibility and flow control checks
2. Symbol marker rendering (if bullet/disclosure) - early return
3. SVG transform and writing mode handling
4. Text origin computation (baseline positioning)
5. PaintNonCssMarkers(kBackground) - composition/spell-check backgrounds
6. UNDERLINE/OVERLINE decorations (kUnderline, kOverline)
   → Called before text for correct layering
7. TEXT GLYPHS + EMPHASIS MARKS
   → DrawTextBlobOp with glyph runs from HarfBuzz shaping
   → DrawEmphasisMarks() called INSIDE TextPainter::Paint() (lines 394-401)
8. LINE-THROUGH decoration
   → Called after text for correct overlap
9. PaintNonCssMarkers(kForeground) - composition/spell-check foregrounds
10. Selection highlighting (conditional)

KEY PAINT STYLES:
- GraphicsContext state saver for nested operations
- DrawingRecorder for display item caching
- HighlightPainter for selection/spelling/grammar highlights

=============================================================================
SECTION 3: FUNCTION CALL HIERARCHY DIAGRAM
=============================================================================

Paint Entry Points:

    TextFragmentPainter::Paint()
    │ [For kNoHighlights / kFastSpellingGrammar case - other cases differ]
    │
    ├─ Calls: ObjectPainter::RecordHitTestData()
    │  [CONDITIONAL: only if ShouldRecordSpecialHitTestData()]
    ├─ Calls: HighlightPainter::PaintNonCssMarkers(kBackground)
    ├─ Calls: TextDecorationPainter::Begin()
    ├─ Calls: TextDecorationPainter::PaintExceptLineThrough()
    ├─ Calls: TextPainter::Paint()
    │  ├─ Updates graphics context for fill/stroke
    │  ├─ Calls: graphics_context_.DrawText()
    │  └─ Calls: graphics_context_.DrawEmphasisMarks()
    │     [INSIDE Paint(), lines 394-401 in text_painter.cc]
    ├─ Calls: TextDecorationPainter::PaintOnlyLineThrough()
    ├─ Calls: HighlightPainter::PaintNonCssMarkers(kForeground)
    └─ Calls: HighlightPainter::PaintSelectedText()
       [CONDITIONAL: depends on HighlightPainter::Case]

    BoxBorderPainter::Paint()
    ├─ ComputeBorderProperties() - analyzes uniformity
    ├─ Determines fast vs slow path
    ├─ Fast path: Single stroked shape
    │  └─ context_.StrokeRect() or StrokeRRect()
    └─ Slow path: For each side
       ├─ PaintOneBorderSide()
       ├─ DrawCurvedBoxSide() or straight line
       └─ Emits: DrawLineOp or DrawRectOp

    BlockPainter::Paint()
    ├─ HasBorderRadius() - check rounded corners
    ├─ BuildFlags() - prepare color and shadows
    └─ If rounded: DrawRRectOp else: DrawRectOp

=============================================================================
SECTION 4: KEY TRACE POINTS
=============================================================================

FOR PAINT CALL HIERARCHY TRACING:

Level 1 (Entry):
  ✓ TextFragmentPainter::Paint()
  ✓ BoxBorderPainter::Paint()
  ✓ BlockPainter::Paint()

Level 2 (Dispatch):
  ✓ TextPainter::Paint()
  ✓ TextDecorationPainter::PaintExceptLineThrough()
  ✓ TextDecorationPainter::PaintOnlyLineThrough()
  ✓ HighlightPainter::PaintNonCssMarkers()

Level 3 (Graphics Emission):
  ✓ graphics_context_.DrawText()
  ✓ graphics_context_.DrawEmphasisMarks()
  ✓ graphics_context_.StrokeRect()
  ✓ graphics_context_.FillRect()
  ✓ graphics_context_.StrokeLine()

SUGGESTED TRACE LOGGING POINTS:

1. Entry Points (log input parameters):
   - void TextFragmentPainter::Paint(const PaintInfo& paint_info, ...)
   - void TextPainter::Paint(const TextFragmentPaintInfo& fragment_paint_info, ...)
   - void BoxBorderPainter::Paint() const
   - void BlockPainter::Paint(const BlockPaintInput& input)

2. Decision Points (log branching decisions):
   - HasBorderRadius() - determines shape type
   - ShouldPaintEmphasisMark() - controls emphasis mark rendering
   - HighlightPainter::Case selection - determines highlighting strategy

3. Draw Operations (log graphics calls with parameters):
   - graphics_context_.DrawText(font, fragment_paint_info, ...)
   - graphics_context_.FillEllipse() / StrokeEllipse() - markers
   - graphics_context_.FillPath() - disclosure triangles
   - graphics_context_.DrawRect() / DrawRRect() - blocks and borders

=============================================================================
SECTION 5: PAINT OPERATIONS REFERENCE
=============================================================================

TEXT PAINTER OPERATIONS (from draw_commands.h):

  DrawTextBlobOp
    - Renders shaped glyph runs
    - Contains HarfBuzz positioning and glyph IDs
    - Params: position (x, y), bounds, font metrics, paint flags

  DrawLineOp
    - Solid or double decoration line (underline/overline)
    - Drawn as filled rectangle

  DrawStrokeLineOp
    - Dotted/dashed decoration (spelling/grammar)
    - Drawn as stroked line with pattern

  DrawWavyLineOp
    - Wavy decoration (spelling/grammar errors)
    - Tiled bezier pattern

  FillEllipseOp / StrokeEllipseOp
    - For disc (●) and circle (○) list markers

  FillPathOp
    - For disclosure triangle markers (▶ ▼)

BLOCK PAINTER OPERATIONS:

  DrawRectOp
    - Simple rectangle fill (no border radius)
    - Includes shadows via draw flags

  DrawRRectOp
    - Rounded rectangle fill
    - Includes border radii and shadows

BORDER PAINTER OPERATIONS:

  DrawLineOp
    - Stroked line segment (for per-side borders)

  DrawRectOp / DrawRRectOp
    - Filled or stroked rectangles for borders

  DrawDRRectOp
    - Double rounded rect (outer - inner) for special effects

=============================================================================
SECTION 6: CONTENT TYPE ROUTING
=============================================================================

How Chromium routes different content types:

TEXT (inline elements):
  - TextFragmentPainter (fragment-based rendering)
  - Routes to TextPainter for glyph rendering
  - Handles decoration via TextDecorationPainter

BOXES (block elements):
  - BoxFragmentPainter
  - Paints in order: background → border → content
  - Background: BlockPainter
  - Border: BoxBorderPainter
  - Content: child painters (text, images, etc.)

BLOCKS WITH CONTENT:
  - Paint layers establish stacking contexts
  - PaintLayerPainter dispatches based on paint phase
  - Each phase paints specific content type

SELECTION/HIGHLIGHTS:
  - HighlightPainter handles CSS highlights
  - Paints over text with semi-transparent backgrounds
  - Handles ::selection, ::spelling-error, ::grammar-error

=============================================================================
SECTION 7: KEY CLASSES AND FUNCTIONS TO TRACE
=============================================================================

TextFragmentPainter (reference/text_fragment_painter.h)
  - Main entry for inline text rendering
  - Paint(const PaintInfo&, const PhysicalOffset&)
  - PaintSymbol() - renders list markers

TextPainter (reference/text_painter.h)
  - Lower-level text glyph rendering
  - Paint(const TextFragmentPaintInfo&, const TextPaintStyle&, ...)
  - PaintSelectedText() - two-pass selection rendering
  - SetEmphasisMark() - configure emphasis (ruby) marks

TextDecorationPainter (reference/text_decoration_painter.h)
  - Decorations (underline, overline, line-through)
  - Begin() / PaintExceptLineThrough() / PaintOnlyLineThrough()
  - Manages decoration order and clipping

HighlightPainter (reference/highlight_painter.h - not fully shown but referenced)
  - Selection and spell-check highlighting
  - PaintNonCssMarkers() - composition/spell-check marks
  - PaintHighlightOverlays() - CSS highlight pseudos

BoxBorderPainter (reference/box_border_painter.h)
  - CSS border rendering
  - Paint() - main entry
  - ComputeBorderProperties() - uniformity analysis
  - PaintSide() - individual border side
  - Multiple internal methods for corner handling

BoxPainter (implied, not fully shown)
  - Would coordinate block painting
  - Likely calls BlockPainter for background
  - Calls BoxBorderPainter for border
  - Dispatches to content painters

GraphicsContext (graphics backend)
  - DrawText() - emit glyph rendering
  - DrawEmphasisMarks() - emit emphasis marks
  - FillEllipse() / StrokeEllipse() - shapes
  - FillPath() - arbitrary paths
  - StrokeRect() / FillRect() - basic shapes

=============================================================================
SECTION 8: ARCHITECTURAL INSIGHTS
=============================================================================

1. PAINTING PHASES:
   Chromium uses paint phases to control what gets painted:
   - kBlockBackground - backgrounds only
   - kSelfPaintingInlineBoxes - inline content
   - kForeground - main content
   - kOutline - outline borders
   - kSelectionDragImage - for drag preview
   - etc.

2. STATE MANAGEMENT:
   - GraphicsContextStateSaver manages graphics state
   - DrawingRecorder caches display items
   - SelectionBoundsRecorder tracks selection regions

3. OPTIMIZATION STRATEGIES:
   - Border painter analyzes uniformity for fast path
   - Text painter uses shape result caching
   - Display item caching prevents redundant rendering

4. COORDINATE SYSTEMS:
   - Physical coordinates (absolute layout position)
   - Logical coordinates (writing mode aware)
   - Line-relative coordinates (baseline relative)
   - Writing mode rotation handled via AffineTransform

5. INTEGRATION POINTS:
   - Property tree IDs (transform_id, clip_id, effect_id)
   - DOM node IDs for selection/tracing
   - Auto dark mode handling

=============================================================================
