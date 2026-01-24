REVIEW AGENT
============

Reviews the Discovery Agent's work to validate groupings and find missing items.

## Goal

Validate that:
1. Existing painter groupings are correct (no file belongs in a different group)
2. No painter groups were missed
3. No files were missed within each group

## Strategy

### Step 1: List all *_painter.{cc,h} files in Chromium source

Scan `./chromium/src/third_party/blink/renderer/core/paint/` for all painter files.

Create a complete inventory of every painter file that exists.

### Step 2: List all files in 04_paint/*/reference/

For each painter directory in `04_paint/`, list all files in its `reference/` subdirectory.

### Step 3: Compare inventories

For each file in Chromium source:
- Is it present in any 04_paint/{group}/reference/ directory?
- If yes, does it belong in that group? (based on naming and functionality)
- If no, which group should it belong to?

### Step 4: Check grouping logic

Review each group to ensure files are correctly grouped:

**text_painter group** should contain:
- text_painter.{cc,h}
- text_fragment_painter.{cc,h}
- text_decoration_painter.{cc,h}
- text_decoration_info.{cc,h}
- text_combine_painter.{cc,h}
- text_shadow_painter.{cc,h}
- text_paint_style.h
- decoration_line_painter.{cc,h}
- highlight_painter.{cc,h}
- styleable_marker_painter.{cc,h}

**block_painter group** should contain:
- box_fragment_painter.{cc,h}
- box_model_object_painter.{cc,h}
- box_painter.{cc,h}
- inline_box_fragment_painter.{cc,h}
- line_box_fragment_painter.{cc,h} (if exists)

**border_painter group** should contain:
- box_border_painter.{cc,h}
- border_shape_painter.{cc,h}
- border_shape_utils.{cc,h}
- contoured_border_geometry.{cc,h}

**svg_painter group** should contain:
- All svg_*_painter.{cc,h} files

**table_painter group** should contain:
- table_painters.{cc,h} (or individual table_*_painter files)

**layout_painter group** should contain:
- paint_layer_painter.{cc,h}
- object_painter.{cc,h}
- view_painter.{cc,h}
- frame_painter.{cc,h}
- frame_set_painter.{cc,h}
- fragment_painter.{cc,h}

**replaced_painter group** should contain:
- replaced_painter.{cc,h}
- image_painter.{cc,h}
- video_painter.{cc,h}
- html_canvas_painter.{cc,h}
- embedded_content_painter.{cc,h}
- embedded_object_painter.{cc,h}

**Individual groups** (each should have its own directory):
- css_mask_painter
- fieldset_painter
- gap_decorations_painter
- mathml_painter
- nine_piece_image_painter
- outline_painter
- scrollable_area_painter
- theme_painter

### Step 5: Identify issues

Report:
1. **Misplaced files**: Files in wrong group
2. **Missing files**: Files in Chromium source but not in any reference/
3. **Missing groups**: Painters that need their own directory but don't have one
4. **Orphan files**: Files in reference/ that don't exist in Chromium source

## Output

Create `04_paint/docs/discovery_review_report.md` with:

```
DISCOVERY AGENT REVIEW REPORT
=============================

## Summary

- Total painter files in Chromium: {count}
- Total files in 04_paint/*/reference/: {count}
- Files correctly placed: {count}
- Issues found: {count}

## Issues Found

### Misplaced Files

| File | Current Group | Should Be In |
|------|---------------|--------------|
| {file} | {current} | {correct} |

### Missing Files

| File | Should Be In Group |
|------|-------------------|
| {file} | {group} |

### Missing Groups

| Painter Name | Reason |
|--------------|--------|
| {painter} | {why it needs own group} |

### Orphan Files

| File | In Group | Notes |
|------|----------|-------|
| {file} | {group} | {explanation} |

## Grouping Validation

### text_painter ✓/✗
- Expected files: {list}
- Present: {list}
- Missing: {list}
- Extra: {list}

### block_painter ✓/✗
...

## Recommendations

1. {recommendation 1}
2. {recommendation 2}
...
```

## Invocation

```
REVIEW AGENT

Review the Discovery Agent's work for completeness and correctness.

1. List all *_painter.{cc,h} files in ./chromium/src/third_party/blink/renderer/core/paint/
   - Create complete inventory

2. List all files in 04_paint/*/reference/ directories
   - Note which group each file is in

3. Compare the two lists:
   - Find files in Chromium but not in any reference/
   - Find files in reference/ but not in Chromium (orphans)

4. Validate groupings:
   - For each group, check if files match expected pattern
   - text_painter: text_*, decoration_*, highlight_*, styleable_marker_*
   - block_painter: box_*, inline_box_*, line_box_*
   - border_painter: box_border_*, border_shape_*, contoured_border_*
   - svg_painter: svg_*
   - table_painter: table_*
   - layout_painter: paint_layer_*, object_*, view_*, frame_*, fragment_*
   - replaced_painter: replaced_*, image_*, video_*, html_canvas_*, embedded_*
   - Individual painters: each specialized painter in its own directory

5. Check for support files that may have been missed:
   - Read main painter .cc files to find #include statements
   - Check if included *_painter.h files are in the same group
   - Check for related *_info.{cc,h} or *_style.{cc,h} files

6. Output report to 04_paint/docs/discovery_review_report.md
```
