# MathMLPainter Class Hierarchy

## Overview

`MathMLPainter` is a specialized painter class in Chromium's Blink rendering engine responsible for painting MathML-specific graphical elements. It handles the visual rendering of mathematical notation including fractions, radicals (square roots), and operators.

## Class Definition

```cpp
class MathMLPainter {
  STACK_ALLOCATED();

 public:
  explicit MathMLPainter(const PhysicalBoxFragment& box_fragment)
      : box_fragment_(box_fragment) {}
  void Paint(const PaintInfo&, PhysicalOffset);

 private:
  void PaintBar(const PaintInfo&, const PhysicalRect&);
  void PaintFractionBar(const PaintInfo&, PhysicalOffset);
  void PaintOperator(const PaintInfo&, PhysicalOffset);
  void PaintRadicalSymbol(const PaintInfo&, PhysicalOffset);
  void PaintStretchyOrLargeOperator(const PaintInfo&, PhysicalOffset);

  const PhysicalBoxFragment& box_fragment_;
};
```

## Class Characteristics

| Attribute | Value |
|-----------|-------|
| Namespace | `blink` |
| Allocation | `STACK_ALLOCATED()` (short-lived, stack-based) |
| Header | `third_party/blink/renderer/core/paint/mathml_painter.h` |
| Implementation | `third_party/blink/renderer/core/paint/mathml_painter.cc` |

## Inheritance and Relationships

```
                    +-----------------------+
                    |   BoxFragmentPainter  |
                    |   (Caller/Owner)      |
                    +-----------+-----------+
                                |
                                | Creates and invokes
                                v
                    +-----------------------+
                    |     MathMLPainter     |
                    |   (Stack Allocated)   |
                    +-----------+-----------+
                                |
                                | References
                                v
                    +-----------------------+
                    |  PhysicalBoxFragment  |
                    |    (box_fragment_)    |
                    +-----------+-----------+
                                |
                                | Contains
                                v
                    +-----------------------+
                    |    MathMLPaintInfo    |
                    | (Operator parameters) |
                    +-----------------------+
```

## Member Variable

| Member | Type | Description |
|--------|------|-------------|
| `box_fragment_` | `const PhysicalBoxFragment&` | Reference to the physical box fragment containing MathML layout information |

## Public Interface

### Constructor

```cpp
explicit MathMLPainter(const PhysicalBoxFragment& box_fragment)
```

- Takes a reference to a `PhysicalBoxFragment` containing the MathML element to paint
- Stack-allocated for performance (no heap allocation)

### Paint Method

```cpp
void Paint(const PaintInfo& info, PhysicalOffset paint_offset)
```

- Main entry point for painting MathML elements
- Dispatches to specific painting methods based on element type

## Private Methods

| Method | Purpose |
|--------|---------|
| `PaintBar()` | Paints horizontal bars (used by fractions and radicals) |
| `PaintFractionBar()` | Paints the fraction line between numerator and denominator |
| `PaintOperator()` | Paints stretched or large mathematical operators |
| `PaintRadicalSymbol()` | Paints the radical (square root) symbol and overbar |
| `PaintStretchyOrLargeOperator()` | Renders operator glyphs using font shaping |

## Integration with Paint System

### Invocation from BoxFragmentPainter

```cpp
// In box_fragment_painter.cc (line 826-828)
if (is_visible && fragment.HasExtraMathMLPainting()) {
  MathMLPainter(fragment).Paint(paint_info, paint_offset);
}
```

### Condition for MathML Painting

The `HasExtraMathMLPainting()` method in `PhysicalBoxFragment` returns true when:
1. The fragment is a MathML fraction (`IsMathMLFraction()`), OR
2. The fragment has associated `MathMLPaintInfo` data

## Key Dependencies

| Dependency | Purpose |
|------------|---------|
| `PhysicalBoxFragment` | Contains layout geometry and MathML-specific data |
| `MathMLPaintInfo` | Stores operator character, shape results, and sizing |
| `PaintInfo` | Provides graphics context and paint phase information |
| `DrawingRecorder` | Handles display list recording and caching |
| `GraphicsContextStateSaver` | RAII wrapper for graphics state management |

## MathML Element Types Handled

1. **Fractions** (`<mfrac>`)
   - Detected via `IsMathMLFraction()`
   - Painted by `PaintFractionBar()`

2. **Radicals** (`<msqrt>`, `<mroot>`)
   - Detected via `GetMathMLPaintInfo().IsRadicalOperator()`
   - Painted by `PaintRadicalSymbol()`

3. **Operators** (`<mo>`)
   - Default case for elements with MathML paint info
   - Painted by `PaintOperator()`

## File Location

```
chromium/src/third_party/blink/renderer/core/paint/
    mathml_painter.h      // Class declaration
    mathml_painter.cc     // Implementation
```
