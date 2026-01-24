# Table Painter Call Diagram

## Paint Flow Overview

```
PaintLayerPainter::Paint()
    |
    v
PaintLayerPainter::PaintWithPhase(kSelfBlockBackgroundOnly)
    |
    v
BoxFragmentPainter::Paint()
    |
    v
BoxFragmentPainter::PaintBoxDecorationBackground()
    |
    +-- BoxFragmentPainter::PaintBoxDecorationBackgroundWithRect()
            |
            +-- [If IsTablePart()] --> Table Painter Dispatch
```

## Table Painter Dispatch

```
BoxFragmentPainter::PaintBoxDecorationBackgroundWithRect()
    |
    +-- IsTableCell()?
    |       |
    |       +-- YES --> TableCellPainter::PaintBoxDecorationBackground()
    |
    +-- IsTableRow()?
    |       |
    |       +-- YES --> TableRowPainter::PaintBoxDecorationBackground()
    |
    +-- IsTableSection()?
    |       |
    |       +-- YES --> TableSectionPainter::PaintBoxDecorationBackground()
    |
    +-- IsTable()?
            |
            +-- YES --> TablePainter::PaintBoxDecorationBackground()
```

## TablePainter Flow

```
TablePainter::PaintBoxDecorationBackground(paint_info, paint_rect, box_decoration_data)
    |
    +-- Compute grid_paint_rect from fragment_.TableGridRect()
    |
    +-- [If box_decoration_data.ShouldPaint()]
    |       |
    |       +-- BoxFragmentPainter::PaintBoxDecorationBackgroundWithRectImpl()
    |               (paints table background on grid rect)
    |
    +-- Collect column_geometries with backgrounds
    |
    +-- [For each section child]
            |
            +-- TableSectionPainter::PaintColumnsBackground()
                    (paints <col>/<colgroup> backgrounds)
```

## TableSectionPainter Flow

```
TableSectionPainter::PaintBoxDecorationBackground(paint_info, paint_rect, box_decoration_data)
    |
    +-- [If ShouldPaintShadow()] --> PaintNormalBoxShadow()
    |
    +-- Compute part_rect (handle fragmentation)
    |
    +-- [For each row child]
    |       |
    |       +-- TableRowPainter::PaintTablePartBackgroundIntoCells()
    |               (paints section background into cells)
    |
    +-- [If ShouldPaintShadow()] --> PaintInsetBoxShadowWithInnerRect()


TableSectionPainter::PaintColumnsBackground(paint_info, section_offset, columns_rect, column_geometries)
    |
    +-- [For each row child]
            |
            +-- TableRowPainter::PaintColumnsBackground()
```

## TableRowPainter Flow

```
TableRowPainter::PaintBoxDecorationBackground(paint_info, paint_rect, box_decoration_data)
    |
    +-- [If ShouldPaintShadow()] --> PaintNormalBoxShadow()
    |
    +-- Compute part_rect (handle fragmentation)
    |
    +-- PaintTablePartBackgroundIntoCells(paint_info, row_layout_box, part_rect, row_offset)
    |
    +-- [If ShouldPaintShadow()] --> PaintInsetBoxShadowWithInnerRect()


TableRowPainter::PaintTablePartBackgroundIntoCells(paint_info, table_part, part_rect, row_offset)
    |
    +-- [For each cell child]
            |
            +-- TableCellPainter::PaintBackgroundForTablePart()


TableRowPainter::PaintColumnsBackground(paint_info, row_offset, columns_rect, column_geometries)
    |
    +-- [For each cell child]
            |
            +-- [Match cell to column geometry]
                    |
                    +-- TableCellPainter::PaintBackgroundForTablePart()
```

## TableCellPainter Flow

```
TableCellPainter::PaintBoxDecorationBackground(paint_info, paint_rect, box_decoration_data)
    |
    +-- TableCellBackgroundClipper (clips to avoid covering collapsed borders)
    |
    +-- BoxFragmentPainter::PaintBoxDecorationBackgroundWithRectImpl()


TableCellPainter::PaintBackgroundForTablePart(paint_info, table_part, part_rect, cell_offset)
    |
    +-- [If not visible or transfers to view] --> return
    |
    +-- Get background color and layers from table_part style
    |
    +-- TableCellBackgroundClipper (clips for collapsed borders)
    |
    +-- BoxBackgroundPaintContext (sets up cell-relative painting)
    |
    +-- BoxFragmentPainter::PaintFillLayers()
```

## Collapsed Borders Flow

```
BoxFragmentPainter::PaintObject()
    |
    +-- [If IsTable() && phase == kDescendantBlockBackgroundsOnly]
            |
            +-- TablePainter::PaintCollapsedBorders(paint_info, paint_offset, visual_rect)
                    |
                    +-- Get collapsed_borders from fragment_.TableCollapsedBorders()
                    |
                    +-- Get collapsed_borders_geometry from fragment_.TableCollapsedBordersGeometry()
                    |
                    +-- DrawingRecorder (caches the border painting)
                    |
                    +-- [For each section]
                            |
                            +-- [For each edge in section]
                                    |
                                    +-- ComputeEdgeJoints() (determine joint winners)
                                    |
                                    +-- [If edge.CanPaint()]
                                            |
                                            +-- BoxBorderPainter::DrawBoxSide()
```

## Background Layering Order

When a table has backgrounds at multiple levels, they paint in this order (bottom to top):

1. **Table background** - painted on grid rect by TablePainter
2. **Column/Colgroup backgrounds** - painted into cells by TableSectionPainter -> TableRowPainter -> TableCellPainter
3. **Section backgrounds** - painted into cells by TableSectionPainter -> TableRowPainter -> TableCellPainter
4. **Row backgrounds** - painted into cells by TableRowPainter -> TableCellPainter
5. **Cell backgrounds** - painted by TableCellPainter

All backgrounds paint BEFORE collapsed borders (which paint in `kDescendantBlockBackgroundsOnly` phase).

## TableCellBackgroundClipper

Used when:
- Table has collapsed borders AND
- Any ancestor table part (cell, row, or section) has a layer

Purpose: Clips background painting to prevent covering collapsed borders.

```
TableCellBackgroundClipper(context, table_cell, cell_rect, is_painting_in_contents_space)
    |
    +-- needs_clip_ = !is_painting_in_contents_space &&
    |                 (cell.HasLayer() || row.HasLayer() || section.HasLayer()) &&
    |                 table.HasCollapsedBorders()
    |
    +-- [If needs_clip_]
            |
            +-- Compute clip_rect = cell_rect - border_outsets
            |
            +-- context.Save()
            |
            +-- context.Clip(clip_rect)
            |
            +-- [On destruction] context.Restore()
```
