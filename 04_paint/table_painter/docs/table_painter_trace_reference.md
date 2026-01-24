# Table Painter Trace Reference

## Key Classes and Files

| Class | File | Purpose |
|-------|------|---------|
| `TablePainter` | `table_painters.cc` | Paints table background and collapsed borders |
| `TableSectionPainter` | `table_painters.cc` | Paints section backgrounds via rows |
| `TableRowPainter` | `table_painters.cc` | Paints row backgrounds into cells |
| `TableCellPainter` | `table_painters.cc` | Paints cell backgrounds and table part backgrounds |
| `TableBorders` | `layout/table/table_borders.h` | Stores and computes collapsed border edges |
| `TableCollapsedEdge` | `table_painters.cc` (anonymous namespace) | Helper for painting collapsed border edges |
| `TableCellBackgroundClipper` | `table_painters.cc` (anonymous namespace) | Clips backgrounds around collapsed borders |
| `BoxFragmentPainter` | `box_fragment_painter.cc` | Entry point that dispatches to table painters |

## Important Data Structures

### TableBorders

Precomputes and stores collapsed borders for efficient access during painting.

**Edge Storage**:
- Edges stored in a 1D array
- Each cell has 2 edges: left and top
- Right edge = next cell's left edge
- Bottom edge = cell below's top edge
- Total edges for R rows x C columns: `2 * (R+1) * (C+1)`

**Key Properties**:
- `edges_per_row_`: Number of edges per table row
- `is_collapsed_`: Whether borders are collapsed
- `table_border_`: The table's border box strut

**Edge Structure**:
```cpp
struct Edge {
    Member<const ComputedStyle> style;  // Style defining the border
    EdgeSide edge_side;                  // Which side: kTop, kRight, kBottom, kLeft
    wtf_size_t box_order;               // For painting precedence (lower wins)
};
```

### TableCollapsedEdge

Helper class for iterating and painting collapsed border edges.

**Key Methods**:
- `CanPaint()`: Returns true if edge should be painted
- `BorderWidth()`, `BorderStyle()`, `BorderColor()`: Edge properties
- `IsInlineAxis()`: True for horizontal edges
- `TableColumn()`, `TableRow()`: Edge position in table
- `CompareForPaint()`: Determines which edge wins at intersections
- Edge navigation: `EdgeBeforeStartIntersection()`, `EdgeAfterStartIntersection()`, etc.

### CollapsedTableBordersGeometry

Provides column offsets for positioning collapsed borders.

```cpp
struct CollapsedTableBordersGeometry {
    Vector<LayoutUnit> columns;  // Column start offsets
};
```

## Border Collapse Algorithm

### Edge Priority Rules

When two edges meet at an intersection, the winner is determined by:

1. **Paintability**: Edges that can paint win over those that cannot
2. **Border Width**: Wider borders win
3. **Border Style**: Styles ranked (high to low): double > solid > dashed > dotted > ridge > outset > groove > inset
4. **Box Order**: Lower box order wins (earlier in DOM traversal)

### Joint Computation

`ComputeEdgeJoints()` determines how edges share intersection space:

```
     before_edge
          |
over_edge-+-after_edge
          |
     under_edge

For an inline (horizontal) edge:
- Start intersection: where edge meets left cell boundary
- End intersection: where edge meets right cell boundary

For a block (vertical) edge:
- Start intersection: where edge meets top cell boundary
- End intersection: where edge meets bottom cell boundary
```

### Fragmentation Handling

For fragmented tables (spanning multiple pages/columns):
- `is_start_row_fragmented`: Row continues from previous fragment
- `is_end_row_fragmented`: Row continues to next fragment
- Borders at fragmentation boundaries are painted as "half" borders
- `is_over_edge_fragmentation_boundary` / `is_under_edge_fragmentation_boundary` flags

## Painting Phases

### Background Phase (`kSelfBlockBackgroundOnly`)

Each table part paints its own background:

1. **Table**: Paints on `grid_paint_rect` (union of all sections)
2. **Columns**: Painted into cells via `PaintColumnsBackground()` chain
3. **Sections**: Painted into cells via `PaintTablePartBackgroundIntoCells()`
4. **Rows**: Painted into cells via `PaintTablePartBackgroundIntoCells()`
5. **Cells**: Painted directly

### Collapsed Borders Phase (`kDescendantBlockBackgroundsOnly`)

Borders paint AFTER all backgrounds to appear on top:

```cpp
// In BoxFragmentPainter::PaintObject()
if (box_fragment_.IsTable() &&
    paint_phase == PaintPhase::kDescendantBlockBackgroundsOnly) {
    TablePainter(box_fragment_)
        .PaintCollapsedBorders(paint_info, paint_offset, VisualRect(paint_offset));
}
```

## Key Code Patterns

### Iterating Table Children

```cpp
for (const PhysicalFragmentLink& child : fragment_.Children()) {
    if (!child->IsTableSection()) {
        continue;
    }
    // Process section...
}
```

### Creating Painters

```cpp
TableSectionPainter(To<PhysicalBoxFragment>(*child.fragment))
    .PaintBoxDecorationBackground(paint_info, paint_rect, box_decoration_data);
```

### Background Painting into Cells

```cpp
TableCellPainter(To<PhysicalBoxFragment>(child_fragment))
    .PaintBackgroundForTablePart(
        paint_info, table_part,
        table_part_paint_rect,
        row_paint_offset + child.offset);
```

### Collapsed Border Painting

```cpp
for (auto edge = TableCollapsedEdge(*collapsed_borders, start_edge_index);
     edge.Exists(); ++edge) {
    if (!edge.CanPaint())
        continue;

    // Compute edge rect...
    ComputeEdgeJoints(*collapsed_borders, edge, ...);

    // Adjust rect based on joint winners...

    BoxBorderPainter::DrawBoxSide(
        paint_info.context, ToPixelSnappedRect(physical_border_rect),
        box_side, edge.BorderColor(), edge.BorderStyle(), auto_dark_mode);
}
```

## Hit Testing Notes

Table rows and sections do not participate in hit testing:

```cpp
bool BoxFragmentPainter::ShouldRecordHitTestData(const PaintInfo& paint_info) {
    return !GetPhysicalFragment().IsTableRow() &&
           !GetPhysicalFragment().IsTableSection();
}
```

## Related CSS Properties

- `border-collapse: collapse | separate`
- `border-spacing`
- `empty-cells: show | hide`
- `caption-side: top | bottom`
- `table-layout: auto | fixed`

## Debugging

### DCHECK Assertions

- `TablePainter`: `DCHECK(fragment_.IsTable())`
- `TableSectionPainter`: `DCHECK(fragment_.IsTableSection())`
- `TableRowPainter`: `DCHECK(fragment_.IsTableRow())`

### TableBorders Debug Methods

```cpp
#if DCHECK_IS_ON()
String DumpEdges();
void ShowEdges();
bool operator==(const TableBorders& other) const;
#endif
```

## Common Issues

1. **Double painting at joints**: Handled by `ComputeEdgeJoints()` winner determination
2. **Background covering borders**: Handled by `TableCellBackgroundClipper`
3. **Fragmentation edge cases**: Special handling in `PaintCollapsedBorders()` for fragmented rows
4. **Column index out of bounds**: Protected by `NOTREACHED()` check in border painting loop
