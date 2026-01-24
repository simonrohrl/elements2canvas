# ThemePainter Trace Reference

## Overview

This document provides a reference for tracing theme painting operations in Chromium's Blink renderer. Theme painting is responsible for rendering platform-native form controls.

## Key Files

| File | Location | Purpose |
|------|----------|---------|
| `theme_painter.h` | `core/paint/` | Base class interface |
| `theme_painter.cc` | `core/paint/` | Shared painting logic |
| `theme_painter_default.h` | `core/paint/` | Default implementation header |
| `theme_painter_default.cc` | `core/paint/` | Default implementation (Aura theme) |

## Form Control Types

### Native vs Custom Appearance

Controls can have either:
- **Native Appearance**: Rendered by `WebThemeEngine` (platform-specific)
- **Custom Appearance**: Rendered by CSS (returns `true` from paint methods)

### Control Classification

| Control | HTML Element | Appearance Value | Painting |
|---------|--------------|------------------|----------|
| Checkbox | `<input type="checkbox">` | `kCheckbox` | Native |
| Radio | `<input type="radio">` | `kRadio` | Native |
| Button | `<button>`, `<input type="button/submit/reset">` | `kButton`, `kPushButton`, `kSquareButton` | Native |
| TextField | `<input type="text">` | `kTextField` | Native* |
| TextArea | `<textarea>` | `kTextArea` | Native* |
| Select | `<select>` | `kMenulist` | Native |
| Range | `<input type="range">` | `kSliderHorizontal/Vertical` | Native |
| Progress | `<progress>` | `kProgressBar` | Native |
| Search | `<input type="search">` | `kSearchField` | Native* |
| Spin Button | Number input arrows | `kInnerSpinButton` | Native |
| Meter | `<meter>` | `kMeter` | CSS |
| Listbox | Multi-select | `kListbox` | CSS |
| Media Controls | Video/audio sliders | `kMediaSlider*` | CSS |

*Note: TextField, TextArea, and SearchField fall back to CSS painting if `HasBorderRadius()` or `HasBackgroundImage()` is true.

## Graphics Context Operations

### Direct Drawing Operations

Located in `theme_painter.cc` and `theme_painter_default.cc`:

#### 1. FillRect (Slider Ticks)

```cpp
// theme_painter.cc:415
paint_info.context.FillRect(
    tick_rect,                                                    // gfx::RectF
    o.ResolveColor(GetCSSPropertyColor()),                       // Color
    PaintAutoDarkMode(style, DarkModeFilter::ElementRole::kBackground)
);
```

**Purpose**: Draws tick marks on slider controls that have associated datalists.

#### 2. DrawImage (Search Cancel Button)

```cpp
// theme_painter_default.cc:901
paint_info.context.DrawImage(
    target_image,                    // Image reference
    Image::kSyncDecode,             // Decode mode
    ImageAutoDarkMode::Disabled(),  // Auto dark mode
    ImagePaintTimingInfo(),         // Timing info
    gfx::RectF(painting_rect)       // Destination rect
);
```

**Purpose**: Draws the search field cancel/clear button icon.

### Transform Operations (DirectionFlippingScope)

```cpp
// Horizontal flip (RTL)
paint_info_.context.Save();
paint_info_.context.Translate(2 * rect.x() + rect.width(), 0);
paint_info_.context.Scale(-1, 1);

// Vertical flip
paint_info_.context.Save();
paint_info_.context.Translate(0, 2 * rect.y() + rect.height());
paint_info_.context.Scale(1, -1);

// Restore
paint_info_.context.Restore();
```

**Purpose**: Flips coordinate system for RTL languages and vertical writing modes.

### Zoom Transform Operations

```cpp
// theme_painter_default.cc:917-919
paint_info.context.Translate(unzoomed_rect.x(), unzoomed_rect.y());
paint_info.context.Scale(zoom_level, zoom_level);
paint_info.context.Translate(-unzoomed_rect.x(), -unzoomed_rect.y());
```

**Purpose**: Applies zoom transformation for high-DPI rendering of checkboxes and radio buttons.

## WebThemeEngine Paint Calls

All native control painting delegates to the WebThemeEngine:

```cpp
WebThemeEngineHelper::GetNativeThemeEngine()->Paint(
    paint_info.context.Canvas(),  // cc::PaintCanvas
    part,                         // WebThemeEngine::Part
    state,                        // WebThemeEngine::State
    rect,                         // gfx::Rect
    &extra_params,               // Control-specific parameters
    forced_colors_mode,          // bool
    color_scheme,                // mojom::ColorScheme
    preferred_contrast,          // mojom::PreferredContrast
    color_provider,              // ui::ColorProvider*
    accent_color                 // optional<SkColor>
);
```

### Parts and Their Locations

| Part | Line in theme_painter_default.cc |
|------|----------------------------------|
| `kPartCheckbox` | 360 |
| `kPartRadio` | 403 |
| `kPartButton` | 426 |
| `kPartTextField` | 464 |
| `kPartMenuList` | 507, 532 |
| `kPartSliderTrack` | 666 |
| `kPartSliderThumb` | 716 |
| `kPartInnerSpinButton` | 753 |
| `kPartProgressBar` | 795 |

## ExtraParams Structures

### ButtonExtraParams

```cpp
struct ButtonExtraParams {
    bool checked;        // Checkbox/radio checked state
    bool indeterminate;  // Checkbox indeterminate state
    bool has_border;     // Button has border
    float zoom;          // Zoom level
};
```

### TextFieldExtraParams

```cpp
struct TextFieldExtraParams {
    bool is_text_area;         // Is textarea vs input
    bool is_listbox;           // Is listbox
    bool has_border;           // Has visible border
    float zoom;                // Zoom level
    SkColor background_color;  // Background color
    bool auto_complete_active; // Autofill active
};
```

### MenuListExtraParams

```cpp
struct MenuListExtraParams {
    bool has_border;           // Has visible border
    bool has_border_radius;    // Has rounded corners
    float zoom;                // Zoom level
    SkColor background_color;  // Background color
    bool fill_content_area;    // Fill with background
    ArrowDirection arrow_direction;  // Down/Left/Right
    int arrow_x, arrow_y;      // Arrow position
    float arrow_size;          // Arrow size
    SkColor arrow_color;       // Arrow color
};
```

### SliderExtraParams

```cpp
struct SliderExtraParams {
    bool vertical;        // Vertical orientation
    bool in_drag;         // Currently being dragged
    float zoom;           // Zoom level
    int thumb_x, thumb_y; // Thumb position (track only)
    bool right_to_left;   // RTL direction
};
```

### ProgressBarExtraParams

```cpp
struct ProgressBarExtraParams {
    bool determinate;            // Has known value
    int value_rect_x, value_rect_y;      // Value rect position
    int value_rect_width, value_rect_height;  // Value rect size
    float zoom;                  // Zoom level
    bool is_horizontal;          // Horizontal orientation
};
```

### InnerSpinButtonExtraParams

```cpp
struct InnerSpinButtonExtraParams {
    bool spin_up;                    // Up button active
    bool read_only;                  // Input is read-only
    SpinArrowsDirection spin_arrows_direction;  // Up/Down or Left/Right
};
```

## Platform Theming

### Color Scheme Handling

The system supports both light and dark color schemes:

```cpp
mojom::blink::ColorScheme color_scheme = style.UsedColorScheme();
const ui::ColorProvider* color_provider =
    document.GetColorProviderForPainting(color_scheme);
```

### Accent Color

CSS `accent-color` property affects checkboxes, radios, sliders, and progress bars:

```cpp
std::optional<SkColor> accent_color = GetAccentColor(style, document);
```

When accent color is set, the color scheme may be automatically adjusted to ensure contrast:

```cpp
color_scheme = GetColorSchemeForAccentColor(
    element, color_scheme, accent_color, part);
```

### Forced Colors Mode

High contrast accessibility mode:

```cpp
document.InForcedColorsMode()  // Check if in forced colors
document.GetPreferredContrast()  // Get contrast preference
```

## State Determination

Control states are determined from element state:

```cpp
WebThemeEngine::State GetWebThemeState(const Element& element) {
    if (element.IsDisabledFormControl())
        return WebThemeEngine::kStateDisabled;
    if (element.IsActive())
        return WebThemeEngine::kStatePressed;
    if (element.IsHovered())
        return WebThemeEngine::kStateHover;
    return WebThemeEngine::kStateNormal;
}
```

## Tracing Tips

### Identifying Theme Paint Operations

1. Look for `ThemePainter::Paint()` in call stacks
2. Check `style.EffectiveAppearance()` to see which control type
3. Watch for `WebThemeEngine::Paint()` calls for native rendering

### Return Value Semantics

- `return false`: Theme painting handled everything
- `return true`: CSS background/border should also paint

### Key Breakpoint Locations

| Purpose | File | Function |
|---------|------|----------|
| All theme painting | theme_painter.cc | `ThemePainter::Paint()` |
| Checkbox | theme_painter_default.cc | `ThemePainterDefault::PaintCheckbox()` |
| Radio | theme_painter_default.cc | `ThemePainterDefault::PaintRadio()` |
| Button | theme_painter_default.cc | `ThemePainterDefault::PaintButton()` |
| Slider | theme_painter_default.cc | `ThemePainterDefault::PaintSliderTrack/Thumb()` |
| Progress | theme_painter_default.cc | `ThemePainterDefault::PaintProgressBar()` |
| Native render | web_theme_engine.h | `WebThemeEngine::Paint()` |

## Temporal Controls (Date/Time)

Multiple-fields temporal inputs (date, time, datetime-local, month, week) use `kTextField` appearance but are counted separately for metrics:

```cpp
bool IsMultipleFieldsTemporalInput(FormControlType type) {
#if !BUILDFLAG(IS_ANDROID)
  return type == FormControlType::kInputDate ||
         type == FormControlType::kInputDatetimeLocal ||
         type == FormControlType::kInputMonth ||
         type == FormControlType::kInputTime ||
         type == FormControlType::kInputWeek;
#else
  return false;
#endif
}
```

On Android, these controls use native pickers instead.

## UseCounter Metrics

Theme painting tracks usage of appearance values for deprecation monitoring:

```cpp
#define COUNT_APPEARANCE(doc, feature) \
  doc.CountUse(WebFeature::kCSSValueAppearance##feature##Rendered)
```

Tracked appearances include:
- Checkbox, Radio, PushButton, SquareButton
- TextField, TextArea, SearchField, SearchCancel
- MenuList, MenuListButton
- ProgressBar
- SliderHorizontal, SliderVertical
- SliderThumbHorizontal, SliderThumbVertical
- InnerSpinButton
- MediaSlider, MediaSliderThumb, MediaVolumeSlider, MediaVolumeSliderThumb
