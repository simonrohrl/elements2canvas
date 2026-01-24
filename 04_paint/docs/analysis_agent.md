ANALYSIS AGENT
==============

Traces call structure from paint_layer_painter.cc through each painter group
and produces detailed documentation.

## Goal

**One Analysis Agent per painter directory.** For each painter directory
(except replaced_painter which is skipped), trace the exact call hierarchy
and produce three documentation files:
- `{group}_hierarchy.md` - detailed hierarchy analysis with trace points
- `{group}_call_diagram.md` - visual ASCII diagrams of call flow
- `{group}_trace_reference.md` - quick reference for instrumentation

## Strategy

### Step 1: Start at paint_layer_painter.cc

Read `./chromium/src/third_party/blink/renderer/core/paint/paint_layer_painter.cc`

- Identify how it dispatches to different painters
- Map paint phases to painter calls
- Document the top-level entry flow

### Step 2: For each painter group, trace the call structure

For a painter group (e.g., text_painter):

1. Read the main entry point file (e.g., text_fragment_painter.cc)
2. Trace every function call in the Paint() method
3. For each called function:
   - Note the file and line number
   - Note parameters passed
   - Note any conditional logic (if/switch)
   - Recursively trace if it calls other painters
4. Identify draw operations (graphics_context_.Draw*, Fill*, Stroke*)
5. Document decision points that affect paint flow

### Step 3: Copy missing reference files

While tracing, if you discover a source file that:
- Clearly belongs to this painter group (not a utility class)
- Is not already in `04_paint/{painter_group}/reference/`

Copy it from Chromium source to the reference directory:
```
./chromium/src/third_party/blink/renderer/core/paint/{file}.cc
  → 04_paint/{painter_group}/reference/{file}.cc
```

Examples of files to copy:
- `decoration_line_painter.cc` → text_painter group (specific to text decoration)
- `inline_box_fragment_painter.cc` → text_painter group (if text-specific)

Do NOT copy:
- Utility classes used by multiple painters (e.g., `paint_info.cc`)
- Graphics backend classes (e.g., `graphics_context.cc`)

### Step 4: Produce documentation for each painter group

Create three files in `04_paint/{painter_group}/docs/`:

#### {painter_group}_hierarchy.md
Similar to paint_hierarchy.md:
- Paint call hierarchy tree
- Entry points by content type
- Paint order in detail
- Key trace points
- Paint operations reference

#### {painter_group}_call_diagram.md
Similar to call_diagram.md:
- ASCII visual diagrams of call flow
- Paint operation types
- Flow chart examples
- Trace logging examples

#### {painter_group}_trace_reference.md
Similar to trace_reference.md:
- Core entry points to instrument
- Paint flow with file:line references
- Key draw operations emitted
- Instrumentation checklist

## Output Format

### {painter_group}_hierarchy.md

Document structure:

```
{PAINTER_GROUP} PAINT CODE HIERARCHY ANALYSIS
=============================================

SECTION 1: PAINT CALL HIERARCHY
===============================

TOP LEVEL: {MainPainter}::Paint()
├── {What triggers this painter}
└── Delegates to {sub-painters} based on {condition}

ENTRY POINTS BY CONTENT TYPE:

1. {CONTENT_TYPE_1}
   ├─ {SubPainter1}::Paint()
   │  └─ Emits: {DrawOp1}
   │
   └─ {SubPainter2}::Paint()
      └─ Emits: {DrawOp2}

2. {CONTENT_TYPE_2}
   └─ ...

SECTION 2: PAINT ORDER IN DETAIL
================================

{PAINTER_GROUP}'S PAINT ORDER (from {MainPainter}::Paint):

NOTE: {Any conditional logic that affects the flow}

1. {First step} - {what happens}
2. {Second step}
   → {detail about this step}
3. {Third step}
   → {detail}
...

KEY PAINT STYLES:
- {Style/pattern 1}
- {Style/pattern 2}

SECTION 3: FUNCTION CALL HIERARCHY DIAGRAM
==========================================

    {MainPainter}::Paint()
    │ [FILE: reference/{file}.cc:{line}]
    │ [{Note about conditional paths if any}]
    │
    ├─ Calls: {Function1}()
    │  [{CONDITIONAL: condition if applicable}]
    │
    ├─ Calls: {Function2}()
    │  ├─ {What it does}
    │  ├─ Calls: {NestedFunction}()
    │  └─ Calls: graphics_context_.{DrawOp}()
    │     [INSIDE {Function2}(), lines {X-Y} in {file}.cc]
    │
    ├─ Calls: {Function3}()
    │
    └─ Calls: {Function4}()
       [{CONDITIONAL: depends on {condition}}]

SECTION 4: KEY TRACE POINTS
===========================

FOR PAINT CALL HIERARCHY TRACING:

Level 1 (Entry):
  ✓ {MainPainter}::Paint()
  ✓ {OtherEntryPainter}::Paint()

Level 2 (Dispatch):
  ✓ {SubPainter1}::Paint()
  ✓ {SubPainter2}::{Method}()

Level 3 (Graphics Emission):
  ✓ graphics_context_.{DrawOp1}()
  ✓ graphics_context_.{DrawOp2}()

SUGGESTED TRACE LOGGING POINTS:

1. Entry Points (log input parameters):
   - void {MainPainter}::Paint(const {ParamType}& {param}, ...)
   - void {SubPainter}::Paint(const {ParamType}& {param}, ...)

2. Decision Points (log branching decisions):
   - {DecisionFunction}() - determines {what}
   - {AnotherDecision}() - controls {what}

3. Draw Operations (log graphics calls with parameters):
   - graphics_context_.{DrawOp}({params})
   - graphics_context_.{OtherOp}({params})

SECTION 5: PAINT OPERATIONS REFERENCE
=====================================

{PAINTER_GROUP} OPERATIONS:

  {DrawOp1}
    - {What it renders}
    - Params: {param1}, {param2}, ...

  {DrawOp2}
    - {What it renders}
    - Params: {param1}, {param2}, ...

SECTION 6: KEY CLASSES AND FUNCTIONS
====================================

{MainPainter} (reference/{file}.h)
  - {Description}
  - Paint({params}) - {what it does}
  - {OtherMethod}() - {what it does}

{SubPainter} (reference/{file}.h)
  - {Description}
  - {Method}() - {what it does}

GraphicsContext (graphics backend)
  - {DrawOp}() - {what it emits}
  - {OtherOp}() - {what it emits}
```

### {painter_group}_call_diagram.md

Document structure:

```
{PAINTER_GROUP} CALL HIERARCHY - VISUAL DIAGRAM
===============================================

                        ┌─────────────────────────────┐
                        │  {ParentPainter}::Paint()   │
                        │  (Entry point)              │
                        └────────────┬────────────────┘
                                     │
                 ┌───────────────────┼───────────────────┐
                 │                   │                   │
                 ▼                   ▼                   ▼
         ┌──────────────┐   ┌──────────────┐   ┌──────────────┐
         │{Condition1}  │   │{Condition2}  │   │{Condition3}  │
         └──────┬───────┘   └──────┬───────┘   └──────┬───────┘
                │                   │                   │
                ▼                   ▼                   ▼
        ┌─────────────────┐ ┌────────────────────┐ ┌──────────────┐
        │{Painter1}       │ │{Painter2}          │ │{Painter3}    │
        │::Paint()        │ │::Paint()           │ │::Paint()     │
        └────┬────────────┘ └────┬───────────────┘ └──────┬───────┘
             │                   │                        │
    ┌────────┴────────┐          │                ┌───────┴────────┐
    │                 │          │                │                │
    ▼                 ▼          ▼                ▼                ▼
┌─────────┐    ┌────────────┐   │          ┌─────────────┐  ┌─────────────┐
│{Check1} │    │{BuildOp}() │   │          │{Analyze}    │  │{Special}    │
└─────────┘    └────────────┘   │          │{Props}      │  │{Cases}      │
                    │           │          └─────────────┘  └─────────────┘
                    ▼           │                │                │
              ┌─────────────┐   │        ┌───────┴────────┐       │
              │{Decision}   │   │        │                │       │
              └──┬───┬────┘    │        ▼                ▼       │
                 │   │         │   ┌──────────┐   ┌────────────┐  │
              ┌──▼───▼──┐      │   │Fast Path │   │Slow Path   │  │
              │Draw{Op} │      │   │{detail}  │   │{detail}    │  │
              └─────────┘      │   └──────────┘   └────────────┘  │
                               │        │              │          │
                               ▼        ▼              ▼          ▼
                          ┌─────────────────────────────────────────┐
                          │          DRAW OPERATIONS                │
                          └─────────────────────────────────────────┘

═══════════════════════════════════════════════════════════════════════════════

PAINT OPERATION TYPES:
══════════════════════

{CATEGORY1} OPS:
────────────────
    {DrawOp1}
        ├─ {param1} ({description})
        ├─ {param2} ({description})
        └─ {param3} ({description})

    {DrawOp2}
        ├─ {param1}
        └─ {param2}

{CATEGORY2} OPS:
────────────────
    {DrawOp3}
        ├─ {param1}
        └─ {param2}

═══════════════════════════════════════════════════════════════════════════════

FLOW CHART: HOW {ELEMENT_TYPE} GETS PAINTED
═══════════════════════════════════════════

    Element: <{tag}>{content}</{tag}>

    START: {EntryPoint}
    │
    └─→ {MainPainter}::Paint()
        │ {context_info}
        │
        └─→ For each {item}:
            │
            ├─→ {SubPainter}::Paint()
            │   │
            │   ├─ Check {condition}? {YES/NO}
            │   │
            │   ├─→ {Step1}()
            │   │   │
            │   │   └─→ {DrawOp}({params})
            │   │
            │   ├─→ {Step2}()
            │   │   │
            │   │   └─→ {DrawOp}({params})
            │   │
            │   └─→ {Step3}()
            │
            └─→ {Output} returned
                │
                └─→ {What happens next}

═══════════════════════════════════════════════════════════════════════════════

TRACE LOGGING EXAMPLE:
══════════════════════

To trace {example_element}:

    [CALL] {MainPainter}::Paint()
        input: {param1}={value1}
               {param2}={value2}
    [CALL]   {SubPainter}::Paint()
        {param}: {value}
    [EMIT]    {DrawOp}({logged_params})
    [RET]   OK
    [RET]  OK

    Output {OutputType}:
    [
        {DrawOp1}(...),
        {DrawOp2}(...)
    ]

═══════════════════════════════════════════════════════════════════════════════
```

### {painter_group}_trace_reference.md

Document structure:

```
{PAINTER_GROUP} - QUICK TRACE REFERENCE
=======================================

CORE ENTRY POINTS TO INSTRUMENT:
━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━

{FLOW_NAME} FLOW:
─────────────────

  {MainPainter}::Paint()
    │ [FILE: reference/{file}.cc:{line}]
    │
    ├─→ {Function1}()
    │   [{CONDITIONAL: only if {condition}}]
    │
    ├─→ {Function2}()
    │   [{Description of what it does}]
    │
    │   ┌─────────────────────────────────────────────────────┐
    │   │ NOTE: {Important information about flow variations} │
    │   │ - {Case1} → {what happens}                          │
    │   │ - {Case2} → {what happens}                          │
    │   └─────────────────────────────────────────────────────┘
    │
    │   [For {specific_case} case:]
    │
    ├─→ {Function3}()
    │
    ├─→ {Function4}()
    │   [FILE: reference/{file}.cc:{line}]
    │   │ [{Description}]
    │   │
    │   ├─→ graphics_context_.{DrawOp1}()
    │   │   *** KEY DRAW OPERATION ***
    │   │
    │   └─→ graphics_context_.{DrawOp2}()
    │       [{Description} - painted INSIDE {Function4}()]
    │       [Lines {X-Y} in {file}.cc]
    │
    ├─→ {Function5}()
    │   [{Description} - painted AFTER {something} for {reason}]
    │
    └─→ {Function6}()
        [{Description} - CONDITIONAL based on {condition}]

{ANOTHER_FLOW_NAME} FLOW:
─────────────────────────

  {AnotherPainter}::Paint()
    [FILE: {path}:{line}]
    │ [Handles: {step1} → {step2} → {step3}]
    │
    ├─ Check: {condition1}? → if not, return {result}
    │
    ├─ Check: {condition2}? → if not, return {result}
    │
    ├─ Call: {Function}() → {what it does}
    │
    ├─ IF {condition}:
    │   └─→ {DrawOp}()
    │       *** KEY DRAW OPERATION ***
    │
    └─ ELSE:
        └─→ {AlternateDrawOp}()
            *** KEY DRAW OPERATION ***

═══════════════════════════════════════════════════════════════════════════════

KEY DRAW OPERATIONS EMITTED:
────────────────────────────

{PAINTER1} EMITS:
  • {DrawOp1}({param1}, {param2}, {param3})
  • {DrawOp2}({params}) - {description}
  • {DrawOp3}({params}) - {description}

{PAINTER2} EMITS:
  • {DrawOp4}({params})
  • {DrawOp5}({params})

═══════════════════════════════════════════════════════════════════════════════

PAINT PHASE ROUTING:
────────────────────

Each painter is called during specific paint phases:

  {kPhase1}
    └─→ {Painter1}::Paint() [{description}]

  {kPhase2}
    └─→ {Painter2}::Paint() [{description}]
    └─→ {Painter3}::Paint()

  {kPhase3}
    └─→ {Painter4}::Paint()

═══════════════════════════════════════════════════════════════════════════════

INSTRUMENTATION CHECKLIST:
──────────────────────────

For comprehensive paint tracing, log these function entries/exits:

LEVEL 1 - TOP ENTRY POINTS:
  [ ] {MainPainter}::Paint()
        - Log: {what_to_log1}, {what_to_log2}
  [ ] {OtherPainter}::Paint()
        - Log: {what_to_log}

LEVEL 2 - DISPATCH POINTS:
  [ ] {SubPainter1}::Paint()
        - Log: {what_to_log}
  [ ] {SubPainter2}::{Method}()
        - Log: {what_to_log}

LEVEL 3 - DECISION POINTS:
  [ ] {DecisionFunction}()
        - Log: {decision_values}
  [ ] {AnotherDecision}()
        - Log: {decision_values}

LEVEL 4 - DRAW OPERATIONS:
  [ ] graphics_context_.{DrawOp1}()
        - Log: {params_to_log}
  [ ] graphics_context_.{DrawOp2}()
        - Log: {params_to_log}

═══════════════════════════════════════════════════════════════════════════════

FILE LOCATIONS:
───────────────

Reference Implementation (Chromium source):
  04_paint/{painter_group}/reference/{file1}.cc
  04_paint/{painter_group}/reference/{file2}.cc

Headers:
  04_paint/{painter_group}/reference/{file1}.h
  04_paint/{painter_group}/reference/{file2}.h

═══════════════════════════════════════════════════════════════════════════════
```

## Invocation

```
ANALYSIS AGENT

For painter group: {painter_group}
Reference files at: 04_paint/{painter_group}/reference/
Chromium source at: ./chromium/src/third_party/blink/renderer/core/paint/

1. Read paint_layer_painter.cc to understand how this painter is called
   - What paint phase triggers it?
   - What context is passed to it?

2. Read the main entry point (e.g., {main_painter}.cc)
   - Trace Paint() method line by line
   - For each function call, note:
     * File and line number
     * What it does
     * What parameters it takes
     * Whether it's conditional

3. Recursively trace called functions
   - Follow calls to other painters in the group
   - Stop at graphics_context_ calls (these are draw operations)
   - Document decision points (if/switch/ternary)

4. Copy missing reference files
   - If you discover a file that belongs to this painter group
   - And it's not already in 04_paint/{painter_group}/reference/
   - Copy it from Chromium source (both .cc and .h)
   - Do NOT copy utility classes shared across groups

5. Create documentation using the templates in this file:
   - 04_paint/{painter_group}/docs/{painter_group}_hierarchy.md
   - 04_paint/{painter_group}/docs/{painter_group}_call_diagram.md
   - 04_paint/{painter_group}/docs/{painter_group}_trace_reference.md
```

## Painter Group Specifics

### text_painter group

Main entry: `TextFragmentPainter::Paint()`
Key files:
- text_fragment_painter.cc (entry)
- text_painter.cc (glyph rendering)
- text_decoration_painter.cc (underline/overline/line-through)
- highlight_painter.cc (selection/spelling)
- decoration_line_painter.cc (line drawing)

Focus on:
- HighlightPainter::Case switch statement
- Paint order (decorations before/after text)
- Emphasis marks (inside TextPainter::Paint)

### block_painter group

Main entry: `BlockPainter::Paint()` or `BoxFragmentPainter::Paint()`
Key files:
- block_painter.cc (background)
- box_fragment_painter.cc (fragment coordination)

Focus on:
- Visibility checks
- Border radius handling
- Shadow rendering

### border_painter group

Main entry: `BoxBorderPainter::Paint()`
Key files:
- box_border_painter.cc

Focus on:
- Fast path vs slow path decision
- Per-side rendering
- Special styles (double, dotted, dashed, groove, ridge)

## Execution

**One Analysis Agent runs per painter directory.** Agents can run in parallel.

### Painter Directories to Analyze

Run analysis for these 14 directories (one agent each):

| Directory | Main Entry Point | Priority |
|-----------|-----------------|----------|
| text_painter | TextFragmentPainter::Paint() | High |
| block_painter | BoxFragmentPainter::Paint() | High |
| border_painter | BoxBorderPainter::Paint() | High |
| layout_painter | PaintLayerPainter::Paint() | High |
| svg_painter | SVGContainerPainter::Paint() | Medium |
| table_painter | TablePainter::Paint() | Medium |
| outline_painter | OutlinePainter::Paint() | Medium |
| scrollable_area_painter | ScrollableAreaPainter::Paint() | Medium |
| theme_painter | ThemePainter::Paint() | Medium |
| css_mask_painter | CSSMaskPainter::Paint() | Low |
| fieldset_painter | FieldsetPainter::Paint() | Low |
| gap_decorations_painter | GapDecorationsPainter::Paint() | Low |
| mathml_painter | MathMLPainter::Paint() | Low |
| nine_piece_image_painter | NinePieceImagePainter::Paint() | Low |

### Skipped Directories

The following directories are **NOT analyzed** for now:

| Directory | Reason |
|-----------|--------|
| replaced_painter | Replaced content (images, video, canvas, iframes) - skipped for now |

### Parallel Execution Example

```
# Run analysis for high-priority painters in parallel
Analysis Agent for text_painter
Analysis Agent for block_painter
Analysis Agent for border_painter
Analysis Agent for layout_painter
```

Each agent outputs to its respective `04_paint/{painter_group}/docs/` directory.
