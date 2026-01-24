# MathMLPainter Trace Reference

## Source File Analysis

**File**: `third_party/blink/renderer/core/paint/mathml_painter.cc`
**Lines**: 191 total
**Copyright**: 2020 The Chromium Authors

---

## Line-by-Line Trace of Paint() Method

### Paint() Entry Point (Lines 164-188)

```cpp
164: void MathMLPainter::Paint(const PaintInfo& info, PhysicalOffset paint_offset) {
```
**Entry point** - Called by `BoxFragmentPainter::PaintInternal()` when `HasExtraMathMLPainting()` is true.

| Parameter | Type | Description |
|-----------|------|-------------|
| `info` | `const PaintInfo&` | Contains graphics context, paint phase, and painting state |
| `paint_offset` | `PhysicalOffset` | Accumulated offset for positioning |

```cpp
165:   const DisplayItemClient& display_item_client =
166:       *box_fragment_.GetLayoutObject();
```
**Lines 165-166**: Obtain the display item client from the layout object. This client is used for display list caching.

```cpp
167:   if (DrawingRecorder::UseCachedDrawingIfPossible(
168:           info.context, display_item_client, info.phase))
169:     return;
```
**Lines 167-169**: **Cache check** - If a cached drawing exists for this client and paint phase, return early without re-painting. This is a critical performance optimization.

```cpp
170:   DrawingRecorder recorder(
171:       info.context, display_item_client, info.phase,
172:       BoxFragmentPainter(box_fragment_).VisualRect(paint_offset));
```
**Lines 170-172**: Create a `DrawingRecorder` RAII object that:
- Begins recording drawing operations
- Associates them with the display item client
- Stores the visual rect for invalidation purposes

```cpp
174:   // Fraction
175:   if (box_fragment_.IsMathMLFraction()) {
176:     PaintFractionBar(info, paint_offset);
177:     return;
178:   }
```
**Lines 174-178**: **Fraction dispatch** - Check if this is a `<mfrac>` element. Paint the fraction bar and return.

```cpp
180:   // Radical symbol
181:   if (box_fragment_.GetMathMLPaintInfo().IsRadicalOperator()) {
182:     PaintRadicalSymbol(info, paint_offset);
183:     return;
184:   }
```
**Lines 180-184**: **Radical dispatch** - Check if this is a radical operator (`<msqrt>` or `<mroot>`). Paint the radical symbol and return.

```cpp
186:   // Operator
187:   PaintOperator(info, paint_offset);
188: }
```
**Lines 186-188**: **Default operator dispatch** - For all other MathML elements with paint info (stretched/large operators).

---

## PaintBar() Method (Lines 19-32)

**Purpose**: Paints a horizontal bar used for fraction lines and radical overbars.

```cpp
19: void MathMLPainter::PaintBar(const PaintInfo& info,
20:                              const PhysicalRect& bar_rect) {
21:   gfx::Rect snapped_bar_rect = ToPixelSnappedRect(bar_rect);
```
**Line 21**: Convert to pixel-snapped rectangle for crisp rendering.

```cpp
22:   if (snapped_bar_rect.IsEmpty()) {
23:     return;
24:   }
```
**Lines 22-24**: Early exit if the bar has zero dimensions.

```cpp
25:   // The (vertical) origin of `snapped_bar_rect` is now at the mid-point of the
26:   // bar. Shift up by half the height to produce the corresponding rectangle.
27:   snapped_bar_rect -= gfx::Vector2d(0, snapped_bar_rect.height() / 2);
```
**Lines 25-27**: Adjust vertical position. The input rectangle's origin is at the vertical midpoint of the bar; shift up by half the height to get the actual rectangle.

```cpp
28:   const ComputedStyle& style = box_fragment_.Style();
29:   info.context.FillRect(
30:       snapped_bar_rect, style.VisitedDependentColor(GetCSSPropertyColor()),
31:       PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kForeground));
32: }
```
**Lines 28-32**: **GraphicsContext draw operation** - Fill the rectangle with the computed foreground color, applying dark mode transformation if active.

### Graphics Operation: `FillRect()`

| Parameter | Value | Description |
|-----------|-------|-------------|
| Rectangle | `snapped_bar_rect` | Pixel-aligned bar bounds |
| Color | `style.VisitedDependentColor(GetCSSPropertyColor())` | Text color (respects `:visited`) |
| Dark Mode | `PaintAutoDarkMode(style, kForeground)` | Auto dark mode for foreground elements |

---

## PaintStretchyOrLargeOperator() Method (Lines 34-49)

**Purpose**: Renders stretched or large mathematical operator glyphs.

```cpp
34: void MathMLPainter::PaintStretchyOrLargeOperator(const PaintInfo& info,
35:                                                  PhysicalOffset paint_offset) {
36:   const ComputedStyle& style = box_fragment_.Style();
37:   const MathMLPaintInfo& parameters = box_fragment_.GetMathMLPaintInfo();
```
**Lines 36-37**: Obtain computed style and MathML-specific paint parameters.

```cpp
38:   UChar operator_character = parameters.operator_character;
39:   TextFragmentPaintInfo text_fragment_paint_info = {
40:       StringView(base::span_from_ref(operator_character)), 0, 1,
41:       parameters.operator_shape_result_view.Get()};
```
**Lines 38-41**: Prepare text fragment info:
- `operator_character`: Unicode character for the operator
- `StringView`: Single-character string
- Range: `0` to `1` (one character)
- `operator_shape_result_view`: Pre-computed glyph shaping results

```cpp
42:   GraphicsContextStateSaver state_saver(info.context);
```
**Line 42**: Save graphics context state (will be restored on scope exit).

```cpp
43:   info.context.SetFillColor(style.VisitedDependentColor(GetCSSPropertyColor()));
```
**Line 43**: **GraphicsContext operation** - Set fill color for text rendering.

```cpp
44:   AutoDarkMode auto_dark_mode(
45:       PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kForeground));
```
**Lines 44-45**: Configure dark mode handling for foreground elements.

```cpp
46:   info.context.DrawText(*style.GetFont(), text_fragment_paint_info,
47:                         gfx::PointF(paint_offset), kInvalidDOMNodeId,
48:                         auto_dark_mode);
49: }
```
**Lines 46-49**: **GraphicsContext draw operation** - Draw the operator character.

### Graphics Operation: `SetFillColor()`

| Parameter | Value |
|-----------|-------|
| Color | `style.VisitedDependentColor(GetCSSPropertyColor())` |

### Graphics Operation: `DrawText()`

| Parameter | Value | Description |
|-----------|-------|-------------|
| Font | `*style.GetFont()` | Font from computed style |
| Text Info | `text_fragment_paint_info` | Character + shaping results |
| Position | `gfx::PointF(paint_offset)` | Baseline position |
| Node ID | `kInvalidDOMNodeId` | Not associated with specific DOM node |
| Dark Mode | `auto_dark_mode` | Dark mode configuration |

---

## PaintFractionBar() Method (Lines 51-70)

**Purpose**: Paints the horizontal line separating numerator and denominator in fractions.

```cpp
51: void MathMLPainter::PaintFractionBar(const PaintInfo& info,
52:                                      PhysicalOffset paint_offset) {
53:   DCHECK(box_fragment_.Style().IsHorizontalWritingMode());
```
**Line 53**: Assert horizontal writing mode (MathML fractions only support horizontal).

```cpp
54:   const ComputedStyle& style = box_fragment_.Style();
55:   LayoutUnit line_thickness = FractionLineThickness(style);
56:   if (!line_thickness)
57:     return;
```
**Lines 54-57**: Get line thickness from style. Return early if zero (e.g., `linethickness="0"`).

```cpp
58:   LayoutUnit axis_height = MathAxisHeight(style);
59:   if (auto baseline = box_fragment_.FirstBaseline()) {
```
**Lines 58-59**: Get math axis height and fragment baseline for positioning.

```cpp
60:     auto borders = box_fragment_.Borders();
61:     auto padding = box_fragment_.Padding();
62:     PhysicalRect bar_rect = {
63:         borders.left + padding.left, *baseline - axis_height,
64:         box_fragment_.Size().width - borders.HorizontalSum() -
65:             padding.HorizontalSum(),
66:         line_thickness};
67:     bar_rect.Move(paint_offset);
68:     PaintBar(info, bar_rect);
69:   }
70: }
```
**Lines 60-69**: Calculate bar rectangle:
- **X**: Start after left border and padding
- **Y**: Baseline minus axis height (places bar on math axis)
- **Width**: Full content width (excluding borders and padding)
- **Height**: Line thickness from style

---

## PaintOperator() Method (Lines 72-100)

**Purpose**: Paints stretched or large operators with proper positioning.

```cpp
72: void MathMLPainter::PaintOperator(const PaintInfo& info,
73:                                   PhysicalOffset paint_offset) {
74:   const ComputedStyle& style = box_fragment_.Style();
75:   const MathMLPaintInfo& parameters = box_fragment_.GetMathMLPaintInfo();
76:   LogicalOffset offset(LayoutUnit(), parameters.operator_ascent);
```
**Lines 74-76**: Initialize with operator ascent as block offset (for baseline alignment).

```cpp
77:   PhysicalOffset physical_offset = offset.ConvertToPhysical(
78:       style.GetWritingDirection(),
79:       PhysicalSize(box_fragment_.Size().width, box_fragment_.Size().height),
80:       PhysicalSize(parameters.operator_inline_size,
81:                    parameters.operator_ascent + parameters.operator_descent));
```
**Lines 77-81**: Convert logical offset to physical coordinates considering:
- Writing direction
- Fragment size
- Operator dimensions

```cpp
82:   auto borders = box_fragment_.Borders();
83:   auto padding = box_fragment_.Padding();
84:   physical_offset.left += borders.left + padding.left;
85:   physical_offset.top += borders.top + padding.top;
```
**Lines 82-85**: Add border and padding offsets.

```cpp
87:   // TODO(http://crbug.com/1124301): MathOperatorLayoutAlgorithm::Layout
88:   // passes the operator's inline size but this does not match the width of the
89:   // box fragment, which relies on the min-max sizes instead. Shift the paint
90:   // offset to work around that issue, splitting the size error symmetrically.
91:   DCHECK(box_fragment_.Style().IsHorizontalWritingMode());
92:   bool is_ltr = style.GetWritingDirection().IsLtr() ||
93:                 !RuntimeEnabledFeatures::MathMLOperatorRTLMirroringEnabled();
94:   physical_offset.left +=
95:       (box_fragment_.Size().width - borders.HorizontalSum() -
96:        padding.HorizontalSum() - parameters.operator_inline_size) /
97:       2 * (is_ltr ? 1 : -1);
```
**Lines 87-97**: Center the operator horizontally within the box. Handles LTR/RTL direction.

```cpp
99:   PaintStretchyOrLargeOperator(info, paint_offset + physical_offset);
100: }
```
**Line 99**: Delegate to `PaintStretchyOrLargeOperator()` with computed offset.

---

## PaintRadicalSymbol() Method (Lines 102-162)

**Purpose**: Paints radical symbols (square root, nth root) including the overbar.

### Phase 1: Get Base Child Metrics (Lines 104-112)

```cpp
104:   LayoutUnit base_child_width;
105:   LayoutUnit base_child_ascent;
106:   if (!box_fragment_.Children().empty()) {
107:     const auto& base_child =
108:         To<PhysicalBoxFragment>(*box_fragment_.Children()[0]);
109:     base_child_width = base_child.Size().width;
110:     base_child_ascent =
111:         base_child.FirstBaseline().value_or(base_child.Size().height);
112:   }
```
Get dimensions of the base (expression under the radical).

### Phase 2: Calculate Vertical Symbol Position (Lines 114-142)

```cpp
119:   bool has_index =
120:       To<MathMLRadicalElement>(box_fragment_.GetNode())->HasIndex();
121:   auto vertical = GetRadicalVerticalParameters(style, has_index);
```
Determine if this is `<mroot>` (has index) vs `<msqrt>` (no index) and get vertical spacing parameters.

```cpp
123:   auto radical_base_ascent =
124:       base_child_ascent + parameters.radical_base_margins.inline_start;
125:   LayoutUnit block_offset =
126:       box_fragment_.FirstBaseline().value_or(box_fragment_.Size().height) -
127:       vertical.vertical_gap - radical_base_ascent;
```
Calculate block offset for radical symbol positioning.

### Phase 3: Paint Vertical Radical Symbol (Lines 134-144)

```cpp
134:   LogicalOffset radical_symbol_offset(
135:       inline_offset, block_offset + parameters.operator_ascent);
136:   auto radical_symbol_physical_offset = radical_symbol_offset.ConvertToPhysical(
137:       style.GetWritingDirection(),
138:       PhysicalSize(box_fragment_.Size().width, box_fragment_.Size().height),
139:       PhysicalSize(RuntimeEnabledFeatures::MathMLOperatorRTLMirroringEnabled()
140:                        ? parameters.operator_inline_size
141:                        : parameters.operator_ascent,
142:                    parameters.operator_ascent + parameters.operator_descent));
143:   PaintStretchyOrLargeOperator(info,
144:                                paint_offset + radical_symbol_physical_offset);
```
Paint the vertical part of the radical symbol.

### Phase 4: Paint Horizontal Overbar (Lines 146-161)

```cpp
147:   LayoutUnit rule_thickness = vertical.rule_thickness;
148:   if (!rule_thickness)
149:     return;
```
Skip overbar if rule thickness is zero.

```cpp
150:   LayoutUnit base_width =
151:       base_child_width + parameters.radical_base_margins.InlineSum();
152:   LogicalOffset bar_offset =
153:       LogicalOffset(inline_offset, block_offset) +
154:       LogicalSize(parameters.operator_inline_size, LayoutUnit());
```
Calculate overbar position (starts at end of radical symbol).

```cpp
158:   PhysicalRect bar_rect = {bar_physical_offset.left, bar_physical_offset.top,
159:                            base_width, rule_thickness};
160:   bar_rect.Move(paint_offset);
161:   PaintBar(info, bar_rect);
```
Paint the horizontal overbar using `PaintBar()`.

---

## Summary of GraphicsContext Operations

| Method | Operation | Purpose |
|--------|-----------|---------|
| `PaintBar()` | `FillRect()` | Draw fraction/radical bars |
| `PaintStretchyOrLargeOperator()` | `SetFillColor()` | Set text color |
| `PaintStretchyOrLargeOperator()` | `DrawText()` | Render operator glyphs |

## MathML Element Types and Paint Paths

| Element | Detection | Paint Method | Graphics Operations |
|---------|-----------|--------------|---------------------|
| `<mfrac>` | `IsMathMLFraction()` | `PaintFractionBar()` -> `PaintBar()` | `FillRect()` |
| `<msqrt>` | `IsRadicalOperator()` | `PaintRadicalSymbol()` | `DrawText()` + `FillRect()` |
| `<mroot>` | `IsRadicalOperator()` | `PaintRadicalSymbol()` | `DrawText()` + `FillRect()` |
| `<mo>` (stretched) | Default | `PaintOperator()` | `DrawText()` |

## Key Data Structures

### MathMLPaintInfo

```cpp
struct MathMLPaintInfo {
  UChar operator_character;                           // Unicode operator
  Member<const ShapeResultView> operator_shape_result_view;  // Glyph shaping
  LayoutUnit operator_inline_size;                    // Horizontal extent
  LayoutUnit operator_ascent;                         // Above baseline
  LayoutUnit operator_descent;                        // Below baseline
  BoxStrut radical_base_margins;                      // Spacing for radicals
  std::optional<LayoutUnit> radical_operator_inline_offset;  // Radical-specific
};
```

### IsRadicalOperator Detection

```cpp
bool IsRadicalOperator() const {
  return radical_operator_inline_offset.has_value();
}
```
Radicals are identified by having a non-null `radical_operator_inline_offset`.
