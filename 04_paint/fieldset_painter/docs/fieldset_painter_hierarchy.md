# FieldsetPainter Class Hierarchy

## Overview

The `FieldsetPainter` is a specialized painter in the Chromium Blink rendering engine responsible for painting HTML `<fieldset>` elements. It handles the unique visual requirement of fieldsets: painting a border around the element while leaving a "cutout" gap where the `<legend>` element is positioned.

## Class Definition

```cpp
class FieldsetPainter {
  STACK_ALLOCATED();

 public:
  explicit FieldsetPainter(const PhysicalBoxFragment& fieldset);

  void PaintBoxDecorationBackground(const PaintInfo&,
                                    const PhysicalRect&,
                                    const BoxDecorationData&);
  void PaintMask(const PaintInfo&, const PhysicalOffset&);

 private:
  FieldsetPaintInfo CreateFieldsetPaintInfo() const;

  const PhysicalBoxFragment& fieldset_;
};
```

## Hierarchy and Dependencies

```
                    +-----------------------+
                    |   PhysicalBoxFragment |
                    |      (fieldset_)      |
                    +-----------+-----------+
                                |
                                | holds reference to
                                v
                    +-----------------------+
                    |    FieldsetPainter    |
                    +-----------+-----------+
                                |
            +-------------------+-------------------+
            |                                       |
            v                                       v
+------------------------+            +------------------------+
| FieldsetPaintInfo      |            | BoxFragmentPainter     |
| (helper struct)        |            | (delegate painter)     |
+------------------------+            +------------------------+
| - border_outsets       |            | - PaintNormalBoxShadow |
| - legend_cutout_rect   |            | - PaintFillLayers      |
+------------------------+            | - PaintInsetBoxShadow  |
                                      | - PaintBorder          |
                                      | - PaintMaskImages      |
                                      +------------------------+
```

## Related Classes

### FieldsetPaintInfo (Struct)

A helper struct that calculates the painting adjustments needed for fieldsets:

```cpp
struct FieldsetPaintInfo {
  STACK_ALLOCATED();

 public:
  FieldsetPaintInfo(const ComputedStyle& fieldset_style,
                    const PhysicalSize& fieldset_size,
                    const PhysicalBoxStrut& fieldset_borders,
                    const PhysicalRect& legend_border_box);

  // Block-start border outset caused by the rendered legend
  PhysicalBoxStrut border_outsets;

  // The cutout rectangle where border should not be painted
  PhysicalRect legend_cutout_rect;
};
```

### BoxFragmentPainter

The `FieldsetPainter` delegates most actual painting operations to `BoxFragmentPainter`:

- `PaintNormalBoxShadow()` - Outer box shadow
- `PaintFillLayers()` - Background painting
- `PaintInsetBoxShadowWithBorderRect()` - Inner box shadow
- `PaintBorder()` - Border painting (with legend cutout applied)
- `PaintMaskImages()` - Mask image painting

### Key Dependencies

| Class/Type | Purpose |
|------------|---------|
| `PhysicalBoxFragment` | The layout fragment representing the fieldset |
| `ComputedStyle` | Style information for the fieldset |
| `PaintInfo` | Paint context and phase information |
| `GraphicsContext` | Low-level drawing API |
| `BoxDecorationData` | Decoration painting decisions |
| `WritingModeConverter` | Handles writing mode conversions |

## Memory Management

- `FieldsetPainter` is `STACK_ALLOCATED()` - created on the stack, not heap
- `FieldsetPaintInfo` is also `STACK_ALLOCATED()`
- Both are lightweight objects created per-paint operation

## File Locations

| File | Description |
|------|-------------|
| `fieldset_painter.h` | Class declaration |
| `fieldset_painter.cc` | Implementation |
| `fieldset_paint_info.h` | Helper struct declaration |
| `fieldset_paint_info.cc` | Helper struct implementation |

## Chromium Source Path

```
third_party/blink/renderer/core/paint/fieldset_painter.h
third_party/blink/renderer/core/paint/fieldset_painter.cc
third_party/blink/renderer/core/paint/fieldset_paint_info.h
third_party/blink/renderer/core/paint/fieldset_paint_info.cc
```
