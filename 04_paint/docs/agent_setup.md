AGENT-BASED PAINT TRACING SYSTEM
=================================

This document describes a multi-agent workflow for adding span trace logging
to Chromium's paint code using Claude Code.

## Overview

The system uses three specialized agents that work in sequence:

```
┌─────────────────────┐     ┌─────────────────────┐     ┌─────────────────────┐
│   DISCOVERY AGENT   │────▶│   ANALYSIS AGENT    │────▶│ IMPLEMENTATION AGENT│
│                     │     │                     │     │                     │
│ • Find painters     │     │ • Study call flow   │     │ • Add trace spans   │
│ • Create dirs       │     │ • Build hierarchy   │     │ • Edit source files │
│ • Copy reference    │     │ • Document patterns │     │ • Verify changes    │
└─────────────────────┘     └─────────────────────┘     └─────────────────────┘
        │                           │                           │
        ▼                           ▼                           ▼
   04_paint/                  04_paint/docs/              chromium/src/
   {painter}/reference/       {painter}_hierarchy.md      third_party/blink/
```

## Agent 1: Discovery Agent

**Purpose**: Identify all relevant painters in Chromium source and set up project structure.

**Inputs**:
- Chromium source path (e.g., `/chromium/src/third_party/blink/renderer/core/paint/`)
- Target paint phase (e.g., "text", "background", "border")

**Outputs**:
- Directory structure: `04_paint/{painter_name}/reference/`
- Copied reference files from Chromium source

**Key tasks**:
1. Search Chromium for `*_painter.cc` and `*_painter.h` files
2. Identify dependencies (GraphicsContext, PaintInfo, etc.)
3. Create standardized directory structure
4. Copy relevant source files to reference/

**Example invocation**:
```
Task: Discovery Agent
Prompt: |
  Search /chromium/src/third_party/blink/renderer/core/paint/ for all painter classes.
  For each painter found:
  1. Create directory: 04_paint/{painter_name}/
  2. Create subdirs: reference/, src/, test/, docs/
  3. Copy the .cc and .h files to reference/
  4. Create a manifest.json listing all files and their relationships

  Focus on these painters: text_painter, text_fragment_painter, text_decoration_painter,
  box_border_painter, block_painter, highlight_painter
```

## Agent 2: Analysis Agent

**Purpose**: Study Chromium source code to understand call hierarchies and paint order.

**Inputs**:
- Reference files from Discovery Agent
- Existing documentation in 04_paint/docs/

**Outputs**:
- `{painter}_hierarchy.md` - Call hierarchy for each painter
- `{painter}_trace_points.md` - Recommended instrumentation points
- Updated `call_diagram.md` with new painters

**Key tasks**:
1. Read reference source files
2. Trace function calls from entry points
3. Identify decision branches (if/switch statements)
4. Document paint order and draw operations
5. Identify key trace points

**Example invocation**:
```
Task: Analysis Agent
Prompt: |
  Analyze the reference code in 04_paint/text_painter/reference/

  Create documentation covering:
  1. Entry point: TextPainter::Paint() - all parameters and their purpose
  2. Call hierarchy: What functions does it call and in what order?
  3. Decision points: What conditions affect the paint flow?
  4. Draw operations: What graphics operations are emitted?
  5. Trace points: Where should we add span logging?

  Output to: 04_paint/docs/text_painter_analysis.md

  Reference existing docs: paint_hierarchy.md, call_diagram.md, trace_reference.md
```

## Agent 3: Implementation Agent

**Purpose**: Add span trace logging to Chromium source files.

**Inputs**:
- Analysis documentation from Agent 2
- Trace point specifications
- Chromium source files to modify

**Outputs**:
- Modified Chromium source files with trace spans
- Patch files for review

**Key tasks**:
1. Read trace point specifications
2. Add TRACE_EVENT macros at entry/exit points
3. Log relevant parameters
4. Ensure proper span nesting
5. Generate patch files

**Example invocation**:
```
Task: Implementation Agent
Prompt: |
  Add trace span logging to text_painter.cc based on:
  - 04_paint/docs/text_painter_analysis.md
  - 04_paint/docs/trace_reference.md

  Use Chromium's TRACE_EVENT macros:
  - TRACE_EVENT0("blink", "TextPainter::Paint")
  - TRACE_EVENT1("blink", "TextPainter::Paint", "text_length", length)

  Trace points to instrument:
  1. TextPainter::Paint() entry/exit
  2. DrawText() call with glyph count
  3. DrawEmphasisMarks() if called

  Generate a patch file: 04_paint/patch/text_painter.patch
```

## Workflow Execution

### Option A: Sequential Manual Execution

Run each agent manually in sequence:

```bash
# Step 1: Run Discovery Agent
claude "Run discovery agent for paint code..."

# Step 2: Run Analysis Agent (after discovery completes)
claude "Run analysis agent on discovered painters..."

# Step 3: Run Implementation Agent (after analysis completes)
claude "Run implementation agent to add traces..."
```

### Option B: Orchestrated Execution

Use a master prompt that coordinates all three:

```
I need to add trace logging to Chromium's paint code.

Phase 1 - Discovery:
- Find all painters in /chromium/src/third_party/blink/renderer/core/paint/
- Set up directories in 04_paint/

Phase 2 - Analysis:
- For each painter, analyze the call hierarchy
- Document trace points in 04_paint/docs/

Phase 3 - Implementation:
- Add TRACE_EVENT macros to the source files
- Generate patch files in 04_paint/patch/

Start with Phase 1, then proceed to Phase 2 when complete.
Wait for my approval before Phase 3.
```

### Option C: Parallel Agent Execution

For independent painters, run analysis agents in parallel:

```
Run these analysis tasks in parallel:
1. Analyze text_painter reference code
2. Analyze block_painter reference code
3. Analyze border_painter reference code

Each should output to 04_paint/docs/{painter}_analysis.md
```

## Directory Structure After Execution

```
04_paint/
├── docs/
│   ├── agent_setup.md          # This file
│   ├── paint_hierarchy.md      # Overall hierarchy
│   ├── call_diagram.md         # Visual diagrams
│   ├── trace_reference.md      # Quick reference
│   ├── text_painter_analysis.md
│   ├── block_painter_analysis.md
│   └── border_painter_analysis.md
│
├── text_painter/
│   ├── reference/              # Chromium source copies
│   │   ├── text_painter.cc
│   │   ├── text_painter.h
│   │   ├── text_fragment_painter.cc
│   │   └── text_fragment_painter.h
│   ├── src/                    # Our implementation
│   ├── test/                   # Tests
│   └── docs/                   # Painter-specific docs
│
├── block_painter/
│   ├── reference/
│   ├── src/
│   ├── test/
│   └── docs/
│
├── border_painter/
│   ├── reference/
│   ├── src/
│   ├── test/
│   └── docs/
│
└── patch/                      # Generated patches
    ├── text_painter.patch
    ├── block_painter.patch
    └── border_painter.patch
```

## Trace Span Format

The implementation agent should use consistent trace spans:

```cpp
// Entry point tracing
TRACE_EVENT_BEGIN("blink.paint", "TextPainter::Paint",
    "text_length", fragment_paint_info.text.length(),
    "has_emphasis", emphasis_mark_.length() > 0);

// ... paint operations ...

TRACE_EVENT_END("blink.paint");

// Or scoped version:
TRACE_EVENT("blink.paint", "TextPainter::Paint",
    "text_length", fragment_paint_info.text.length());
```

## Agent Communication

Agents communicate through files:

| From | To | File | Content |
|------|-----|------|---------|
| Discovery | Analysis | `manifest.json` | List of painters and files |
| Analysis | Implementation | `*_analysis.md` | Trace points and hierarchy |
| Implementation | User | `*.patch` | Source modifications |

## Error Handling

Each agent should:
1. Validate inputs exist before proceeding
2. Report missing dependencies clearly
3. Create partial outputs if possible
4. Log decisions for debugging

## Customization

### Adding New Painters

1. Add painter name to Discovery Agent's focus list
2. Run Discovery Agent to set up directory
3. Run Analysis Agent on new reference code
4. Run Implementation Agent to add traces

### Changing Trace Format

Modify Implementation Agent prompt to use different macro style:
- Chromium: `TRACE_EVENT0`, `TRACE_EVENT1`, `TRACE_EVENT2`
- Custom: Your own tracing macros
- perfetto: Direct perfetto API calls

### Adjusting Trace Granularity

Analysis Agent can be tuned for:
- **Coarse**: Entry/exit of main Paint() functions only
- **Medium**: Include major decision points
- **Fine**: Every draw operation and helper call

## Example Session

```
User: Set up tracing for text_painter

Claude: I'll run the three-agent workflow:

[Agent 1: Discovery]
- Found text_painter.cc, text_painter.h in chromium source
- Created 04_paint/text_painter/reference/
- Copied 4 files

[Agent 2: Analysis]
- Analyzed TextPainter::Paint() call flow
- Identified 5 trace points
- Created 04_paint/docs/text_painter_analysis.md

[Agent 3: Implementation]
- Added TRACE_EVENT macros at 5 locations
- Generated 04_paint/patch/text_painter.patch

Ready for review. Run `git apply 04_paint/patch/text_painter.patch` to apply.
```

## Related Files

- [paint_hierarchy.md](paint_hierarchy.md) - Overall paint hierarchy
- [call_diagram.md](call_diagram.md) - Visual call flow diagrams
- [trace_reference.md](trace_reference.md) - Quick trace point reference
