DISCOVERY AGENT
===============

Finds all painter classes in Chromium's paint system and catalogs them.

## Strategy (in order)

### Step 1: Start at entry point

Read `./chromium/src/third_party/blink/renderer/core/paint/paint_layer_painter.cc`

- Trace which `*Painter` classes it calls/instantiates
- Follow those to find what they call (e.g., `TextFragmentPainter` → `TextPainter`, `TextDecorationPainter`, etc.)
- Build a call tree of painters

### Step 2: Scan paint directory

List all `*_painter.{cc,h}` in `./chromium/src/third_party/blink/renderer/core/paint/`

- Find any painters not discovered in step 1
- Mark them as "not in main paint flow" vs "in paint flow"

### Step 3: Group related painters

Group painters by their primary function:

- **text_painter group**: text_painter, text_fragment_painter, text_decoration_painter, decoration_line_painter, highlight_painter, etc.
- **block_painter group**: block_painter, box_fragment_painter, etc.
- **border_painter group**: box_border_painter, etc.
- (discover additional groups as found)

### Step 4: Check existing directories

For each painter group:
- Check if `04_paint/{group}/` directory exists
- Check if `04_paint/{group}/reference/` contains all relevant .cc and .h files
- List any missing files

## Actions

The agent directly modifies the filesystem:

### 1. Create directories

For each painter group, create if not exists:
```
04_paint/{group}/
04_paint/{group}/reference/
04_paint/{group}/src/
04_paint/{group}/docs/
```

### 2. Copy reference files

Copy .cc and .h files from Chromium source to reference/:
```
./chromium/src/third_party/blink/renderer/core/paint/text_painter.cc
  → 04_paint/text_painter/reference/text_painter.cc

./chromium/src/third_party/blink/renderer/core/paint/text_painter.h
  → 04_paint/text_painter/reference/text_painter.h
```

### 3. Output report

Create `04_paint/docs/painter_discovery_report.md` with:
- Paint flow call tree (ASCII diagram)
- List of all painters found
- Which painters are in main paint flow vs not
- What directories were created
- What files were copied

## Invocation

```
DISCOVERY AGENT

Search Chromium paint code starting from paint_layer_painter.cc.

1. Read ./chromium/src/third_party/blink/renderer/core/paint/paint_layer_painter.cc
   - Find all *Painter classes it references
   - For each painter found, read its source and find what painters IT calls
   - Build a call tree

2. List all *_painter.{cc,h} files in ./chromium/src/third_party/blink/renderer/core/paint/
   - Compare against painters found in step 1
   - Mark each as "in_paint_flow" or "not_in_paint_flow"

3. Group related painters:
   - text_painter group: anything with text_, decoration_, highlight_ prefix
   - block_painter group: block_, box_fragment_, box_model_
   - border_painter group: box_border_, border_shape_
   - svg_painter group: svg_* prefix
   - table_painter group: table_* prefix
   - For each remaining painter: create its own directory (e.g., outline_painter/, theme_painter/)
   - NEVER create a "misc" or catch-all category

4. For each group:
   - Create 04_paint/{group}/ directory if it doesn't exist
   - Create 04_paint/{group}/reference/ subdirectory
   - Copy all .cc and .h files for that group from Chromium to reference/

5. Output report to 04_paint/docs/painter_discovery_report.md
```
