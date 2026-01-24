# Table Painter Hierarchy

## Overview

The table painting system in Chromium Blink follows a hierarchical structure that mirrors the HTML table model. Each level of the hierarchy has a dedicated painter class that handles the decoration and background painting for that table part.

## Class Hierarchy

```
TablePainter
    |
    +-- TableSectionPainter (thead, tbody, tfoot)
            |
            +-- TableRowPainter (tr)
                    |
                    +-- TableCellPainter (td, th)
```

## Painter Classes

### TablePainter

**Location**: `third_party/blink/renderer/core/paint/table_painters.cc`

**Constructor**: Takes a `PhysicalBoxFragment` where `fragment.IsTable()` is true.

**Responsibilities**:
- Paint table box decoration background on the grid rect
- Paint column group (`<colgroup>`) and column (`<col>`) backgrounds
- Paint collapsed borders across all sections

**Key Methods**:
- `PaintBoxDecorationBackground()`: Paints table background and column backgrounds
- `PaintCollapsedBorders()`: Paints all collapsed borders for the table
- `WillCheckColumnBackgrounds()`: Returns true if column geometries exist

### TableSectionPainter

**Constructor**: Takes a `PhysicalBoxFragment` where `fragment.IsTableSection()` is true.

**Responsibilities**:
- Paint section (thead/tbody/tfoot) box decorations
- Delegate background painting to rows and cells
- Coordinate column background painting

**Key Methods**:
- `PaintBoxDecorationBackground()`: Paints section shadows, delegates to rows
- `PaintColumnsBackground()`: Iterates rows to paint column backgrounds into cells

### TableRowPainter

**Constructor**: Takes a `PhysicalBoxFragment` where `fragment.IsTableRow()` is true.

**Responsibilities**:
- Paint row box decorations
- Paint row backgrounds into cells
- Coordinate column backgrounds for cells

**Key Methods**:
- `PaintBoxDecorationBackground()`: Paints row shadows and delegates to cells
- `PaintTablePartBackgroundIntoCells()`: Paints a table part's background into all cells
- `PaintColumnsBackground()`: Paints column backgrounds into cells

### TableCellPainter

**Constructor**: Takes a `PhysicalBoxFragment` for a table cell.

**Responsibilities**:
- Paint cell box decoration background
- Paint backgrounds for table parts (table, section, row, column)
- Handle collapsed border clipping

**Key Methods**:
- `PaintBoxDecorationBackground()`: Paints cell decorations with collapsed border clipping
- `PaintBackgroundForTablePart()`: Paints a table part's background into this cell

## Entry Point

Table painters are invoked from `BoxFragmentPainter::PaintBoxDecorationBackgroundWithRect()`:

```cpp
if (GetPhysicalFragment().IsTablePart()) {
    if (box_fragment_.IsTableCell()) {
        TableCellPainter(box_fragment_).PaintBoxDecorationBackground(...);
    } else if (box_fragment_.IsTableRow()) {
        TableRowPainter(box_fragment_).PaintBoxDecorationBackground(...);
    } else if (box_fragment_.IsTableSection()) {
        TableSectionPainter(box_fragment_).PaintBoxDecorationBackground(...);
    } else {
        DCHECK(box_fragment_.IsTable());
        TablePainter(box_fragment_).PaintBoxDecorationBackground(...);
    }
}
```

## Fragment Types

Each painter validates its fragment type:
- `TablePainter`: `DCHECK(fragment_.IsTable())`
- `TableSectionPainter`: `DCHECK(fragment_.IsTableSection())`
- `TableRowPainter`: `DCHECK(fragment_.IsTableRow())`
- `TableCellPainter`: No DCHECK (accepts any cell fragment)

## Painting Coordination

The hierarchy uses a delegation pattern where:
1. Higher-level painters iterate their children
2. Each child painter handles its own decorations
3. Background painting flows from outer (table) to inner (cell)
4. Collapsed borders paint after all backgrounds (in `kDescendantBlockBackgroundsOnly` phase)
